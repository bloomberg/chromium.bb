// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_dialog.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"

namespace {

// A flag used for SetExtensionInstallDialogAutoConfirmForTests
enum AutoConfirmForTest {
  DO_NOT_SKIP = 0,
  PROCEED,
  ABORT
};

void AutoConfirmTask(ExtensionInstallUI::Delegate* delegate, bool proceed) {
  if (proceed)
    delegate->InstallUIProceed();
  else
    delegate->InstallUIAbort(true);
}

void DoAutoConfirm(AutoConfirmForTest setting,
                   ExtensionInstallUI::Delegate* delegate) {
  bool proceed = (setting == PROCEED);
  // We use PostTask instead of calling the delegate directly here, because in
  // the real implementations it's highly likely the message loop will be
  // pumping a few times before the user clicks accept or cancel.
  MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&AutoConfirmTask, delegate, proceed));
}

AutoConfirmForTest CheckAutoConfirmCommandLineSwitch() {
  const CommandLine* cmdline = CommandLine::ForCurrentProcess();
  if (!cmdline->HasSwitch(switches::kAppsGalleryInstallAutoConfirmForTests))
    return DO_NOT_SKIP;
  std::string value = cmdline->GetSwitchValueASCII(
      switches::kAppsGalleryInstallAutoConfirmForTests);
  if (value == "accept")
    return PROCEED;
  else if (value == "cancel")
    return ABORT;
  else
    NOTREACHED();
  return DO_NOT_SKIP;
}

}  // namespace

void ShowExtensionInstallDialog(Profile* profile,
                                ExtensionInstallUI::Delegate* delegate,
                                const Extension* extension,
                                SkBitmap* icon,
                                const ExtensionInstallUI::Prompt& prompt) {
  AutoConfirmForTest auto_confirm = CheckAutoConfirmCommandLineSwitch();
  if (auto_confirm != DO_NOT_SKIP) {
    DoAutoConfirm(auto_confirm, delegate);
    return;
  }
  ShowExtensionInstallDialogImpl(profile, delegate, extension, icon, prompt);
}

bool ShowExtensionInstallDialogForManifest(
    Profile *profile,
    ExtensionInstallUI::Delegate* delegate,
    const DictionaryValue* manifest,
    const std::string& id,
    const std::string& localized_name,
    const std::string& localized_description,
    SkBitmap* icon,
    const ExtensionInstallUI::Prompt& prompt,
    scoped_refptr<Extension>* dummy_extension) {
  scoped_ptr<DictionaryValue> localized_manifest;
  if (!localized_name.empty() || !localized_description.empty()) {
    localized_manifest.reset(manifest->DeepCopy());
    if (!localized_name.empty()) {
      localized_manifest->SetString(extension_manifest_keys::kName,
                                    localized_name);
    }
    if (!localized_description.empty()) {
      localized_manifest->SetString(extension_manifest_keys::kDescription,
                                    localized_description);
    }
  }

  std::string init_errors;
  *dummy_extension = Extension::CreateWithId(
      FilePath(),
      Extension::INTERNAL,
      localized_manifest.get() ? *localized_manifest.get() : *manifest,
      Extension::NO_FLAGS,
      id,
      &init_errors);
  if (!dummy_extension->get()) {
    return false;
  }

  if (icon->empty())
    icon = const_cast<SkBitmap*>(&Extension::GetDefaultIcon(
        (*dummy_extension)->is_app()));

  // In tests, we may have setup to proceed or abort without putting up the real
  // confirmation dialog.
  AutoConfirmForTest auto_confirm = CheckAutoConfirmCommandLineSwitch();
  if (auto_confirm != DO_NOT_SKIP) {
    DoAutoConfirm(auto_confirm, delegate);
    return true;
  }

  ExtensionInstallUI::Prompt filled_out_prompt = prompt;
  filled_out_prompt.SetPermissions(
      (*dummy_extension)->GetPermissionMessageStrings());

  ShowExtensionInstallDialog(profile,
                             delegate,
                             dummy_extension->get(),
                             icon,
                             filled_out_prompt);
  return true;
}
