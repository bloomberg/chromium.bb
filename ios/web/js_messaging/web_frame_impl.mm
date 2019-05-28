// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/js_messaging/web_frame_impl.h"

#import <Foundation/Foundation.h>

#include "base/base64.h"
#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
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
                           bool is_main_frame,
                           GURL security_origin,
                           web::WebState* web_state)
    : frame_id_(frame_id),
      is_main_frame_(is_main_frame),
      security_origin_(security_origin),
      web_state_(web_state),
      weak_ptr_factory_(this) {
  DCHECK(web_state);
  web_state->AddObserver(this);

  web_state->AddScriptCommandCallback(
      base::BindRepeating(&WebFrameImpl::OnJavaScriptReply,
                          base::Unretained(this), base::Unretained(web_state)),
      GetScriptCommandPrefix());
}

WebFrameImpl::~WebFrameImpl() {
  CancelPendingRequests();
  DetachFromWebState();
}

void WebFrameImpl::SetEncryptionKey(
    std::unique_ptr<crypto::SymmetricKey> frame_key) {
  frame_key_ = std::move(frame_key);
}

void WebFrameImpl::SetNextMessageId(int message_id) {
  next_message_id_ = message_id;
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

bool WebFrameImpl::CanCallJavaScriptFunction() const {
  // JavaScript can always be called on the main frame without encryption
  // because calling the function directly on the webstate with
  // |ExecuteJavaScript| is secure. However, iframes require an encryption key
  // in order to securely pass the function name and parameters to the frame.
  return is_main_frame_ || frame_key_;
}

bool WebFrameImpl::CallJavaScriptFunction(
    const std::string& name,
    const std::vector<base::Value>& parameters,
    bool reply_with_result) {
  if (!CanCallJavaScriptFunction()) {
    return false;
  }

  int message_id = next_message_id_;
  next_message_id_++;

  if (!frame_key_) {
    return ExecuteJavaScriptFunction(name, parameters, message_id,
                                     reply_with_result);
  }

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
      "__gCrWeb.message.routeMessage('%s', '%s', '%s')",
      encoded_message.c_str(), encoded_iv.c_str(), frame_id_.c_str());
  GetWebState()->ExecuteJavaScript(base::UTF8ToUTF16(script));

  return true;
}

bool WebFrameImpl::CallJavaScriptFunction(
    const std::string& name,
    const std::vector<base::Value>& parameters) {
  return CallJavaScriptFunction(name, parameters, /*reply_with_result=*/false);
}

bool WebFrameImpl::CallJavaScriptFunction(
    const std::string& name,
    const std::vector<base::Value>& parameters,
    base::OnceCallback<void(const base::Value*)> callback,
    base::TimeDelta timeout) {
  int message_id = next_message_id_;

  auto timeout_callback = std::make_unique<TimeoutCallback>(base::BindOnce(
      &WebFrameImpl::CancelRequest, base::Unretained(this), message_id));
  auto callbacks = std::make_unique<struct RequestCallbacks>(
      std::move(callback), std::move(timeout_callback));
  pending_requests_[message_id] = std::move(callbacks);

  base::PostDelayedTaskWithTraits(
      FROM_HERE, {web::WebThread::UI},
      pending_requests_[message_id]->timeout_callback->callback(), timeout);
  bool called =
      CallJavaScriptFunction(name, parameters, /*reply_with_result=*/true);
  if (!called) {
    // Remove callbacks if the call failed.
    auto request = pending_requests_.find(message_id);
    if (request != pending_requests_.end()) {
      pending_requests_.erase(request);
    }
  }
  return called;
}

bool WebFrameImpl::ExecuteJavaScriptFunction(
    const std::string& name,
    const std::vector<base::Value>& parameters,
    int message_id,
    bool reply_with_result) {
  if (!IsMainFrame()) {
    return false;
  }

  NSMutableArray* parameter_strings = [[NSMutableArray alloc] init];
  for (const auto& value : parameters) {
    std::string string_value;
    base::JSONWriter::Write(value, &string_value);
    [parameter_strings addObject:base::SysUTF8ToNSString(string_value)];
  }

  NSString* script = [NSString
      stringWithFormat:@"__gCrWeb.%s(%@)", name.c_str(),
                       [parameter_strings componentsJoinedByString:@","]];
  base::WeakPtr<WebFrameImpl> weak_frame = weak_ptr_factory_.GetWeakPtr();
  GetWebState()->ExecuteJavaScript(base::SysNSStringToUTF16(script),
                                   base::BindOnce(^(const base::Value* result) {
                                     if (weak_frame) {
                                       weak_frame->CompleteRequest(message_id,
                                                                   result);
                                     }
                                   }));

  return true;
}

void WebFrameImpl::CompleteRequest(int message_id, const base::Value* result) {
  auto request = pending_requests_.find(message_id);
  if (request == pending_requests_.end()) {
    return;
  }
  CompleteRequest(std::move(request->second), result);
  pending_requests_.erase(request);
}

void WebFrameImpl::CompleteRequest(
    std::unique_ptr<RequestCallbacks> request_callbacks,
    const base::Value* result) {
  request_callbacks->timeout_callback->Cancel();
  std::move(request_callbacks->completion).Run(result);
}

void WebFrameImpl::CancelRequest(int message_id) {
  CompleteRequest(message_id, /*result=*/nullptr);
}

void WebFrameImpl::CancelPendingRequests() {
  for (auto& it : pending_requests_) {
    CompleteRequest(std::move(it.second), /*result=*/nullptr);
  }
  pending_requests_.clear();
}

bool WebFrameImpl::OnJavaScriptReply(web::WebState* web_state,
                                     const base::DictionaryValue& command_json,
                                     const GURL& page_url,
                                     bool interacting,
                                     bool is_main_frame,
                                     WebFrame* sender_frame) {
  auto* command = command_json.FindKey("command");
  if (!command || !command->is_string() || !command_json.HasKey("messageId")) {
    NOTREACHED();
    return false;
  }

  const std::string command_string = command->GetString();
  if (command_string != (GetScriptCommandPrefix() + ".reply")) {
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
    // Request may have already been processed due to timeout.
    return false;
  }

  auto callbacks = std::move(request->second);
  pending_requests_.erase(request);
  callbacks->timeout_callback->Cancel();
  const base::Value* result = command_json.FindKey("result");
  std::move(callbacks->completion).Run(result);

  return true;
}

void WebFrameImpl::DetachFromWebState() {
  if (web_state_) {
    web_state_->RemoveScriptCommandCallback(GetScriptCommandPrefix());
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

const std::string WebFrameImpl::GetScriptCommandPrefix() {
  return kJavaScriptReplyCommandPrefix + frame_id_;
}

void WebFrameImpl::WebStateDestroyed(web::WebState* web_state) {
  CancelPendingRequests();
  DetachFromWebState();
}

WebFrameImpl::RequestCallbacks::RequestCallbacks(
    base::OnceCallback<void(const base::Value*)> completion,
    std::unique_ptr<TimeoutCallback> timeout)
    : completion(std::move(completion)), timeout_callback(std::move(timeout)) {}

WebFrameImpl::RequestCallbacks::~RequestCallbacks() {}

}  // namespace web
