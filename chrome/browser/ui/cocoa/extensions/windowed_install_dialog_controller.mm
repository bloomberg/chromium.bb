// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/windowed_install_dialog_controller.h"

#import "base/mac/sdk_forward_declarations.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/extensions/extension_install_view_controller.h"
#include "ui/base/cocoa/window_size_constants.h"

@interface WindowedInstallController
    : NSWindowController<NSWindowDelegate> {
 @private
  base::scoped_nsobject<ExtensionInstallViewController> installViewController_;
  WindowedInstallDialogController* dialogController_;  // Weak. Owns us.
}

@property(readonly, nonatomic) ExtensionInstallViewController* viewController;

- (id)initWithNavigator:(content::PageNavigator*)navigator
               delegate:(WindowedInstallDialogController*)delegate
                 prompt:(scoped_refptr<ExtensionInstallPrompt::Prompt>)prompt;

@end

WindowedInstallDialogController::WindowedInstallDialogController(
    const ExtensionInstallPrompt::ShowParams& show_params,
    ExtensionInstallPrompt::Delegate* delegate,
    scoped_refptr<ExtensionInstallPrompt::Prompt> prompt)
    : delegate_(delegate) {
  install_controller_.reset([[WindowedInstallController alloc]
      initWithNavigator:show_params.navigator
               delegate:this
                 prompt:prompt]);
  [[install_controller_ window] makeKeyAndOrderFront:nil];
}

WindowedInstallDialogController::~WindowedInstallDialogController() {
  DCHECK(!install_controller_);
  DCHECK(!delegate_);
}

void WindowedInstallDialogController::OnWindowClosing() {
  install_controller_.reset();
  if (delegate_) {
    delegate_->InstallUIAbort(false);
    delegate_ = NULL;
  }
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

ExtensionInstallViewController*
WindowedInstallDialogController::GetViewController() {
  return [install_controller_ viewController];
}

void WindowedInstallDialogController::InstallUIProceed() {
  delegate_->InstallUIProceed();
  delegate_ = NULL;
  [[install_controller_ window] close];
}

void WindowedInstallDialogController::InstallUIAbort(bool user_initiated) {
  delegate_->InstallUIAbort(user_initiated);
  delegate_ = NULL;
  [[install_controller_ window] close];
}

@implementation WindowedInstallController

- (id)initWithNavigator:(content::PageNavigator*)navigator
               delegate:(WindowedInstallDialogController*)delegate
                 prompt:(scoped_refptr<ExtensionInstallPrompt::Prompt>)prompt {
  base::scoped_nsobject<NSWindow> controlledPanel(
      [[NSPanel alloc] initWithContentRect:ui::kWindowSizeDeterminedLater
                                 styleMask:NSTitledWindowMask
                                   backing:NSBackingStoreBuffered
                                     defer:NO]);
  if ((self = [super initWithWindow:controlledPanel])) {
    dialogController_ = delegate;
    installViewController_.reset([[ExtensionInstallViewController alloc]
        initWithNavigator:navigator
                 delegate:delegate
                   prompt:prompt]);
    NSWindow* window = [self window];

    // Ensure the window does not display behind the app launcher window, and is
    // otherwise hard to lose behind other windows (since it is not modal).
    [window setLevel:NSDockWindowLevel];

    // Animate the window when ordered in, the same way as an NSAlert.
    if ([window respondsToSelector:@selector(setAnimationBehavior:)])
      [window setAnimationBehavior:NSWindowAnimationBehaviorAlertPanel];

    [window setTitle:base::SysUTF16ToNSString(prompt->GetDialogTitle())];
    NSRect viewFrame = [[installViewController_ view] frame];
    [window setFrame:[window frameRectForContentRect:viewFrame]
             display:NO];
    [window setContentView:[installViewController_ view]];
    [window setDelegate:self];
    [window center];
  }
  return self;
}

- (ExtensionInstallViewController*)viewController {
  return installViewController_;
}

- (void)windowWillClose:(NSNotification*)notification {
  [[self window] setDelegate:nil];
  dialogController_->OnWindowClosing();
}

@end
