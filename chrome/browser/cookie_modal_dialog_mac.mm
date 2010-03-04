// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cookie_modal_dialog.h"

#import <Cocoa/Cocoa.h>

#include "app/l10n_util_mac.h"
#import "base/cocoa_protocols_mac.h"
#include "base/scoped_nsobject.h"
#include "base/logging.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"

// Helper object that will become a real NSWindowController in the future.
@interface CookiePromptModalDialogHelper : NSObject<NSAlertDelegate> {
 @private
  scoped_nsobject<NSAlert> alert_;
  scoped_nsobject<NSMatrix> matrix_;
}

- (id)initWithBridge:(CookiePromptModalDialog*)bridge;
- (NSAlert*)alert;
- (void)alertDidEnd:(NSAlert*)alert
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo;
@end

@implementation CookiePromptModalDialogHelper

- (id)initWithBridge:(CookiePromptModalDialog*)bridge {
  // The cookie confirmation dialog needs both a radio group and a disclosure
  // triangle, so it's too complex to be shown as an NSAlert -- a custom window
  // is required. However, that requires small modifications to the parent class
  // AppModalDialog, so I'll do that in another CL.
  if ((self = [super init])) {
    alert_.reset([[NSAlert alloc] init]);

    string16 displayHost = UTF8ToUTF16(bridge->origin().host());
    int descriptionStringId =
        bridge->dialog_type() == CookiePromptModalDialog::DIALOG_TYPE_COOKIE ?
            IDS_COOKIE_ALERT_LABEL : IDS_DATA_ALERT_LABEL;
    NSString* description = l10n_util::GetNSStringF(
        descriptionStringId, displayHost);
    NSString* allow =
        l10n_util::GetNSStringWithFixup(IDS_COOKIE_ALERT_ALLOW_BUTTON);
    NSString* block =
        l10n_util::GetNSStringWithFixup(IDS_COOKIE_ALERT_BLOCK_BUTTON);

    NSString* remember = l10n_util::GetNSStringF(
        IDS_COOKIE_ALERT_REMEMBER_RADIO, displayHost);
    NSString* ask = l10n_util::GetNSStringWithFixup(IDS_COOKIE_ALERT_ASK_RADIO);

    scoped_nsobject<NSButtonCell> prototype([[NSButtonCell alloc] init]);
    [prototype.get() setButtonType:NSRadioButton];
    matrix_.reset(
        [[NSMatrix alloc] initWithFrame:NSZeroRect
                                   mode:NSRadioModeMatrix
                              prototype:prototype
                           numberOfRows:2
                        numberOfColumns:1]);
    NSArray *cellArray = [matrix_.get() cells];
    [[cellArray objectAtIndex:0] setTitle:remember];
    [[cellArray objectAtIndex:1] setTitle:ask];
    [matrix_.get() sizeToFit];
    [alert_.get() setAccessoryView:matrix_.get()];

    [alert_.get() setMessageText:description];
    [alert_.get() addButtonWithTitle:allow];
    [alert_.get() addButtonWithTitle:block];
  }
  return self;
}

- (NSAlert*)alert {
  return alert_.get();
}

// |contextInfo| is the bridge back to the C++ CookiePromptModalDialog.
- (void)alertDidEnd:(NSAlert*)alert
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo {
  CookiePromptModalDialog* bridge =
      reinterpret_cast<CookiePromptModalDialog*>(contextInfo);
  bool remember = [matrix_.get() selectedRow] == 0;
  switch (returnCode) {
    case NSAlertFirstButtonReturn: {  // OK
      bool sessionExpire = false;
      bridge->AllowSiteData(remember, sessionExpire);
      break;
    }
    case NSAlertSecondButtonReturn: {  // Cancel
      bridge->BlockSiteData(remember);
      break;
    }
    case NSRunStoppedResponse: {  // Window was closed underneath us
      bridge->BlockSiteData(remember);
      break;
    }
    default:  {
      NOTREACHED();
      remember = false;
      bridge->BlockSiteData(remember);
    }
  }
}
@end

void CookiePromptModalDialog::CreateAndShowDialog() {
  scoped_nsobject<CookiePromptModalDialogHelper> helper(
      [[CookiePromptModalDialogHelper alloc] initWithBridge:this]);
  NSAlert* alert = [helper alert];
  DCHECK(alert);

  NSInteger result = [alert runModal];
  [helper.get() alertDidEnd:alert returnCode:result contextInfo:this];

  // Other than JavaScriptAppModalDialog, the cross-platform part of this class
  // does not call |CompleteDialog()|, an explicit call is required.
  CompleteDialog();
  Cleanup();
  delete this;
}

// The functions below are used by the automation framework.

void CookiePromptModalDialog::AcceptWindow() {
  NOTIMPLEMENTED();
}

void CookiePromptModalDialog::CancelWindow() {
  NOTIMPLEMENTED();
}

// This is only used by the app-modal dialog machinery on windows.
NativeDialog CookiePromptModalDialog::CreateNativeDialog() {
  NOTIMPLEMENTED();
  return nil;
}
