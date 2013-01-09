// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_SETTINGS_H_
#define CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_SETTINGS_H_

#include <string>

#include "base/basictypes.h"
#include "base/string16.h"
#include "base/time.h"
#include "base/version.h"
#include "chrome/installer/util/util_constants.h"

class BrowserDistribution;

namespace installer {
class ChannelInfo;
class InstallationState;
}

// This class provides accessors to the Google Update 'ClientState' information
// that recorded when the user downloads the chrome installer. It is
// google_update.exe responsibility to write the initial values.
class GoogleUpdateSettings {
 public:
  // Update policy constants defined by Google Update; do not change these.
  enum UpdatePolicy {
    UPDATES_DISABLED    = 0,
    AUTOMATIC_UPDATES   = 1,
    MANUAL_UPDATES_ONLY = 2,
  };

  // Defines product data that is tracked/used by Google Update.
  struct ProductData {
    // The currently installed version.
    std::string version;
    // The time that Google Update last updated this product.  (This means
    // either running an updater successfully, or doing an update check that
    // results in no update available.)
    base::Time last_success;
    // The result reported by the most recent run of an installer/updater.
    int last_result;
    // The error code, if any, reported by the most recent run of an
    // installer or updater.  This is typically platform independent.
    int last_error_code;
    // The extra error code, if any, reported by the most recent run of
    // an installer or updater.  This is typically an error code specific
    // to the platform -- i.e. on Windows, it will be a Win32 HRESULT.
    int last_extra_code;
  };

  // Returns true if this install is system-wide, false if it is per-user.
  static bool IsSystemInstall();

  // Returns whether the user has given consent to collect UMA data and send
  // crash dumps to Google. This information is collected by the web server
  // used to download the chrome installer.
  static bool GetCollectStatsConsent();

  // Sets the user consent to send UMA and crash dumps to Google. Returns
  // false if the setting could not be recorded.
  static bool SetCollectStatsConsent(bool consented);

#if defined(OS_WIN)
  // Returns whether the user has given consent to collect UMA data and send
  // crash dumps to Google. This information is collected by the web server
  // used to download the chrome installer.
  static bool GetCollectStatsConsentAtLevel(bool system_install);

  // Sets the user consent to send UMA and crash dumps to Google. Returns
  // false if the setting could not be recorded.
  static bool SetCollectStatsConsentAtLevel(bool system_install,
                                            bool consented);
#endif

  // Returns the metrics id set in the registry (that can be used in crash
  // reports). If none found, returns empty string.
  static bool GetMetricsId(std::wstring* metrics_id);

  // Sets the metrics id to be used in crash reports.
  static bool SetMetricsId(const std::wstring& metrics_id);

  // Sets the machine-wide EULA consented flag required on OEM installs.
  // Returns false if the setting could not be recorded.
  static bool SetEULAConsent(const installer::InstallationState& machine_state,
                             BrowserDistribution* dist,
                             bool consented);

  // Returns the last time chrome was run in days. It uses a recorded value
  // set by SetLastRunTime(). Returns -1 if the value was not found or if
  // the value is corrupted.
  static int GetLastRunTime();

  // Stores the time that this function was last called using an encoded
  // form of the system local time. Retrieve the time using GetLastRunTime().
  // Returns false if the value could not be stored.
  static bool SetLastRunTime();

  // Removes the storage used by SetLastRunTime() and SetLastRunTime(). Returns
  // false if the operation failed. Returns true if the storage was freed or
  // if it never existed in the first place.
  static bool RemoveLastRunTime();

  // Returns in |browser| the browser used to download chrome as recorded
  // Google Update. Returns false if the information is not available.
  static bool GetBrowser(std::wstring* browser);

  // Returns in |language| the language selected by the user when downloading
  // chrome. This information is collected by the web server used to download
  // the chrome installer. Returns false if the information is not available.
  static bool GetLanguage(std::wstring* language);

  // Returns in |brand| the RLZ brand code or distribution tag that has been
  // assigned to a partner. Returns false if the information is not available.
  //
  // NOTE: This function is Windows only.  If the code you are writing is not
  // specifically for Windows, prefer calling google_util::GetBrand().
  static bool GetBrand(std::wstring* brand);

  // Returns in |brand| the RLZ reactivation brand code or distribution tag
  // that has been assigned to a partner for reactivating a dormant chrome
  // install. Returns false if the information is not available.
  //
  // NOTE: This function is Windows only.  If the code you are writing is not
  // specifically for Windows, prefer calling
  // google_util::GetReactivationBrand().
  static bool GetReactivationBrand(std::wstring* brand);

  // Returns in |client| the google_update client field, which is currently
  // used to track experiments. Returns false if the entry does not exist.
  static bool GetClient(std::wstring* client);

  // Sets the google_update client field. Unlike GetClient() this is set only
  // for the current user. Returns false if the operation failed.
  static bool SetClient(const std::wstring& client);

  // Returns in 'client' the RLZ referral available for some distribution
  // partners. This value does not exist for most chrome or chromium installs.
  static bool GetReferral(std::wstring* referral);

  // Overwrites the current value of the referral with an empty string. Returns
  // true if this operation succeeded.
  static bool ClearReferral();

