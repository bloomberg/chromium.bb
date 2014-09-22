// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_APP_PACK_UPDATER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_APP_PACK_UPDATER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/extensions/external_cache.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"

namespace extensions {
class ExternalLoader;
}

namespace net {
class URLRequestContextGetter;
}

namespace policy {

class AppPackExternalLoader;
class EnterpriseInstallAttributes;

// The AppPackUpdater manages a set of extensions that are configured via a
// device policy to be locally cached and installed into the Demo user account
// at login time.
class AppPackUpdater : public chromeos::ExternalCache::Delegate {
 public:
  // Callback to listen for updates to the screensaver extension's path.
  typedef base::Callback<void(const base::FilePath&)> ScreenSaverUpdateCallback;

  // The |request_context| is used for the update checks.
  AppPackUpdater(net::URLRequestContextGetter* request_context,
                 EnterpriseInstallAttributes* install_attributes);
  virtual ~AppPackUpdater();

  // Returns true if the ExternalLoader for the app pack has already been
  // created.
  bool created_external_loader() const { return created_extension_loader_; }

  // Creates an extensions::ExternalLoader that will load the crx files
  // downloaded by the AppPackUpdater. This can be called at most once, and the
  // caller owns the returned value.
  extensions::ExternalLoader* CreateExternalLoader();

  // |callback| will be invoked whenever the screen saver extension's path
  // changes. It will be invoked "soon" after this call if a valid path already
  // exists. Subsequent calls will override the previous |callback|. A null
  // |callback| can be used to remove a previous callback.
  void SetScreenSaverUpdateCallback(const ScreenSaverUpdateCallback& callback);

  // If a user of one of the AppPack's extensions detects that the extension
  // is damaged then this method can be used to remove it from the cache and
  // retry to download it after a restart.
  void OnDamagedFileDetected(const base::FilePath& path);

 private:
  // Implementation of ExternalCache::Delegate:
  virtual void OnExtensionListsUpdated(
      const base::DictionaryValue* prefs) OVERRIDE;

  // Called when the app pack device setting changes.
  void AppPackChanged();

  // Loads the current policy and schedules a cache update.
  void LoadPolicy();

  // Notifies the |extension_loader_| that the cache has been updated, providing
  // it with an updated list of app-pack extensions.
  void UpdateExtensionLoader();

  // Sets |screen_saver_path_| and invokes |screen_saver_update_callback_| if
  // appropriate.
  void SetScreenSaverPath(const base::FilePath& path);

  // The extension ID and path of the CRX file of the screen saver extension,
  // if it is configured by the policy. Otherwise these fields are empty.
  std::string screen_saver_id_;
  base::FilePath screen_saver_path_;

  // Callback to invoke whenever the screen saver's extension path changes.
  // Can be null.
  ScreenSaverUpdateCallback screen_saver_update_callback_;

  // The extension loader wires the AppPackUpdater to the extensions system, and
  // makes it install the currently cached extensions.
  bool created_extension_loader_;
  base::WeakPtr<AppPackExternalLoader> extension_loader_;

  // For checking the device mode.
  EnterpriseInstallAttributes* install_attributes_;

  // Extension cache.
  chromeos::ExternalCache external_cache_;

  scoped_ptr<chromeos::CrosSettings::ObserverSubscription>
      app_pack_subscription_;

  base::WeakPtrFactory<AppPackUpdater> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppPackUpdater);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_APP_PACK_UPDATER_H_
