// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BROWSER_PASSWORD_GENERATION_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_BROWSER_PASSWORD_GENERATION_BUBBLE_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/base_bubble_controller.h"
#import "chrome/browser/ui/cocoa/styled_text_field.h"
#include "components/autofill/common/password_generation_util.h"
#include "content/public/common/password_form.h"

namespace autofill {
class PasswordGenerator;
}

namespace content {
class RenderViewHost;
}

class Browser;
class PasswordManager;
@class PasswordGenerationBubbleController;

// Customized text field that is used to display the regenerate icon.
@interface PasswordGenerationTextField : StyledTextField {
}

- (id)initWithFrame:(NSRect)frame
     withController:(PasswordGenerationBubbleController*)controller
        normalImage:(NSImage*)normalImage
         hoverImage:(NSImage*)hoverImage;
@end

@interface PasswordGenerationBubbleController :
    BaseBubbleController<NSTextFieldDelegate> {
 @private
  // |renderViewHost_| and |passwordManager_| may be nil in testing.
  content::RenderViewHost* renderViewHost_;
  PasswordManager* passwordManager_;
  autofill::PasswordGenerator* passwordGenerator_;
  content::PasswordForm form_;
  autofill::password_generation::PasswordGenerationActions actions_;

  PasswordGenerationTextField* textField_;   // weak
}

@property(readonly) PasswordGenerationTextField* textField;

- (id)initWithWindow:(NSWindow*)parentWindow
          anchoredAt:(NSPoint)point
      renderViewHost:(content::RenderViewHost*)renderViewHost
     passwordManager:(PasswordManager*)passwordManager
      usingGenerator:(autofill::PasswordGenerator*)passwordGenerator
             forForm:(const content::PasswordForm&)form;
- (void)performLayout;
- (IBAction)fillPassword:(id)sender;
- (void)regeneratePassword;
@end

// Testing interfaces

@interface PasswordGenerationTextField (ExposedForTesting)
- (void)simulateIconClick;
@end

#endif  // CHROME_BROWSER_UI_COCOA_BROWSER_PASSWORD_GENERATION_BUBBLE_CONTROLLER_H_
