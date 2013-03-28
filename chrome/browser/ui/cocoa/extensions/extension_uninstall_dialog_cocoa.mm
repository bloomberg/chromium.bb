// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_uninstall_dialog.h"

#import <Cocoa/Cocoa.h>

#include <string>

#include "base/strings/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/extensions/extension.h"
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
class ExtensionUninstallDialogCocoa : public ExtensionUninstallDialog {
 public:
  ExtensionUninstallDialogCocoa(
      Profile* profile,
      Browser* browser,
      Delegate* delegate);
  virtual ~ExtensionUninstallDialogCocoa() OVERRIDE;

 private:
  virtual void Show() OVERRIDE;
};

ExtensionUninstallDialogCocoa::ExtensionUninstallDialogCocoa(
    Profile* profile,
    Browser* browser,
    ExtensionUninstallDialog::Delegate* delegate)
    : ExtensionUninstallDialog(profile, browser, delegate) {}

ExtensionUninstallDialogCocoa::~ExtensionUninstallDialogCocoa() {}

void ExtensionUninstallDialogCocoa::Show() {
  NSAlert* alert = [[[NSAlert alloc] init] autorelease];

  NSButton* continueButton = [alert addButtonWithTitle:l10n_util::GetNSString(
      IDS_EXTENSION_PROMPT_UNINSTALL_BUTTON)];
  // Clear the key equivalent (currently 'Return') because cancel is the default
  // button.
  [continueButton setKeyEquivalent:@""];

  NSButton* cancelButton = [alert addButtonWithTitle:l10n_util::GetNSString(
      IDS_CANCEL)];
  [cancelButton setKeyEquivalent:@"\r"];

  [alert setMessageText:l10n_util::GetNSStringF(
       IDS_EXTENSION_UNINSTALL_PROMPT_HEADING,
       UTF8ToUTF16(extension_->name()))];
  [alert setAlertStyle:NSWarningAlertStyle];
  [alert setIcon:gfx::NSImageFromImageSkia(icon_)];

  if ([alert runModal] == NSAlertFirstButtonReturn)
    delegate_->ExtensionUninstallAccepted();
  else
    delegate_->ExtensionUninstallCanceled();
}

}  // namespace

// static
ExtensionUninstallDialog* ExtensionUninstallDialog::Create(
    Profile* profile,
    Browser* browser,
    Delegate* delegate) {
  return new ExtensionUninstallDialogCocoa(profile, browser, delegate);
}
