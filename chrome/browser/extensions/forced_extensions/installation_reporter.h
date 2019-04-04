// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALLATION_REPORTER_H_
#define CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALLATION_REPORTER_H_

#include <utility>

#include "base/optional.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/updater/extension_downloader_delegate.h"
#include "extensions/common/extension_id.h"

class Profile;

namespace extensions {

// Helper class to save and retrieve extension installation stage and failure
// reasons.
class InstallationReporter {
 public:
  // Stage of extension installing process. Typically forced extensions from
  // policies should go through all stages in this order, other extensions skip
  // CREATED stage.
  enum class Stage {
    // Extension found in ForceInstall policy and added to
    // ExtensionManagement::settings_by_id_.
    CREATED,

    // Extension added to PendingExtensionManager.
    PENDING,

    // Extension added to ExtensionDownloader.
    DOWNLOADING,

    // Extension archive downloaded and is about to be unpacked/checked/etc.
    INSTALLING,

    // Extension installation finished (either successfully or not).
    COMPLETE,
  };

  // Enum used for UMA. Do NOT reorder or remove entry. Don't forget to
  // update enums.xml (name: ExtensionInstallationFailureReason) when adding new
  // entries.
  enum class FailureReason {
    // Reason for the failure is not reported. Typically this should not happen,
    // because if we know that we need to install an extension, it should
    // immediately switch to CREATED stage leading to IN_PROGRESS failure
    // reason, not UNKNOWN.
    UNKNOWN = 0,

    // Invalid id of the extension.
    INVALID_ID = 1,

    // Error during parsing extension individual settings.
    MALFORMED_EXTENSION_SETTINGS = 2,

    // The extension is marked as replaced by ARC app.
    REPLACED_BY_ARC_APP = 3,

    // Malformed extension dictionary for the extension.
    MALFORMED_EXTENSION_DICT = 4,

    // The extension format from extension dict is not supported.
    NOT_SUPPORTED_EXTENSION_DICT = 5,

    // Invalid file path in the extension dict.
    MALFORMED_EXTENSION_DICT_FILE_PATH = 6,

    // Invalid version in the extension dict.
    MALFORMED_EXTENSION_DICT_VERSION = 7,

    // Invalid updated URL in the extension dict.
    MALFORMED_EXTENSION_DICT_UPDATE_URL = 8,

    // The extension doesn't support browser locale.
    LOCALE_NOT_SUPPORTED = 9,

    // The extension marked as it shouldn't be installed.
    NOT_PERFORMING_NEW_INSTALL = 10,

    // Profile is older than supported by the extension.
    TOO_OLD_PROFILE = 11,

    // The extension can't be installed for enterprise.
    DO_NOT_INSTALL_FOR_ENTERPRISE = 12,

    // The extension is already installed.
    ALREADY_INSTALLED = 13,

    // The download of the crx failed.
    CRX_FETCH_FAILED = 14,

    // Failed to fetch the manifest for this extension.
    MANIFEST_FETCH_FAILED = 15,

    // The manifest couldn't be parsed.
    MANIFEST_INVALID = 16,

    // The manifest was fetched and parsed, and there are no updates for this
    // extension.
    NO_UPDATE = 17,

    // The crx was downloaded, but failed to install.
    // Corresponds to CrxInstallErrorType.
    CRX_INSTALL_ERROR_DECLINED = 18,
    CRX_INSTALL_ERROR_SANDBOXED_UNPACKER_FAILURE = 19,
    CRX_INSTALL_ERROR_OTHER = 20,

    // Extensions without update url should receive a default one, but somewhy
    // this didn't work. Internal error, should never happen.
    NO_UPDATE_URL = 21,

    // Extension failed to add to PendingExtensionManager.
    PENDING_ADD_FAILED = 22,

    // ExtensionDownloader refuses to start downloading this extensions
    // (possible reasons: invalid ID/URL).
    DOWNLOADER_ADD_FAILED = 23,

    // Extension (at the moment of check) is not installed nor some installation
    // error reported, so extension is being installed now, stuck in some stage
    // or some failure was not reported. See enum Stage for more details.
    // This option is a failure only in the sense that we failed to install
    // extension in required time.
    IN_PROGRESS = 24,

    // Magic constant used by the histogram macros.
    // Always update it to the max value.
    kMaxValue = IN_PROGRESS,
  };

  // Contains information about extension installation: failure reason, if any
  // reported, specific details in case of CRX install error, current
  // installation stage if known.
  struct InstallationData {
    InstallationData();
    InstallationData(const InstallationData&);

    base::Optional<extensions::InstallationReporter::Stage> install_stage;
    base::Optional<extensions::ExtensionDownloaderDelegate::Stage>
        downloading_stage;
    base::Optional<extensions::InstallationReporter::FailureReason>
        failure_reason;
    base::Optional<extensions::CrxInstallErrorDetail> install_error_detail;
  };

  // Remembers failure reason and in-progress stages in memory.
  static void ReportInstallationStage(const Profile* profile,
                                      const ExtensionId& id,
                                      Stage stage);
  static void ReportFailure(const Profile* profile,
                            const ExtensionId& id,
                            FailureReason reason);
  static void ReportDownloadingStage(const Profile* profile,
                                     const ExtensionId& id,
                                     ExtensionDownloaderDelegate::Stage stage);
  static void ReportCrxInstallError(const Profile* profile,
                                    const ExtensionId& id,
                                    FailureReason reason,
                                    CrxInstallErrorDetail crx_install_error);

  // Retrieves known information for installation of extension |id|.
  // Returns empty data if not found.
  static InstallationData Get(const Profile* profile, const ExtensionId& id);
  static std::string GetFormattedInstallationData(const InstallationData& data);

  // Clears all failures for the given profile.
  static void Clear(const Profile* profile);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALLATION_REPORTER_H_
