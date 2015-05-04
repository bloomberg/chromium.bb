// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/web_controller_observer_bridge.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/web/public/web_state/crw_web_controller_observer.h"
#include "ios/web/public/web_state/web_state.h"

namespace web {

WebControllerObserverBridge::WebControllerObserverBridge(
    id<CRWWebControllerObserver> web_controller_observer,
    WebState* web_state,
    CRWWebController* web_controller)
    : WebStateObserver(web_state),
      web_controller_observer_(web_controller_observer),
      web_controller_(web_controller) {
  DCHECK(web_controller_observer_);
  DCHECK(web_controller_);

  // Listen for script commands if needed.
  if ([web_controller_observer_ respondsToSelector:@selector(commandPrefix)]) {
    NSString* prefix = [web_controller_observer_ commandPrefix];
    DCHECK_GT([prefix length], 0u);
    script_command_callback_prefix_ = base::SysNSStringToUTF8(prefix);
    web_state->AddScriptCommandCallback(
        base::Bind(&WebControllerObserverBridge::ScriptCommandReceived,
                   base::Unretained(this)),
        script_command_callback_prefix_);
  }
}

WebControllerObserverBridge::~WebControllerObserverBridge() {
  if (!script_command_callback_prefix_.empty())
    web_state()->RemoveScriptCommandCallback(script_command_callback_prefix_);
}

void WebControllerObserverBridge::PageLoaded(
    PageLoadCompletionStatus load_completion_status) {
  if (load_completion_status == PageLoadCompletionStatus::SUCCESS &&
      [web_controller_observer_ respondsToSelector:@selector(pageLoaded:)])
    [web_controller_observer_ pageLoaded:web_controller_];
}

bool WebControllerObserverBridge::ScriptCommandReceived(
    const base::DictionaryValue& value,
    const GURL& url,
    bool user_is_interacting) {
  DCHECK(!script_command_callback_prefix_.empty());
  return [web_controller_observer_ handleCommand:value
                                   webController:web_controller_
                               userIsInteracting:user_is_interacting
                                       originURL:url];
}

}  // namespace web
