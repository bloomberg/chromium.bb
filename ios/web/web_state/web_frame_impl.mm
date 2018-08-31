// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/web_state/web_frame_impl.h"

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "crypto/aead.h"
#include "crypto/random.h"
#import "ios/web/public/web_state/web_state.h"
#include "ios/web/public/web_task_traits.h"

#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kJavaScriptReplyCommandPrefix[] = "frameMessaging_";
}

namespace web {

WebFrameImpl::WebFrameImpl(const std::string& frame_id,
                           std::unique_ptr<crypto::SymmetricKey> frame_key,
                           int initial_message_id,
                           bool is_main_frame,
                           GURL security_origin,
                           web::WebState* web_state)
    : frame_id_(frame_id),
      frame_key_(std::move(frame_key)),
      next_message_id_(initial_message_id),
      is_main_frame_(is_main_frame),
      security_origin_(security_origin),
      web_state_(web_state) {
  DCHECK(frame_key_);
  DCHECK(web_state);
  web_state->AddObserver(this);

  const std::string command_prefix = kJavaScriptReplyCommandPrefix + frame_id;
  web_state->AddScriptCommandCallback(
      base::BindRepeating(&WebFrameImpl::OnJavaScriptReply,
                          base::Unretained(this), base::Unretained(web_state)),
      command_prefix);
}

WebFrameImpl::~WebFrameImpl() {
  CancelPendingRequests();
  DetachFromWebState();
}

void WebFrameImpl::DetachFromWebState() {
  if (web_state_) {
    const std::string command_prefix =
        kJavaScriptReplyCommandPrefix + frame_id_;
    web_state_->RemoveScriptCommandCallback(command_prefix);
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

WebState* WebFrameImpl::GetWebState() {
  return web_state_;
}

std::string WebFrameImpl::GetFrameId() const {
  return frame_id_;
}

bool WebFrameImpl::IsMainFrame() const {
  return is_main_frame_;
}

GURL WebFrameImpl::GetSecurityOrigin() const {
  return security_origin_;
}

bool WebFrameImpl::CallJavaScriptFunction(
    const std::string& name,
    const std::vector<base::Value>& parameters,
    bool reply_with_result) {
  int message_id = next_message_id_;
  next_message_id_++;

  base::DictionaryValue message;
  message.SetKey("messageId", base::Value(message_id));
  message.SetKey("replyWithResult", base::Value(reply_with_result));
  message.SetKey("functionName", base::Value(name));
  base::ListValue parameters_value(parameters);
  message.SetKey("parameters", std::move(parameters_value));

  std::string json;
  base::JSONWriter::Write(message, &json);

  crypto::Aead aead(crypto::Aead::AES_256_GCM);
  aead.Init(&frame_key_->key());

  std::string iv;
  crypto::RandBytes(base::WriteInto(&iv, aead.NonceLength() + 1),
                    aead.NonceLength());

  std::string ciphertext;
  if (!aead.Seal(json, iv, /*additional_data=*/nullptr, &ciphertext)) {
    LOG(ERROR) << "Error sealing message for WebFrame.";
    return false;
  }

  std::string encoded_iv;
  base::Base64Encode(iv, &encoded_iv);
  std::string encoded_message;
  base::Base64Encode(ciphertext, &encoded_message);
  std::string script = base::StringPrintf(
      "__gCrWeb.frameMessaging.routeMessage('%s', '%s', '%s')",
      encoded_message.c_str(), encoded_iv.c_str(), frame_id_.c_str());
  GetWebState()->ExecuteJavaScript(base::UTF8ToUTF16(script));

  return true;
}

bool WebFrameImpl::CallJavaScriptFunction(
    const std::string& name,
    const std::vector<base::Value>& parameters) {
  return CallJavaScriptFunction(name, parameters, /*replyWithResult=*/false);
}

bool WebFrameImpl::CallJavaScriptFunction(
    std::string name,
    const std::vector<base::Value>& parameters,
    base::OnceCallback<void(const base::Value*)> callback,
    base::TimeDelta timeout) {
  int message_id = next_message_id_;

  auto timeout_callback = std::make_unique<TimeoutCallback>(base::BindOnce(
      &WebFrameImpl::CancelRequest, base::Unretained(this), message_id));
  TimeoutCallback* timeout_callback_ptr = timeout_callback.get();
  auto callbacks = std::make_unique<struct RequestCallbacks>(
      std::move(callback), std::move(timeout_callback));
  pending_requests_[message_id] = std::move(callbacks);

  base::PostDelayedTaskWithTraits(FROM_HERE, {web::WebThread::UI},
                                  timeout_callback_ptr->callback(), timeout);
  return CallJavaScriptFunction(name, parameters, /*replyWithResult=*/true);
}

void WebFrameImpl::CancelRequest(int message_id) {
  auto request = pending_requests_.find(message_id);
  if (request == pending_requests_.end()) {
    return;
  }
  CancelRequestWithCallbacks(std::move(request->second));
  pending_requests_.erase(request);
}

void WebFrameImpl::CancelPendingRequests() {
  for (auto& it : pending_requests_) {
    CancelRequestWithCallbacks(std::move(it.second));
  }
  pending_requests_.clear();
}

void WebFrameImpl::CancelRequestWithCallbacks(
    std::unique_ptr<RequestCallbacks> request_callbacks) {
  request_callbacks->timeout_callback->Cancel();
  std::move(request_callbacks->completion).Run(nullptr);
}

bool WebFrameImpl::OnJavaScriptReply(web::WebState* web_state,
                                     const base::DictionaryValue& command_json,
                                     const GURL& page_url,
                                     bool interacting,
                                     bool is_main_frame) {
  auto* command = command_json.FindKey("command");
  if (!command || !command->is_string() || !command_json.HasKey("messageId")) {
    NOTREACHED();
    return false;
  }

  const std::string command_string = command->GetString();
  if (command_string !=
      (std::string(kJavaScriptReplyCommandPrefix) + frame_id_ + ".reply")) {
    NOTREACHED();
    return false;
  }

  auto* message_id_value = command_json.FindKey("messageId");
  if (!message_id_value->is_double()) {
    NOTREACHED();
    return false;
  }

  int message_id = static_cast<int>(message_id_value->GetDouble());

  auto request = pending_requests_.find(message_id);
  if (request == pending_requests_.end()) {
    NOTREACHED();
    return false;
  }

  auto callbacks = std::move(request->second);
  pending_requests_.erase(request);
  callbacks->timeout_callback->Cancel();
  const base::Value* result = command_json.FindKey("result");
  std::move(callbacks->completion).Run(result);

  return true;
}

void WebFrameImpl::WebStateDestroyed(web::WebState* web_state) {
  CancelPendingRequests();
  DetachFromWebState();
}

WebFrameImpl::RequestCallbacks::RequestCallbacks(
    base::OnceCallback<void(const base::Value*)> completion,
    std::unique_ptr<TimeoutCallback> timeout)
    : completion(std::move(completion)), timeout_callback(std::move(timeout)) {}

WebFrameImpl::RequestCallbacks::~RequestCallbacks(){};

}  // namespace web
