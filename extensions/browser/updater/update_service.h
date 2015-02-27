// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_UPDATE_SERVICE_H_
#define EXTENSIONS_BROWSER_UPDATER_UPDATE_SERVICE_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/updater/extension_downloader_delegate.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class ExtensionDownloader;
class UpdateServiceFactory;
class UpdateServiceTest;

// This service manages the download, update, and installation of extensions.
// It is currently only used by app_shell, but should eventually replace
// ExtensionUpdater in Chrome.
// TODO(rockot): Replace ExtensionUpdater with this service.
class UpdateService : public KeyedService, public ExtensionDownloaderDelegate {
 public:
  static UpdateService* Get(content::BrowserContext* context);

  // TODO(rockot): Remove this. It's a placeholder for a real service interface.
  // Downloads and (TODO) installs a CRX within the current browser context.
  void DownloadAndInstall(const std::string& id,
                          const base::Callback<void(bool)>& callback);

 private:
  friend class UpdateServiceFactory;
  friend class UpdateServiceTest;

  explicit UpdateService(content::BrowserContext* context);
  ~UpdateService() override;

  // ExtensionDownloaderDelegate:
  void OnExtensionDownloadFailed(const std::string& id,
                                 Error error,
                                 const PingResult& ping,
                                 const std::set<int>& request_ids) override;
  void OnExtensionDownloadFinished(const CRXFileInfo& file,
                                   bool file_ownership_passed,
                                   const GURL& download_url,
                                   const std::string& version,
                                   const PingResult& ping,
                                   const std::set<int>& request_id,
                                   const InstallCallback& callback) override;
  bool IsExtensionPending(const std::string& id) override;
  bool GetExtensionExistingVersion(const std::string& id,
                                   std::string* version) override;

  content::BrowserContext* browser_context_;
  scoped_ptr<ExtensionDownloader> downloader_;
  base::Callback<void(bool)> download_callback_;

  DISALLOW_COPY_AND_ASSIGN(UpdateService);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_UPDATE_SERVICE_H_
