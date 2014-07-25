// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_uninstall_dialog.h"

#import <Cocoa/Cocoa.h>

#include <string>

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/extension.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia_util_mac.h"

namespace {

// The Cocoa implementation of ExtensionUninstallDialog. This has a less
// complex life cycle than the Views and GTK implementations because the
// dialog blocks the page from navigating away and destroying the dialog,
// so there's no way for the dialog to outlive its delegate.
class ExtensionUninstallDialogCocoa
    : public extensions::ExtensionUninstallDialog {
 public:
  ExtensionUninstallDialogCocoa(Profile* profile,
                                gfx::NativeWindow parent,
                                Delegate* delegate);
  virtual ~ExtensionUninstallDialogCocoa() OVERRIDE;

 private:
  virtual void Show() OVERRIDE;
};

ExtensionUninstallDialogCocoa::ExtensionUninstallDialogCocoa(
    Profile* profile,
    gfx::NativeWindow parent,
    extensions::ExtensionUninstallDialog::Delegate* delegate)
    : extensions::ExtensionUninstallDialog(profile, parent, delegate) {
}

ExtensionUninstallDialogCocoa::~ExtensionUninstallDialogCocoa() {}

void ExtensionUninstallDialogCocoa::Show() {
  NSAlert* alert = [[[NSAlert alloc] init] autorelease];

  NSButton* continueButton = [alert addButtonWithTitle:l10n_util::GetNSString(
      IDS_EXTENSION_PROMPT_UNINSTALL_BUTTON)];
  NSButton* cancelButton = [alert addButtonWithTitle:l10n_util::GetNSString(
      IDS_CANCEL)];
  // Default to accept when triggered via chrome://extensions page.
  if (triggering_extension_) {
    [continueButton setKeyEquivalent:@""];
    [cancelButton setKeyEquivalent:@"\r"];
  }

  [alert setMessageText:base::SysUTF8ToNSString(GetHeadingText())];
  [alert setAlertStyle:NSWarningAlertStyle];
  [alert setIcon:gfx::NSImageFromImageSkia(icon_)];

  if ([alert runModal] == NSAlertFirstButtonReturn)
    delegate_->ExtensionUninstallAccepted();
  else
    delegate_->ExtensionUninstallCanceled();
}

}  // namespace

// static
extensions::ExtensionUninstallDialog*
extensions::ExtensionUninstallDialog::Create(Profile* profile,
                                             gfx::NativeWindow parent,
                                             Delegate* delegate) {
  return new ExtensionUninstallDialogCocoa(profile, parent, delegate);
}
