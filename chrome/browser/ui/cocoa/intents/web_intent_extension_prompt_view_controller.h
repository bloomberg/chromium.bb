// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_EXTENSION_PROMPT_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_EXTENSION_PROMPT_VIEW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_view_controller.h"

@class ExtensionInstallViewController;
namespace content {
class PageNavigator;
}

// The message view shows an extension install prompt.
@interface WebIntentExtensionPromptViewController : WebIntentViewController {
 @private
  scoped_nsobject<ExtensionInstallViewController> viewController_;
}

- (id)init;

- (void)setNavigator:(content::PageNavigator*)navigator
            delegate:(ExtensionInstallPrompt::Delegate*)delegate
              prompt:(const ExtensionInstallPrompt::Prompt&)prompt;

- (ExtensionInstallViewController*)viewController;

@end

#endif  // CHROME_BROWSER_UI_COCOA_INTENTS_WEB_INTENT_EXTENSION_PROMPT_VIEW_CONTROLLER_H_
