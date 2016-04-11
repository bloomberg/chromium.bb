// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_MODULE_LOAD_ANALYZER_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_MODULE_LOAD_ANALYZER_H_

#include <memory>

#include "base/memory/ref_counted.h"

namespace safe_browsing {

class IncidentReceiver;
class SafeBrowsingDatabaseManager;

// Registers a process-wide analysis with the incident reporting service that
// will examine modules loaded in the process.
void RegisterModuleLoadAnalysis(
    const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager);

// Callback to pass to the incident reporting service. The incident reporting
// service will decide when to start the analysis.
void VerifyModuleLoadState(
    const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
    std::unique_ptr<IncidentReceiver> incident_receiver);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_MODULE_LOAD_ANALYZER_H_
