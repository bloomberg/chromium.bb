// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/java_script_console/java_script_console_tab_helper.h"

#import <Foundation/Foundation.h>

#include "base/values.h"
#include "ios/chrome/browser/web/java_script_console/java_script_console_message.h"
#include "ios/web/public/web_state/web_frame.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Name of message to which javascript console messages are sent.
static const char* kCommandPrefix = "console";
}

JavaScriptConsoleTabHelper::JavaScriptConsoleTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  web_state->AddObserver(this);
  web_state->AddScriptCommandCallback(
      base::BindRepeating(
          &JavaScriptConsoleTabHelper::OnJavaScriptConsoleMessage,
          base::Unretained(this)),
      kCommandPrefix);
}

bool JavaScriptConsoleTabHelper::OnJavaScriptConsoleMessage(
    const base::DictionaryValue& message,
    const GURL& page_url,
    bool has_user_gesture,
    bool main_frame,
    web::WebFrame* sender_frame) {
  // Completely skip processing the message if no delegate exists.
  if (!delegate_) {
    return true;
  }

  const base::Value* log_message = message.FindKey("message");
  if (!log_message) {
    return false;
  }
  const base::Value* log_level_value = message.FindKey("method");
  if (!log_level_value || !log_level_value->is_string()) {
    return false;
  }
  const base::Value* url_value = message.FindKey("url");
  if (!url_value || !url_value->is_string()) {
    return false;
  }

  JavaScriptConsoleMessage frame_message;
  frame_message.level = log_level_value->GetString();
  frame_message.url = GURL(url_value->GetString());
  frame_message.message = base::Value::ToUniquePtrValue(log_message->Clone());
  delegate_->DidReceiveConsoleMessage(web_state_, sender_frame, frame_message);
  return true;
}

void JavaScriptConsoleTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveScriptCommandCallback(kCommandPrefix);
  web_state->RemoveObserver(this);
  web_state_ = nullptr;
}

void JavaScriptConsoleTabHelper::SetDelegate(
    JavaScriptConsoleTabHelperDelegate* delegate) {
  delegate_ = delegate;
}

JavaScriptConsoleTabHelper::~JavaScriptConsoleTabHelper() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_->RemoveScriptCommandCallback(kCommandPrefix);
    web_state_ = nullptr;
  }
}
