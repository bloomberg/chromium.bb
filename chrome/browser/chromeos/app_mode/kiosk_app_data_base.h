// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_DATA_BASE_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_DATA_BASE_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_icon_loader.h"
#include "components/account_id/account_id.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/gfx/image/image_skia.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

class KioskAppDataBase : public KioskAppIconLoader::Delegate {
 public:
  // Dictionary key for apps.
  static const char kKeyApps[];

  const std::string& dictionary_name() const { return dictionary_name_; }
  const std::string& app_id() const { return app_id_; }
  const AccountId& account_id() const { return account_id_; }
  const std::string& name() const { return name_; }
  const gfx::ImageSkia& icon() const { return icon_; }

  // Callbacks for KioskAppIconLoader.
  void OnIconLoadSuccess(const gfx::ImageSkia& icon) override = 0;
  void OnIconLoadFailure() override = 0;

  // Clears locally cached data.
  void ClearCache();

 protected:
  KioskAppDataBase(const std::string& dictionary_name,
                   const std::string& app_id,
                   const AccountId& account_id);
  ~KioskAppDataBase() override;

  // Helper to save name and icon to provided dictionary.
  void SaveToDictionary(DictionaryPrefUpdate& dict_update);

  // Helper to load name and icon from provided dictionary.
  bool LoadFromDictionary(const base::DictionaryValue& dict);

  // Helper to cache |icon| to |cache_dir|.
  void SaveIcon(const SkBitmap& icon, const base::FilePath& cache_dir);

  // In protected section to allow derived classes to modify.
  std::string name_;
  gfx::ImageSkia icon_;

  // Should be released when callbacks are called.
  std::unique_ptr<KioskAppIconLoader> kiosk_app_icon_loader_;

 private:
  // Name of a dictionary that holds kiosk app info in Local State.
  const std::string dictionary_name_;

  const std::string app_id_;
  const AccountId account_id_;

  base::FilePath icon_path_;

  DISALLOW_COPY_AND_ASSIGN(KioskAppDataBase);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_DATA_BASE_H_
