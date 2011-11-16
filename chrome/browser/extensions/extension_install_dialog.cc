// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_dialog.h"

#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/common/extensions/extension.h"

// A flag used for SetExtensionInstallDialogForManifestAutoConfirmForTests
enum AutoConfirmForTest {
  DO_NOT_SKIP = 0,
  PROCEED,
  ABORT
};
AutoConfirmForTest auto_confirm_for_tests = DO_NOT_SKIP;

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
  if (auto_confirm_for_tests != DO_NOT_SKIP) {
    if (auto_confirm_for_tests == PROCEED)
      delegate->InstallUIProceed();
    else
      delegate->InstallUIAbort(true);
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

void SetExtensionInstallDialogForManifestAutoConfirmForTests(
    bool should_proceed) {
  auto_confirm_for_tests = should_proceed ? PROCEED : ABORT;
}
