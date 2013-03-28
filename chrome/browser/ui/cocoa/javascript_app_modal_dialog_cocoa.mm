// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/javascript_app_modal_dialog_cocoa.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/chrome_browser_application_mac.h"
#include "chrome/browser/ui/app_modal_dialogs/javascript_app_modal_dialog.h"
#include "grit/generated_resources.h"
#include "grit/ui_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/ui_base_types.h"

// Helper object that receives the notification that the dialog/sheet is
// going away. Is responsible for cleaning itself up.
@interface JavaScriptAppModalDialogHelper : NSObject<NSAlertDelegate> {
 @private
  scoped_nsobject<NSAlert> alert_;
  NSTextField* textField_;  // WEAK; owned by alert_
}

- (NSAlert*)alert;
- (NSTextField*)textField;
- (void)alertDidEnd:(NSAlert*)alert
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo;

@end

@implementation JavaScriptAppModalDialogHelper

- (NSAlert*)alert {
  alert_.reset([[NSAlert alloc] init]);
  return alert_;
}

- (NSTextField*)textField {
  textField_ = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 300, 22)];
  [[textField_ cell] setLineBreakMode:NSLineBreakByTruncatingTail];
  [alert_ setAccessoryView:textField_];
  [textField_ release];

  return textField_;
}

// |contextInfo| is the JavaScriptAppModalDialogCocoa that owns us.
- (void)alertDidEnd:(NSAlert*)alert
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo {
  scoped_ptr<JavaScriptAppModalDialogCocoa> native_dialog(
      reinterpret_cast<JavaScriptAppModalDialogCocoa*>(contextInfo));
  string16 input;
  if (textField_)
    input = base::SysNSStringToUTF16([textField_ stringValue]);
  bool shouldSuppress = false;
  if ([alert showsSuppressionButton])
    shouldSuppress = [[alert suppressionButton] state] == NSOnState;
  switch (returnCode) {
    case NSAlertFirstButtonReturn:  {  // OK
      native_dialog->dialog()->OnAccept(input, shouldSuppress);
      break;
    }
    case NSAlertSecondButtonReturn:  {  // Cancel
      // If the user wants to stay on this page, stop quitting (if a quit is in
      // progress).
      if (native_dialog->dialog()->is_before_unload_dialog())
        chrome_browser_application_mac::CancelTerminate();
      native_dialog->dialog()->OnCancel(shouldSuppress);
      break;
    }
    case NSRunStoppedResponse: {  // Window was closed underneath us
      // Need to call OnCancel() because there is some cleanup that needs
      // to be done.  It won't call back to the javascript since the
      // JavaScriptAppModalDialog knows that the WebContents was destroyed.
      native_dialog->dialog()->OnCancel(shouldSuppress);
      break;
    }
    default:  {
      NOTREACHED();
    }
  }
}

@end

////////////////////////////////////////////////////////////////////////////////
// JavaScriptAppModalDialogCocoa, public:

