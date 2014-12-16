// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_SUPERVISED_USER_WHITELIST_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_SUPERVISED_USER_WHITELIST_INSTALLER_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class FilePath;
}

namespace component_updater {

class ComponentUpdateService;
class OnDemandUpdater;

class SupervisedUserWhitelistInstaller {
 public:
  typedef base::Callback<void(const base::FilePath& whitelist_path)>
      WhitelistReadyCallback;

  virtual ~SupervisedUserWhitelistInstaller() {}

  static scoped_ptr<SupervisedUserWhitelistInstaller> Create(
      ComponentUpdateService* cus);

  // Turns a CRX ID (which is derived from a hash) back into a hash.
  // Note that the resulting hash will be only 16 bytes long instead of the
  // usual 32 bytes, as the CRX ID is created from the first half of the
  // original hash, but the component installer will still accept this.
  // Public for testing.
  static std::vector<uint8_t> GetHashFromCrxId(const std::string& crx_id);

  // Registers a new whitelist with the given CRX ID and name.
  // |new_installation| should be set to true if the whitelist is not installed
  // locally yet, to trigger installation.
  // |callback| will be called when the whitelist is installed (for new
  // installations) or updated.
  virtual void RegisterWhitelist(const std::string& crx_id,
                                 const std::string& name,
                                 bool new_installation,
                                 const WhitelistReadyCallback& callback) = 0;

  // Unregisters a whitelist.
  virtual void UnregisterWhitelist(const std::string& crx_id) = 0;

 protected:
  // Triggers an update for a whitelist to be installed. Protected so it can be
  // called from the implementation subclass, and declared here so that the
  // OnDemandUpdater can friend this class and the implementation subclass can
  // live in an anonymous namespace.
  static void TriggerComponentUpdate(OnDemandUpdater* updater,
                                     const std::string& crx_id);
};

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_SUPERVISED_USER_WHITELIST_INSTALLER_H_
