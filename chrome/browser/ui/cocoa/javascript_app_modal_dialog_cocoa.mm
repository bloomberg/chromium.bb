// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/javascript_app_modal_dialog_cocoa.h"

#import <Cocoa/Cocoa.h>

#include "base/i18n/rtl.h"
#include "base/logging.h"
#import "base/mac/foundation_util.h"
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
  base::scoped_nsobject<NSAlert> alert_;
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
  NSString* informative_text =
      base::SysUTF16ToNSString(dialog_->message_text());
  [alert_ setInformativeText:informative_text];
  NSString* message_text =
      base::SysUTF16ToNSString(dialog_->title());
  [alert_ setMessageText:message_text];
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

  // Fix RTL dialogs.
  //
  // Mac OS X will always display NSAlert strings as LTR. A workaround is to
  // manually set the text as attributed strings in the implementing
  // NSTextFields. This is a basic correctness issue.
  //
  // In addition, for readability, the overall alignment is set based on the
  // directionality of the first strongly-directional character.
  //
  // If the dialog fields are selectable then they will scramble when clicked.
  // Therefore, selectability is disabled.
  //
  // See http://crbug.com/70806 for more details.

  bool message_has_rtl =
      base::i18n::StringContainsStrongRTLChars(dialog_->title());
  bool informative_has_rtl =
      base::i18n::StringContainsStrongRTLChars(dialog_->message_text());

  NSTextField* message_text_field = nil;
  NSTextField* informative_text_field = nil;
  if (message_has_rtl || informative_has_rtl) {
    // Force layout of the dialog. NSAlert leaves its dialog alone once laid
    // out; if this is not done then all the modifications that are to come will
    // be un-done when the dialog is finally displayed.
    [alert_ layout];

    // Locate the NSTextFields that implement the text display. These are
    // actually available as the ivars |_messageField| and |_informationField|
    // of the NSAlert, but it is safer (and more forward-compatible) to search
    // for them in the subviews.
    for (NSView* view in [[[alert_ window] contentView] subviews]) {
      NSTextField* text_field = base::mac::ObjCCast<NSTextField>(view);
      if ([[text_field stringValue] isEqualTo:message_text])
        message_text_field = text_field;
      else if ([[text_field stringValue] isEqualTo:informative_text])
        informative_text_field = text_field;
    }

    // This may fail in future OS releases, but it will still work for shipped
    // versions of Chromium.
    DCHECK(message_text_field);
    DCHECK(informative_text_field);
  }

  if (message_has_rtl && message_text_field) {
    base::scoped_nsobject<NSMutableParagraphStyle> alignment(
        [[NSParagraphStyle defaultParagraphStyle] mutableCopy]);
    [alignment setAlignment:NSRightTextAlignment];

    NSDictionary* alignment_attributes =
        @{ NSParagraphStyleAttributeName : alignment };
    base::scoped_nsobject<NSAttributedString> attr_string(
        [[NSAttributedString alloc] initWithString:message_text
                                        attributes:alignment_attributes]);

    [message_text_field setAttributedStringValue:attr_string];
    [message_text_field setSelectable:NO];
  }

  if (informative_has_rtl && informative_text_field) {
    base::i18n::TextDirection direction =
        base::i18n::GetFirstStrongCharacterDirection(dialog_->message_text());
    base::scoped_nsobject<NSMutableParagraphStyle> alignment(
        [[NSParagraphStyle defaultParagraphStyle] mutableCopy]);
    [alignment setAlignment:
        (direction == base::i18n::RIGHT_TO_LEFT) ? NSRightTextAlignment
                                                 : NSLeftTextAlignment];

    NSDictionary* alignment_attributes =
        @{ NSParagraphStyleAttributeName : alignment };
    base::scoped_nsobject<NSAttributedString> attr_string(
        [[NSAttributedString alloc] initWithString:informative_text
                                        attributes:alignment_attributes]);

    [informative_text_field setAttributedStringValue:attr_string];
    [informative_text_field setSelectable:NO];
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
