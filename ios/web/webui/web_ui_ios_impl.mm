// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/webui/web_ui_ios_impl.h"

#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "ios/public/provider/web/web_ui_ios_controller.h"
#include "ios/public/provider/web/web_ui_ios_controller_factory.h"
#include "ios/public/provider/web/web_ui_ios_message_handler.h"
#include "ios/web/web_state/web_state_impl.h"

using web::WebUIIOSController;

namespace web {

// static
base::string16 WebUIIOS::GetJavascriptCall(
    const std::string& function_name,
    const std::vector<const base::Value*>& arg_list) {
  base::string16 parameters;
  std::string json;
  for (size_t i = 0; i < arg_list.size(); ++i) {
    if (i > 0)
      parameters += base::char16(',');

    base::JSONWriter::Write(*arg_list[i], &json);
    parameters += base::UTF8ToUTF16(json);
  }
  return base::ASCIIToUTF16(function_name) + base::char16('(') + parameters +
         base::char16(')') + base::char16(';');
}

WebUIIOSImpl::WebUIIOSImpl(WebStateImpl* web_state) : web_state_(web_state) {
  DCHECK(web_state);
}

WebUIIOSImpl::~WebUIIOSImpl() {
  controller_.reset();
}

// WebUIIOSImpl, public:
// ----------------------------------------------------------

WebState* WebUIIOSImpl::GetWebState() const {
  return web_state_;
}

WebUIIOSController* WebUIIOSImpl::GetController() const {
  return controller_.get();
}

void WebUIIOSImpl::SetController(WebUIIOSController* controller) {
  controller_.reset(controller);
}

void WebUIIOSImpl::CallJavascriptFunction(const std::string& function_name) {
  DCHECK(base::IsStringASCII(function_name));
  base::string16 javascript = base::ASCIIToUTF16(function_name + "();");
  ExecuteJavascript(javascript);
}

void WebUIIOSImpl::CallJavascriptFunction(const std::string& function_name,
                                          const base::Value& arg) {
  DCHECK(base::IsStringASCII(function_name));
  std::vector<const base::Value*> args;
  args.push_back(&arg);
  ExecuteJavascript(GetJavascriptCall(function_name, args));
}

void WebUIIOSImpl::CallJavascriptFunction(const std::string& function_name,
                                          const base::Value& arg1,
                                          const base::Value& arg2) {
  DCHECK(base::IsStringASCII(function_name));
  std::vector<const base::Value*> args;
  args.push_back(&arg1);
  args.push_back(&arg2);
  ExecuteJavascript(GetJavascriptCall(function_name, args));
}

void WebUIIOSImpl::CallJavascriptFunction(const std::string& function_name,
                                          const base::Value& arg1,
                                          const base::Value& arg2,
                                          const base::Value& arg3) {
  DCHECK(base::IsStringASCII(function_name));
  std::vector<const base::Value*> args;
  args.push_back(&arg1);
  args.push_back(&arg2);
  args.push_back(&arg3);
  ExecuteJavascript(GetJavascriptCall(function_name, args));
}

void WebUIIOSImpl::CallJavascriptFunction(const std::string& function_name,
                                          const base::Value& arg1,
                                          const base::Value& arg2,
                                          const base::Value& arg3,
                                          const base::Value& arg4) {
  DCHECK(base::IsStringASCII(function_name));
  std::vector<const base::Value*> args;
  args.push_back(&arg1);
  args.push_back(&arg2);
  args.push_back(&arg3);
  args.push_back(&arg4);
  ExecuteJavascript(GetJavascriptCall(function_name, args));
}

void WebUIIOSImpl::CallJavascriptFunction(
    const std::string& function_name,
    const std::vector<const base::Value*>& args) {
  DCHECK(base::IsStringASCII(function_name));
  ExecuteJavascript(GetJavascriptCall(function_name, args));
}

void WebUIIOSImpl::RegisterMessageCallback(const std::string& message,
                                           const MessageCallback& callback) {
  message_callbacks_.insert(std::make_pair(message, callback));
}

void WebUIIOSImpl::ProcessWebUIIOSMessage(const GURL& source_url,
                                          const std::string& message,
                                          const base::ListValue& args) {
  if (controller_->OverrideHandleWebUIIOSMessage(source_url, message, args))
    return;

  // Look up the callback for this message.
  MessageCallbackMap::const_iterator callback =
      message_callbacks_.find(message);
  if (callback != message_callbacks_.end()) {
    // Forward this message and content on.
    callback->second.Run(&args);
  }
}

// WebUIIOSImpl, protected:
// -------------------------------------------------------

void WebUIIOSImpl::AddMessageHandler(WebUIIOSMessageHandler* handler) {
  DCHECK(!handler->web_ui());
  handler->set_web_ui(this);
  handler->RegisterMessages();
  handlers_.push_back(handler);
}

void WebUIIOSImpl::ExecuteJavascript(const base::string16& javascript) {
  web_state_->ExecuteJavaScriptAsync(javascript);
}

}  // namespace web
