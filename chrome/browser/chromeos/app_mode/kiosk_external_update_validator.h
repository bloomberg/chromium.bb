// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_EXTERNAL_UPDATE_VALIDATOR_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_EXTERNAL_UPDATE_VALIDATOR_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/extensions/sandboxed_unpacker.h"

namespace extensions {
class Extension;
}

namespace chromeos {

// Delegate class for KioskExternalUpdateValidator, derived class must support
// WeakPtr.
class KioskExternalUpdateValidatorDelegate {
 public:
  virtual void OnExtenalUpdateUnpackSuccess(
      const std::string& app_id,
      const std::string& version,
      const std::string& min_browser_version,
      const base::FilePath& temp_dir) = 0;
  virtual void OnExternalUpdateUnpackFailure(const std::string& app_id) = 0;

 protected:
  virtual ~KioskExternalUpdateValidatorDelegate() {}
};

// Unpacks the crx file of the kiosk app and validates its signature.
class KioskExternalUpdateValidator
    : public extensions::SandboxedUnpackerClient {
 public:
  KioskExternalUpdateValidator(
      const scoped_refptr<base::SequencedTaskRunner>& backend_task_runner,
      const std::string& app_id,
      const base::FilePath& crx_path,
      const base::FilePath& crx_unpack_dir,
      const base::WeakPtr<KioskExternalUpdateValidatorDelegate>& delegate);

  // Starts validating the external crx file.
  void Start();

 private:
  virtual ~KioskExternalUpdateValidator();

  // SandboxedUnpackerClient overrides.
  virtual void OnUnpackFailure(const base::string16& error_message) OVERRIDE;
  virtual void OnUnpackSuccess(const base::FilePath& temp_dir,
                               const base::FilePath& extension_dir,
                               const base::DictionaryValue* original_manifest,
                               const extensions::Extension* extension,
                               const SkBitmap& install_icon) OVERRIDE;

  // Task runner for executing file I/O tasks.
  const scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;
  std::string app_id_;

  // The directory where the external crx file resides.
  base::FilePath crx_dir_;
  // The temporary directory used by SandBoxedUnpacker for unpacking extensions.
  const base::FilePath crx_unpack_dir_;

  base::WeakPtr<KioskExternalUpdateValidatorDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(KioskExternalUpdateValidator);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_EXTERNAL_UPDATE_VALIDATOR_H_
