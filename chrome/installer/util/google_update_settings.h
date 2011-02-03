// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_SETTINGS_H_
#define CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_SETTINGS_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "chrome/installer/util/util_constants.h"

namespace installer {
class ChannelInfo;
class InstallerState;
}

// This class provides accessors to the Google Update 'ClientState' information
// that recorded when the user downloads the chrome installer. It is
// google_update.exe responsability to write the initial values.
class GoogleUpdateSettings {
 public:
  // Returns whether the user has given consent to collect UMA data and send
  // crash dumps to Google. This information is collected by the web server
  // used to download the chrome installer.
  static bool GetCollectStatsConsent();

  // Sets the user consent to send UMA and crash dumps to Google. Returns
  // false if the setting could not be recorded.
  static bool SetCollectStatsConsent(bool consented);

  // Returns the metrics id set in the registry (that can be used in crash
  // reports). If none found, returns empty string.
  static bool GetMetricsId(std::wstring* metrics_id);

  // Sets the metrics id to be used in crash reports.
  static bool SetMetricsId(const std::wstring& metrics_id);

  // Sets the machine-wide EULA consented flag required on OEM installs.
  // Returns false if the setting could not be recorded.
  static bool SetEULAConsent(const installer::InstallerState& installer_state,
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
  static bool GetBrand(std::wstring* brand);

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

  // Return a human readable modifier for the version string, e.g.
  // the channel (dev, beta, stable). Returns true if this operation succeeded,
  // on success, channel contains one of "", "unknown", "dev" or "beta".
  static bool GetChromeChannel(bool system_install, std::wstring* channel);

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

  // True if a build is strictly organic, according to its brand code.
  static bool IsOrganic(const std::wstring& brand);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(GoogleUpdateSettings);
};

#endif  // CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_SETTINGS_H_
