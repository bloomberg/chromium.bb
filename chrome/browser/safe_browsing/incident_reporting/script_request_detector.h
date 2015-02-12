// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_SCRIPT_REQUEST_DETECTOR_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_SCRIPT_REQUEST_DETECTOR_H_

#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_receiver.h"

namespace net {
class URLRequest;
}

namespace safe_browsing {

class ClientIncidentReport_IncidentData_ScriptRequestIncident;

// Observes network requests and reports suspicious activity.
class ScriptRequestDetector {
 public:
  explicit ScriptRequestDetector(
      scoped_ptr<IncidentReceiver> incident_receiver);
  ~ScriptRequestDetector();

  // Analyzes the |request| and triggers an incident report on suspicious
  // script inclusion.
  void OnResourceRequest(const net::URLRequest* request);

 protected:
  // Testing hook.
  void set_allow_null_profile_for_testing(bool allow_null_profile_for_testing);

 private:
  void InitializeScriptSet();

  void ReportIncidentOnUIThread(
      int render_process_id,
      int render_frame_id,
      scoped_ptr<ClientIncidentReport_IncidentData_ScriptRequestIncident>
          incident_data);

  scoped_ptr<IncidentReceiver> incident_receiver_;
  base::hash_set<std::string> script_set_;
  bool allow_null_profile_for_testing_;

  base::WeakPtrFactory<ScriptRequestDetector> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ScriptRequestDetector);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_SCRIPT_REQUEST_DETECTOR_H_
