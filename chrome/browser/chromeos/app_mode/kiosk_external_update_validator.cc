// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_external_update_validator.h"

#include "base/bind.h"
#include "base/location.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"

namespace chromeos {

KioskExternalUpdateValidator::KioskExternalUpdateValidator(
    const scoped_refptr<base::SequencedTaskRunner>& backend_task_runner,
    const std::string& app_id,
    const base::FilePath& crx_dir,
    const base::FilePath& crx_unpack_dir,
    const base::WeakPtr<KioskExternalUpdateValidatorDelegate>& delegate)
    : backend_task_runner_(backend_task_runner),
      app_id_(app_id),
      crx_dir_(crx_dir),
      crx_unpack_dir_(crx_unpack_dir),
      delegate_(delegate) {
}

KioskExternalUpdateValidator::~KioskExternalUpdateValidator() {
}

void KioskExternalUpdateValidator::Start() {
  scoped_refptr<extensions::SandboxedUnpacker> unpacker(
      new extensions::SandboxedUnpacker(crx_dir_,
                                        extensions::Manifest::EXTERNAL_PREF,
                                        extensions::Extension::NO_FLAGS,
                                        crx_unpack_dir_,
                                        backend_task_runner_.get(),
                                        this));
  if (!backend_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&extensions::SandboxedUnpacker::Start, unpacker.get()))) {
    NOTREACHED();
  }
}

void KioskExternalUpdateValidator::OnUnpackFailure(
    const base::string16& error_message) {
  LOG(ERROR) << "Failed to unpack external kiosk crx file: " << app_id_ << " "
             << error_message;
  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &KioskExternalUpdateValidatorDelegate::OnExternalUpdateUnpackFailure,
          delegate_,
          app_id_));
}

void KioskExternalUpdateValidator::OnUnpackSuccess(
    const base::FilePath& temp_dir,
    const base::FilePath& extension_dir,
    const base::DictionaryValue* original_manifest,
    const extensions::Extension* extension,
    const SkBitmap& install_icon) {
  DCHECK(app_id_ == extension->id());

  std::string minimum_browser_version;
  if (!extension->manifest()->GetString(
          extensions::manifest_keys::kMinimumChromeVersion,
          &minimum_browser_version)) {
    LOG(ERROR) << "Can't find minimum browser version for app " << app_id_;
    minimum_browser_version.clear();
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(
          &KioskExternalUpdateValidatorDelegate::OnExtenalUpdateUnpackSuccess,
          delegate_,
          app_id_,
          extension->VersionString(),
          minimum_browser_version,
          temp_dir));
}

}  // namespace chromeos