  // Set did_run "dr" in the client state value. This is used to measure
  // active users. Returns false if writting to the registry failed.
  static bool UpdateDidRunState(bool did_run, bool system_level);

  // Returns only the channel name: "" (stable), "dev", "beta", "canary", or
  // "unknown" if unknown. This value will not be modified by "-m" for a
  // multi-install. See kChromeChannel* in util_constants.h
  static std::wstring GetChromeChannel(bool system_install);

  // Return a human readable modifier for the version string, e.g.
  // the channel (dev, beta, stable). Returns true if this operation succeeded,
  // on success, channel contains one of "", "unknown", "dev" or "beta" (unless
  // it is a multi-install product, in which case it will return "m",
  // "unknown-m", "dev-m", or "beta-m").
  static bool GetChromeChannelAndModifiers(bool system_install,
                                           string16* channel);

  // This method changes the Google Update "ap" value to move the installation
  // on to or off of one of the recovery channels.
  // - If incremental installer fails we append a magic string ("-full"), if
  // it is not present already, so that Google Update server next time will send
  // full installer to update Chrome on the local machine
  // - If we are currently running full installer, we remove this magic
  // string (if it is present) regardless of whether installer failed or not.
  // There is no fall-back for full installer :)
  // - Unconditionally remove "-multifail" since we haven't crashed.
  // |state_key| should be obtained via InstallerState::state_key().
  static void UpdateInstallStatus(bool system_install,
                                  installer::ArchiveType archive_type,
                                  int install_return_code,
                                  const std::wstring& product_guid);

  // This method updates the value for Google Update "ap" key for Chrome
  // based on whether we are doing incremental install (or not) and whether
  // the install succeeded.
  // - If install worked, remove the magic string (if present).
  // - If incremental installer failed, append a magic string (if
  //   not present already).
  // - If full installer failed, still remove this magic
  //   string (if it is present already).
  //
  // archive_type: tells whether this is incremental install or not.
  // install_return_code: if 0, means installation was successful.
  // value: current value of Google Update "ap" key.
  // Returns true if |value| is modified.
  static bool UpdateGoogleUpdateApKey(installer::ArchiveType archive_type,
                                      int install_return_code,
                                      installer::ChannelInfo* value);

  // For system-level installs, we need to be able to communicate the results
  // of the Toast Experiments back to Google Update. The problem is just that
  // the experiment is run in the context of the user, which doesn't have
  // write access to the HKLM key that Google Update expects the results in.
  // However, when we are about to switch contexts from system to user, we can
  // duplicate the handle to the registry key and pass it (through handle
  // inheritance) to the newly created child process that is launched as the
  // user, allowing the child process to write to the key, with the
  // WriteGoogleUpdateSystemClientKey function below.
  static int DuplicateGoogleUpdateSystemClientKey();

  // Takes a |handle| to a registry key and writes |value| string into the
  // specified |key|. See DuplicateGoogleUpdateSystemClientKey for details.
  static bool WriteGoogleUpdateSystemClientKey(int handle,
                                               const std::wstring& key,
                                               const std::wstring& value);

  // Returns the effective update policy for |app_guid| as dictated by
  // Group Policy settings.  |is_overridden|, if non-NULL, is populated with
  // true if an app-specific policy override is in force, or false otherwise.
  static UpdatePolicy GetAppUpdatePolicy(const std::wstring& app_guid,
                                         bool* is_overridden);

  // Returns Google Update's uninstall command line, or an empty string if none
  // is found.
  static string16 GetUninstallCommandLine(bool system_install);

  // Returns the version of Google Update that is installed.
  static Version GetGoogleUpdateVersion(bool system_install);

  // Returns the time at which Google Update last started an automatic update
  // check, or the null time if this information isn't available.
  static base::Time GetGoogleUpdateLastStartedAU(bool system_install);

  // Returns the time at which Google Update last successfully contacted Google
  // servers and got a valid check response, or the null time if this
  // information isn't available.
  static base::Time GetGoogleUpdateLastChecked(bool system_install);

  // Returns detailed update data for a product being managed by Google Update.
  // Returns true if the |version| and |last_updated| fields in |data|
  // are modified.  The other items are considered optional.
  static bool GetUpdateDetailForApp(bool system_install,
                                    const wchar_t* app_guid,
                                    ProductData* data);

  // Returns product data for Google Update.  (Equivalent to calling
  // GetUpdateDetailForAppGuid with the app guid for Google Update itself.)
  static bool GetUpdateDetailForGoogleUpdate(bool system_install,
                                             ProductData* data);

  // Returns product data for the current product. (Equivalent to calling
  // GetUpdateDetailForApp with the app guid stored in BrowserDistribution.)
  static bool GetUpdateDetail(bool system_install, ProductData* data);

  // Sets |experiment_labels| as the Omaha experiment_labels value in the
  // ClientState key for this Chrome product, if appropriate. If
  // |experiment_labels| is empty, this will delete the value instead. This will
  // return true if the label was successfully set (or deleted), false otherwise
  // (even if the label does not need to be set for this particular distribution
  // type).
  static bool SetExperimentLabels(bool system_install,
                                  const string16& experiment_labels);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(GoogleUpdateSettings);
};

#endif  // CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_SETTINGS_H_
