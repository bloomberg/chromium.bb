// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_JS_CRW_JS_EARLY_SCRIPT_MANAGER_H_
#define IOS_WEB_PUBLIC_WEB_STATE_JS_CRW_JS_EARLY_SCRIPT_MANAGER_H_

#import "ios/web/public/web_state/js/crw_js_injection_manager.h"

// TODO(eugenebut): remove this class from web's public interface.

// Manager to handle all the scripts that need to be injected before the page
// scripts take effect. Includes the base scripts and any feature scripts
// that might need to perform global actions such as overriding HTML methods.
@interface CRWJSEarlyScriptManager : CRWJSInjectionManager
@end

#endif  // IOS_WEB_PUBLIC_WEB_STATE_JS_CRW_JS_EARLY_SCRIPT_MANAGER_H_
