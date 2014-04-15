// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/eula_util.h"

#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"

namespace installer {

namespace {

bool IsEULAAcceptanceFlagged(BrowserDistribution* dist) {
  // Chrome creates the EULA sentinel after the EULA has been accepted when
  // doing so is required by master_preferences. Assume the EULA has not been
  // accepted if the path to the sentinel cannot be determined.
  base::FilePath eula_sentinel;
  return InstallUtil::GetEULASentinelFilePath(&eula_sentinel) &&
      base::PathExists(eula_sentinel);
}

scoped_ptr<MasterPreferences> GetMasterPrefs(const ProductState& prod_state) {
  // The standard location of the master prefs is next to the chrome binary.
  base::FilePath master_prefs_path(
      prod_state.GetSetupPath().DirName().DirName().DirName());
  scoped_ptr<MasterPreferences> install_prefs(new MasterPreferences(
      master_prefs_path.AppendASCII(kDefaultMasterPrefs)));
  if (install_prefs && install_prefs->read_from_file())
    return install_prefs.Pass();

  return scoped_ptr<MasterPreferences>();
}

// Attempts to initialize |state| with any Chrome-related product.
// Returns true if any of these product are installed, otherwise false.
bool GetAnyChromeProductState(bool system_level, ProductState* state) {
  return state->Initialize(system_level, BrowserDistribution::CHROME_BROWSER)
      || state->Initialize(system_level, BrowserDistribution::CHROME_FRAME)
      || state->Initialize(system_level, BrowserDistribution::CHROME_APP_HOST);
}

}  // namespace

EULAAcceptanceResponse IsEULAAccepted(bool system_level) {
  ProductState prod_state;

  if (!system_level) {  // User-level case has seprate flow.
    // Do not simply check Chrome binaries. Instead, check whether or not
    // any Chrome-related products is installed, because the presence of any of
    // these products at user-level implies that the EULA has been accepted.
    return GetAnyChromeProductState(false, &prod_state)
        ? QUERY_EULA_ACCEPTED : QUERY_EULA_NOT_ACCEPTED;
  }

  // System-level. Try to use Chrome binaries product state.
  if (!prod_state.Initialize(true, BrowserDistribution::CHROME_BINARIES)) {
    // Fall back to Chrome Browser product state only.
    if (!prod_state.Initialize(true, BrowserDistribution::CHROME_BROWSER))
      return QUERY_EULA_FAIL;

    LOG_IF(DFATAL, prod_state.is_multi_install())
        << "Binaries are not installed, but Chrome is multi-install.";
  }
  BrowserDistribution* dist = BrowserDistribution::GetSpecificDistribution(
      BrowserDistribution::CHROME_BROWSER);

  // Is Google Update waiting for Chrome's EULA to be accepted?
  DWORD eula_accepted = 0;
  if (prod_state.GetEulaAccepted(&eula_accepted) && !eula_accepted)
    return QUERY_EULA_NOT_ACCEPTED;

  if (InstallUtil::IsFirstRunSentinelPresent() || IsEULAAcceptanceFlagged(dist))
    return QUERY_EULA_ACCEPTED;

  // EULA acceptance not flagged. Now see if it is required.
  scoped_ptr<MasterPreferences> install_prefs(GetMasterPrefs(prod_state));
  // If we fail to get master preferences, assume that EULA is not required.
  if (!install_prefs)
    return QUERY_EULA_ACCEPTED;

  bool eula_required = false;
  // If kRequireEula value is absent, assume EULA is not required.
  if (!install_prefs->GetBool(master_preferences::kRequireEula, &eula_required))
    return QUERY_EULA_ACCEPTED;

  return eula_required ? QUERY_EULA_NOT_ACCEPTED : QUERY_EULA_ACCEPTED;
}

}  // namespace installer
