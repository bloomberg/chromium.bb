// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APPS_APK_WEB_APP_INSTALLER_H_
#define CHROME_BROWSER_CHROMEOS_APPS_APK_WEB_APP_INSTALLER_H_

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/web_application_info.h"
#include "components/arc/common/app.mojom.h"
#include "extensions/common/extension_id.h"

class Profile;

namespace extensions {
class BookmarkAppHelper;
class Extension;
}  // namespace extensions

namespace service_manager {
class Connector;
}

namespace chromeos {

// Class which takes an arc::mojom::WebAppInfo struct and decodes the data
// within it to install as a local web app.
class ApkWebAppInstaller {
 public:
  using InstallFinishCallback =
      base::OnceCallback<void(const extensions::ExtensionId& web_app_id)>;

  // Do nothing class purely for the purpose of allowing us to specify
  // a WeakPtr<Owner> member as a proxy for a profile lifetime observer.
  class Owner {};

  // Installs a web app represented by |web_app_info| with icon bytes
  // |icon_png_data| into |profile|. |owner| must stay alive for this class to
  // finish its tasks, otherwise installation will be aborted. Runs |callback|
  // with either the id of the installed web app if installation was successful,
  // or an empty id if not.
  static void Install(Profile* profile,
                      arc::mojom::WebAppInfoPtr web_app_info,
                      const std::vector<uint8_t>& icon_png_data,
                      InstallFinishCallback callback,
                      base::WeakPtr<Owner> weak_owner);

 protected:
  // This class owns itself and deletes itself when it is finished.
  ApkWebAppInstaller(Profile* profile,
                     InstallFinishCallback callback,
                     base::WeakPtr<Owner> weak_owner);
  virtual ~ApkWebAppInstaller();

  // Starts the installation flow by decoding icon data.
  void Start(service_manager::Connector* connector,
             arc::mojom::WebAppInfoPtr web_app_info,
             const std::vector<uint8_t>& icon_png_data);

  // Calls |callback_| with |id|, and deletes this object. Virtual for testing.
  virtual void CompleteInstallation(const extensions::ExtensionId& id);

  // Callback method for BookmarkAppHelper::Create.
  void OnBookmarkAppCreated(const extensions::Extension* extension,
                            const WebApplicationInfo& web_app_info);

  // Callback method for data_decoder::DecodeImage.
  void OnImageDecoded(const SkBitmap& decoded_image);

  // Run the installation. Virtual for testing.
  virtual void DoInstall();

  const WebApplicationInfo& web_app_info() const { return web_app_info_; }

 private:
  std::unique_ptr<extensions::BookmarkAppHelper> helper_;
  std::unique_ptr<service_manager::Connector> connector_;

  // If |weak_owner_| is ever invalidated while this class is working,
  // installation will be aborted. |weak_owner_|'s lifetime must be equal to or
  // shorter than that of |profile_|.
  Profile* profile_;
  InstallFinishCallback callback_;
  base::WeakPtr<Owner> weak_owner_;

  WebApplicationInfo web_app_info_;

  DISALLOW_COPY_AND_ASSIGN(ApkWebAppInstaller);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APPS_APK_WEB_APP_INSTALLER_H_
