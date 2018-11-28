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

// Standard User Defaults key for "Log JS" debug setting.
NSString* const kLogJavaScript = @"LogJavascript";
}

JavaScriptConsoleTabHelper::JavaScriptConsoleTabHelper(
    web::WebState* web_state) {
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
  const base::Value* log_message = message.FindKey("message");
  const base::Value* log_level_value = message.FindKey("method");
  const base::Value* origin_value = message.FindKey("origin");
  if (!log_message || !log_level_value || !log_level_value->is_string() ||
      !origin_value || !origin_value->is_string()) {
    return false;
  }
  std::string log_level = log_level_value->GetString();
  std::string origin = origin_value->GetString();

  if ([[NSUserDefaults standardUserDefaults] boolForKey:kLogJavaScript]) {
    DVLOG(0) << origin << " [" << log_level << "] " << log_message;
  }

  if (!delegate_) {
    return true;
  }

  JavaScriptConsoleMessage frame_message;
  frame_message.level = log_level;
  frame_message.origin = GURL(origin);
  frame_message.message = base::Value::ToUniquePtrValue(log_message->Clone());
  delegate_->DidReceiveConsoleMessage(frame_message);
  return true;
}

void JavaScriptConsoleTabHelper::WebStateDestroyed(web::WebState* web_state) {
  web_state->RemoveScriptCommandCallback(kCommandPrefix);
  web_state->RemoveObserver(this);
}

void JavaScriptConsoleTabHelper::SetDelegate(
    JavaScriptConsoleTabHelperDelegate* delegate) {
  delegate_ = delegate;
}

JavaScriptConsoleTabHelper::~JavaScriptConsoleTabHelper() = default;
