// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used with Google Update.

#ifndef CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_CONSTANTS_H_
#define CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_CONSTANTS_H_
#pragma once

namespace google_update {

// Strictly speaking Google Update doesn't care about this GUID but it is still
// related to install as it is used by MSI to identify Gears.
extern const wchar_t kGearsUpgradeCode[];

// The GUID Google Update uses to keep track of Chrome upgrades.
extern const wchar_t kChromeUpgradeCode[];

extern const wchar_t kRegPathClients[];

// The difference between ClientState and ClientStateMedium is that the former
// lives on HKCU or HKLM and the later always lives in HKLM. The only use of
// the ClientStateMedium is for the EULA consent. See bug 1594565.
extern const wchar_t kRegPathClientState[];
extern const wchar_t kRegPathClientStateMedium[];

// The name of the "Commands" key that lives in an app's Clients key
// (a.k.a. "Version" key).
extern const wchar_t kRegCommandsKey[];

extern const wchar_t kRegApField[];
extern const wchar_t kRegBrowserField[];
extern const wchar_t kRegCFEndTempOptOutCmdField[];
extern const wchar_t kRegCFOptInCmdField[];
extern const wchar_t kRegCFOptOutCmdField[];
extern const wchar_t kRegCFTempOptOutCmdField[];
extern const wchar_t kRegClientField[];
extern const wchar_t kRegCommandLineField[];
extern const wchar_t kRegDidRunField[];
extern const wchar_t kRegEULAAceptedField[];
extern const wchar_t kRegLangField[];
extern const wchar_t kRegLastCheckedField[];
extern const wchar_t kRegMetricsId[];
extern const wchar_t kRegMSIField[];
extern const wchar_t kRegNameField[];
extern const wchar_t kRegOldVersionField[];
extern const wchar_t kRegOopcrashesField[];
extern const wchar_t kRegRLZBrandField[];
extern const wchar_t kRegReferralField[];
extern const wchar_t kRegRenameCmdField[];
extern const wchar_t kRegSendsPingsField[];
extern const wchar_t kRegUsageStatsField[];
extern const wchar_t kRegVersionField[];
extern const wchar_t kRegWebAccessibleField[];

// last time that chrome ran in the Time internal format.
extern const wchar_t kRegLastRunTimeField[];

}  // namespace google_update

#endif  // CHROME_INSTALLER_UTIL_GOOGLE_UPDATE_CONSTANTS_H_
