// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/js_modal_dialog.h"

#import <Cocoa/Cocoa.h>

#include "app/l10n_util_mac.h"
#include "app/message_box_flags.h"
#import "base/cocoa_protocols_mac.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/chrome_browser_application_mac.h"
#include "grit/app_strings.h"
#include "grit/generated_resources.h"

// Helper object that receives the notification that the dialog/sheet is
// going away. Is responsible for cleaning itself up.
@interface JavaScriptAppModalDialogHelper : NSObject<NSAlertDelegate> {
 @private
  NSAlert* alert_;
  NSTextField* textField_;  // WEAK; owned by alert_
}

- (NSAlert*)alert;
- (NSTextField*)textField;
- (void)alertDidEnd:(NSAlert *)alert
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo;

@end

@implementation JavaScriptAppModalDialogHelper

- (NSAlert*)alert {
  alert_ = [[NSAlert alloc] init];
  return alert_;
}

- (NSTextField*)textField {
  textField_ = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 300, 22)];
  [alert_ setAccessoryView:textField_];
  [textField_ release];

  return textField_;
}

- (void)dealloc {
  [alert_ release];
  [super dealloc];
}

// |contextInfo| is the bridge back to the C++ JavaScriptAppModalDialog. When
// complete, autorelease to clean ourselves up.
- (void)alertDidEnd:(NSAlert*)alert
         returnCode:(int)returnCode
        contextInfo:(void*)contextInfo {
  JavaScriptAppModalDialog* bridge =
      reinterpret_cast<JavaScriptAppModalDialog*>(contextInfo);
  std::wstring input;
  if (textField_)
    input = base::SysNSStringToWide([textField_ stringValue]);
  switch (returnCode) {
    case NSAlertFirstButtonReturn:  {  // OK
      bool shouldSuppress = false;
      if ([alert showsSuppressionButton])
        shouldSuppress = [[alert suppressionButton] state] == NSOnState;
      bridge->OnAccept(input, shouldSuppress);
      break;
    }
    case NSAlertSecondButtonReturn:  {  // Cancel
      // If the user wants to stay on this page, stop quitting (if a quit is in
      // progress).
      if (bridge->is_before_unload_dialog())
        chrome_browser_application_mac::CancelTerminate();

      bridge->OnCancel();
      break;
    }
    case NSRunStoppedResponse: {  // Window was closed underneath us
      // Need to call OnCancel() because there is some cleanup that needs
      // to be done.  It won't call back to the javascript since the
      // JavaScriptAppModalDialog knows that the TabContents was destroyed.
      bridge->OnCancel();
      break;
    }
    default:  {
      NOTREACHED();
    }
  }
  [self autorelease];
  delete bridge;  // Done with the dialog, it needs be destroyed.
}
@end

void JavaScriptAppModalDialog::CreateAndShowDialog() {
  // Determine the names of the dialog buttons based on the flags. "Default"
  // is the OK button. "Other" is the cancel button. We don't use the
  // "Alternate" button in NSRunAlertPanel.
  NSString* default_button = l10n_util::GetNSStringWithFixup(IDS_APP_OK);
  NSString* other_button = l10n_util::GetNSStringWithFixup(IDS_APP_CANCEL);
  bool text_field = false;
  bool one_button = false;
  switch (dialog_flags_) {
    case MessageBoxFlags::kIsJavascriptAlert:
      one_button = true;
      break;
    case MessageBoxFlags::kIsJavascriptConfirm:
      if (is_before_unload_dialog_) {
        default_button = l10n_util::GetNSStringWithFixup(
            IDS_BEFOREUNLOAD_MESSAGEBOX_OK_BUTTON_LABEL);
        other_button = l10n_util::GetNSStringWithFixup(
            IDS_BEFOREUNLOAD_MESSAGEBOX_CANCEL_BUTTON_LABEL);
      }
      break;
    case MessageBoxFlags::kIsJavascriptPrompt:
      text_field = true;
      break;

    default:
      NOTREACHED();
  }

  // Create a helper which will receive the sheet ended selector. It will
  // delete itself when done. It doesn't need anything passed to its init
  // as it will get a contextInfo parameter.
  JavaScriptAppModalDialogHelper* helper =
      [[JavaScriptAppModalDialogHelper alloc] init];

  // Show the modal dialog.
  NSAlert* alert = [helper alert];
  dialog_ = alert;
  NSTextField* field = nil;
  if (text_field) {
    field = [helper textField];
    [field setStringValue:base::SysWideToNSString(default_prompt_text_)];
  }
  [alert setDelegate:helper];
  [alert setInformativeText:base::SysWideToNSString(message_text_)];
  [alert setMessageText:base::SysWideToNSString(title_)];
  [alert addButtonWithTitle:default_button];
  if (!one_button) {
    NSButton* other = [alert addButtonWithTitle:other_button];
    [other setKeyEquivalent:@"\e"];
  }
  if (display_suppress_checkbox_) {
    [alert setShowsSuppressionButton:YES];
    NSString* suppression_title = l10n_util::GetNSStringWithFixup(
        IDS_JAVASCRIPT_MESSAGEBOX_SUPPRESS_OPTION);
    [[alert suppressionButton] setTitle:suppression_title];
  }

  [alert beginSheetModalForWindow:nil  // nil here makes it app-modal
                    modalDelegate:helper
                   didEndSelector:@selector(alertDidEnd:returnCode:contextInfo:)
                      contextInfo:this];

  if (field)
    [[alert window] makeFirstResponder:field];
}

// The functions below are used by the automation framework.
int JavaScriptAppModalDialog::GetDialogButtons() {
  // From the above, it is the case that if there is 1 button, it is always the
  // OK button.  The second button, if it exists, is always the Cancel button.
  int num_buttons = [[dialog_ buttons] count];
  switch (num_buttons) {
    case 1:
      return MessageBoxFlags::DIALOGBUTTON_OK;
    case 2:
      return MessageBoxFlags::DIALOGBUTTON_OK |
             MessageBoxFlags::DIALOGBUTTON_CANCEL;
    default:
      NOTREACHED();
      return 0;
  }
}

// On Mac, this is only used in testing.
void JavaScriptAppModalDialog::AcceptWindow() {
  NSButton* first = [[dialog_ buttons] objectAtIndex:0];
  [first performClick:nil];
}

void JavaScriptAppModalDialog::CancelWindow() {
  DCHECK([[dialog_ buttons] count] >= 2);
  NSButton* second = [[dialog_ buttons] objectAtIndex:1];
  [second performClick:nil];
}

// This is only used by the app-modal dialog machinery on windows.
NativeDialog JavaScriptAppModalDialog::CreateNativeDialog() {
  NOTIMPLEMENTED();
  return nil;
}

void JavaScriptAppModalDialog::CloseModalDialog() {
  NSAlert* alert = dialog_;
  DCHECK([alert isKindOfClass:[NSAlert class]]);
  [NSApp endSheet:[alert window]];
  dialog_ = nil;
}