JavaScriptAppModalDialogCocoa::JavaScriptAppModalDialogCocoa(
    JavaScriptAppModalDialog* dialog)
    : dialog_(dialog),
      helper_(NULL) {
  // Determine the names of the dialog buttons based on the flags. "Default"
  // is the OK button. "Other" is the cancel button. We don't use the
  // "Alternate" button in NSRunAlertPanel.
  NSString* default_button = l10n_util::GetNSStringWithFixup(IDS_APP_OK);
  NSString* other_button = l10n_util::GetNSStringWithFixup(IDS_APP_CANCEL);
  bool text_field = false;
  bool one_button = false;
  switch (dialog_->javascript_message_type()) {
    case content::JAVASCRIPT_MESSAGE_TYPE_ALERT:
      one_button = true;
      break;
    case content::JAVASCRIPT_MESSAGE_TYPE_CONFIRM:
      if (dialog_->is_before_unload_dialog()) {
        if (dialog_->is_reload()) {
          default_button = l10n_util::GetNSStringWithFixup(
              IDS_BEFORERELOAD_MESSAGEBOX_OK_BUTTON_LABEL);
          other_button = l10n_util::GetNSStringWithFixup(
              IDS_BEFORERELOAD_MESSAGEBOX_CANCEL_BUTTON_LABEL);
        } else {
          default_button = l10n_util::GetNSStringWithFixup(
              IDS_BEFOREUNLOAD_MESSAGEBOX_OK_BUTTON_LABEL);
          other_button = l10n_util::GetNSStringWithFixup(
              IDS_BEFOREUNLOAD_MESSAGEBOX_CANCEL_BUTTON_LABEL);
        }
      }
      break;
    case content::JAVASCRIPT_MESSAGE_TYPE_PROMPT:
      text_field = true;
      break;

    default:
      NOTREACHED();
  }

  // Create a helper which will receive the sheet ended selector. It will
  // delete itself when done. It doesn't need anything passed to its init
  // as it will get a contextInfo parameter.
  helper_.reset([[JavaScriptAppModalDialogHelper alloc] init]);

  // Show the modal dialog.
  alert_ = [helper_ alert];
  NSTextField* field = nil;
  if (text_field) {
    field = [helper_ textField];
    [field setStringValue:base::SysUTF16ToNSString(
        dialog_->default_prompt_text())];
  }
  [alert_ setDelegate:helper_];
  [alert_ setInformativeText:base::SysUTF16ToNSString(dialog_->message_text())];
  [alert_ setMessageText:base::SysUTF16ToNSString(dialog_->title())];
  [alert_ addButtonWithTitle:default_button];
  if (!one_button) {
    NSButton* other = [alert_ addButtonWithTitle:other_button];
    [other setKeyEquivalent:@"\e"];
  }
  if (dialog_->display_suppress_checkbox()) {
    [alert_ setShowsSuppressionButton:YES];
    NSString* suppression_title = l10n_util::GetNSStringWithFixup(
        IDS_JAVASCRIPT_MESSAGEBOX_SUPPRESS_OPTION);
    [[alert_ suppressionButton] setTitle:suppression_title];
  }
}

JavaScriptAppModalDialogCocoa::~JavaScriptAppModalDialogCocoa() {
}

////////////////////////////////////////////////////////////////////////////////
// JavaScriptAppModalDialogCocoa, NativeAppModalDialog implementation:

int JavaScriptAppModalDialogCocoa::GetAppModalDialogButtons() const {
  // From the above, it is the case that if there is 1 button, it is always the
  // OK button.  The second button, if it exists, is always the Cancel button.
  int num_buttons = [[alert_ buttons] count];
  switch (num_buttons) {
    case 1:
      return ui::DIALOG_BUTTON_OK;
    case 2:
      return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
    default:
      NOTREACHED();
      return 0;
  }
}

void JavaScriptAppModalDialogCocoa::ShowAppModalDialog() {
  [alert_
      beginSheetModalForWindow:nil  // nil here makes it app-modal
                 modalDelegate:helper_.get()
                didEndSelector:@selector(alertDidEnd:returnCode:contextInfo:)
                   contextInfo:this];

  if ([alert_ accessoryView])
    [[alert_ window] makeFirstResponder:[alert_ accessoryView]];
}

void JavaScriptAppModalDialogCocoa::ActivateAppModalDialog() {
}

void JavaScriptAppModalDialogCocoa::CloseAppModalDialog() {
  DCHECK([alert_ isKindOfClass:[NSAlert class]]);

  // Note: the call below will delete |this|,
  // see JavaScriptAppModalDialogHelper's alertDidEnd.
  [NSApp endSheet:[alert_ window]];
}

void JavaScriptAppModalDialogCocoa::AcceptAppModalDialog() {
  NSButton* first = [[alert_ buttons] objectAtIndex:0];
  [first performClick:nil];
}

void JavaScriptAppModalDialogCocoa::CancelAppModalDialog() {
  DCHECK([[alert_ buttons] count] >= 2);
  NSButton* second = [[alert_ buttons] objectAtIndex:1];
  [second performClick:nil];
}

////////////////////////////////////////////////////////////////////////////////
// NativeAppModalDialog, public:

// static
NativeAppModalDialog* NativeAppModalDialog::CreateNativeJavaScriptPrompt(
    JavaScriptAppModalDialog* dialog,
    gfx::NativeWindow parent_window) {
  return new JavaScriptAppModalDialogCocoa(dialog);
}
