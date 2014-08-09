// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_ENVIRONMENT_DATA_COLLECTION_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_ENVIRONMENT_DATA_COLLECTION_WIN_H_

namespace safe_browsing {

class ClientIncidentReport_EnvironmentData_Process;

// Collects then populates |process| with the sanitized paths of all DLLs
// loaded in the current process. Return false if an error occurred while
// querying for the loaded dlls.
bool CollectDlls(ClientIncidentReport_EnvironmentData_Process* process);

// For each of the dlls in this already populated incident report,
// check one of them is a registered LSP.
void RecordLspFeature(ClientIncidentReport_EnvironmentData_Process* process);

// Checks each module in the provided list for modifications and records these,
// along with any modified exports, in |process|.
void CollectModuleVerificationData(
    const wchar_t* const modules_to_verify[],
    size_t num_modules_to_verify,
    ClientIncidentReport_EnvironmentData_Process* process);

// Populates |process| with the dll names that have been added to the chrome elf
// blacklist through the Windows registry.
void CollectDllBlacklistData(
    ClientIncidentReport_EnvironmentData_Process* process);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_ENVIRONMENT_DATA_COLLECTION_WIN_H_
