// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PENDING_EXTENSION_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_PENDING_EXTENSION_MANAGER_H_
#pragma once

#include <map>
#include <string>

#include "chrome/browser/extensions/pending_extension_info.h"
#include "chrome/common/extensions/extension.h"

class ExtensionServiceInterface;
class GURL;
class PendingExtensionManager;
class Version;

namespace extensions {
class ExtensionUpdaterTest;
void SetupPendingExtensionManagerForTest(
    int count, const GURL& update_url,
    PendingExtensionManager* pending_extension_manager);
}

// Class PendingExtensionManager manages the set of extensions which are
// being installed or updated. In general, installation and updates take
// time, because they involve downloading, unpacking, and installing.
// This class allows us to avoid race cases where multiple sources install
// the same extension.
// The extensions service creates an instance of this class, and manages
// its lifetime. This class should only be used from the UI thread.
class PendingExtensionManager {
 public:
  // |service| is a reference to the ExtensionService whose pending
  // extensions we are managing. The service creates an instance of
  // this class on construction, and destroys it on destruction.
  // The service remains valid over the entire lifetime of this class.
  explicit PendingExtensionManager(const ExtensionServiceInterface& service);
  ~PendingExtensionManager();

  // TODO(skerner): Many of these methods can be private once code in
  // ExtensionService is moved into methods of this class.

  // Remove |id| from the set of pending extensions.
  void Remove(const std::string& id);

  // Get the  information for a pending extension.  Returns true and sets
  // |out_pending_extension_info| if there is a pending extension with id
  // |id|.  Returns false otherwise.
  bool GetById(const std::string& id,
               PendingExtensionInfo* out_pending_extension_info) const;

  // Is |id| in the set of pending extensions?
  bool IsIdPending(const std::string& id) const;

  // Adds an extension in a pending state; the extension with the
  // given info will be installed on the next auto-update cycle.
  // Return true if the extension was added.  Will return false
  // if the extension is pending from another source which overrides
  // sync installs (such as a policy extension) or if the extension
  // is already installed.
  //
  // TODO(akalin): Replace |install_silently| with a list of
  // pre-enabled permissions.
  bool AddFromSync(
      const std::string& id,
      const GURL& update_url,
      PendingExtensionInfo::ShouldAllowInstallPredicate should_allow_install,
      bool install_silently);

  // Given an extension id and an update URL, schedule the extension
  // to be fetched, installed, and activated.
  bool AddFromExternalUpdateUrl(const std::string& id,
                                const GURL& update_url,
                                Extension::Location location);

  // Add a pending extension record for an external CRX file.
  // Return true if the CRX should be installed, false if an existing
  // pending record overrides it.
  bool AddFromExternalFile(
      const std::string& id,
      Extension::Location location,
      const Version& version);

  // Get the set of pending IDs that should be installed from an update URL.
  // Pending extensions that will be installed from local files will not be
  // included in the set.
  void GetPendingIdsForUpdateCheck(
      std::set<std::string>* out_ids_for_update_check) const;

 private:
  typedef std::map<std::string, PendingExtensionInfo> PendingExtensionMap;

  // Assumes an extension with id |id| is not already installed.
  // Return true if the extension was added.
  bool AddExtensionImpl(
      const std::string& id,
      const GURL& update_url,
      const Version& version,
      PendingExtensionInfo::ShouldAllowInstallPredicate should_allow_install,
      bool is_from_sync,
      bool install_silently,
      Extension::Location install_source);

  // Add a pending extension record directly.  Used for unit tests that need
  // to set an inital state. Use friendship to allow the tests to call this
  // method.
  void AddForTesting(const std::string& id,
                     const PendingExtensionInfo& pending_etension_info);

  // Reference to the extension service whose pending extensions this class is
  // managing.  Because this class is a member of |service_|, it is created
  // and destroyed with |service_|. We only use methods from the interface
  // ExtensionServiceInterface.
  const ExtensionServiceInterface& service_;

  // A map from extension id to the pending extension info for that extension.
  PendingExtensionMap pending_extension_map_;

  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest,
                           UpdatePendingExtensionAlreadyInstalled);
  friend class extensions::ExtensionUpdaterTest;
  friend void extensions::SetupPendingExtensionManagerForTest(
      int count, const GURL& update_url,
      PendingExtensionManager* pending_extension_manager);

  DISALLOW_COPY_AND_ASSIGN(PendingExtensionManager);
};

#endif  // CHROME_BROWSER_EXTENSIONS_PENDING_EXTENSION_MANAGER_H_
