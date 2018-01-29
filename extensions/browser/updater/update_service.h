// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_UPDATE_SERVICE_H_
#define EXTENSIONS_BROWSER_UPDATER_UPDATE_SERVICE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/update_client/update_client.h"

namespace base {
class Version;
}

namespace content {
class BrowserContext;
}

namespace update_client {
enum class Error;
class UpdateClient;
}

namespace extensions {

class UpdateDataProvider;
class UpdateServiceFactory;
struct ExtensionUpdateCheckParams;

// This service manages the autoupdate of extensions.  It should eventually
// replace ExtensionUpdater in Chrome.
// TODO(rockot): Replace ExtensionUpdater with this service.
class UpdateService : public KeyedService,
                      update_client::UpdateClient::Observer {
 public:
  static UpdateService* Get(content::BrowserContext* context);

  void Shutdown() override;

  void SendUninstallPing(const std::string& id,
                         const base::Version& version,
                         int reason);

  // Starts an update check for each of extensions stored in |update_params|.
  // If there are any updates available, they will be downloaded, checked for
  // integrity, unpacked, and then passed off to the
  // ExtensionSystem::InstallUpdate method for install completion.
  void StartUpdateCheck(const ExtensionUpdateCheckParams& update_params);

  // This function verifies if the current implementation can update
  // |extension_id|.
  bool CanUpdate(const std::string& extension_id) const;

  // Overriden from update_client::UpdateClient::Observer.
  void OnEvent(Events event, const std::string& id) override;

 private:
  friend class UpdateServiceFactory;
  friend std::unique_ptr<UpdateService>::deleter_type;

  UpdateService(content::BrowserContext* context,
                scoped_refptr<update_client::UpdateClient> update_client);
  ~UpdateService() override;

  // This function is executed by the update client after an update check
  // request has completed.
  void UpdateCheckComplete(const std::vector<std::string>& extension_ids,
                           update_client::Error error);

  content::BrowserContext* browser_context_;

  scoped_refptr<update_client::UpdateClient> update_client_;
  scoped_refptr<UpdateDataProvider> update_data_provider_;

  // The set of extensions that are being checked for update.
  std::set<std::string> updating_extensions_;

  THREAD_CHECKER(thread_checker_);

  // used to create WeakPtrs to |this|.
  base::WeakPtrFactory<UpdateService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UpdateService);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_UPDATE_SERVICE_H_
