// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_PREFS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_PREFS_H_

class PrefRegistrySimple;

namespace enterprise_connectors {

// Pref that maps to the "OnFileAttachedEnterpriseConnector" policy.
extern const char kOnFileAttachedPref[];

// Pref that maps to the "OnFileDownloadedEnterpriseConnector" policy.
extern const char kOnFileDownloadedPref[];

// Pref that maps to the "OnBulkDataEntryEnterpriseConnector" policy.
extern const char kOnBulkDataEntryPref[];

// Pref that maps to the "OnSecurityEventEnterpriseConnector" policy.
extern const char kOnSecurityEventPref[];

void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_CONNECTORS_PREFS_H_
