// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_DOWNLOADER_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_DOWNLOADER_DELEGATE_H_

#include <set>
#include <string>

#include "base/time/time.h"
#include "chrome/browser/extensions/updater/manifest_fetch_data.h"

class GURL;

namespace base {
class FilePath;
}

namespace extensions {

class ExtensionDownloaderDelegate {
 public:
  virtual ~ExtensionDownloaderDelegate();

  // Passed as an argument to ExtensionDownloader::OnExtensionDownloadFailed()
  // to detail the reason for the failure.
  enum Error {
    // Background networking is disabled.
    DISABLED,

    // Failed to fetch the manifest for this extension.
    MANIFEST_FETCH_FAILED,

    // The manifest couldn't be parsed.
    MANIFEST_INVALID,

    // The manifest was fetched and parsed, and there are no updates for
    // this extension.
    NO_UPDATE_AVAILABLE,

    // There was an update for this extension but the download of the crx
    // failed.
    CRX_FETCH_FAILED,
  };

  // Passed as an argument to the completion callbacks to signal whether
  // the extension update sent a ping.
  struct PingResult {
    PingResult();
    ~PingResult();

    // Whether a ping was sent.
    bool did_ping;

    // The start of day, from the server's perspective. This is only valid
    // when |did_ping| is true.
    base::Time day_start;
  };

  // One of the following 3 methods is always invoked for a given extension
  // id, if AddExtension() or AddPendingExtension() returned true when that
  // extension was added to the ExtensionDownloader.
  // To avoid duplicate work, ExtensionDownloader might merge multiple identical
  // requests, so there is not necessarily a separate invocation of one of these
  // methods for each call to AddExtension/AddPendingExtension. If it is
  // important to be able to match up AddExtension calls with
  // OnExtensionDownload callbacks, you need to make sure that for every call to
  // AddExtension/AddPendingExtension the combination of extension id and
  // request id is unique. The OnExtensionDownload related callbacks will then
  // be called with all request ids that resulted in that extension being
  // checked.

  // Invoked if the extension couldn't be downloaded. |error| contains the
  // failure reason.
  virtual void OnExtensionDownloadFailed(const std::string& id,
                                         Error error,
                                         const PingResult& ping_result,
                                         const std::set<int>& request_ids);

  // Invoked if the extension had an update available and its crx was
  // successfully downloaded to |path|. |ownership_passed| is true if delegate
  // should get ownership of the file.
  virtual void OnExtensionDownloadFinished(
      const std::string& id,
      const base::FilePath& path,
      bool file_ownership_passed,
      const GURL& download_url,
      const std::string& version,
      const PingResult& ping_result,
      const std::set<int>& request_ids) = 0;

  // The remaining methods are used by the ExtensionDownloader to retrieve
  // information about extensions from the delegate.

  // Invoked to fill the PingData for the given extension id. Returns false
  // if PingData should not be included for this extension's update check
  // (this is the default).
  virtual bool GetPingDataForExtension(const std::string& id,
                                       ManifestFetchData::PingData* ping);

  // Invoked to get the update url data for this extension's update url, if
  // there is any. The default implementation returns an empty string.
  virtual std::string GetUpdateUrlData(const std::string& id);

  // Invoked to determine whether extension |id| is currently
  // pending installation.
  virtual bool IsExtensionPending(const std::string& id) = 0;

  // Invoked to get the current version of extension |id|. Returns false if
  // that extension is not installed.
  virtual bool GetExtensionExistingVersion(const std::string& id,
                                           std::string* version) = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_DOWNLOADER_DELEGATE_H_
