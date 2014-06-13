// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_ADD_INCIDENT_CALLBACK_H_
#define CHROME_BROWSER_SAFE_BROWSING_ADD_INCIDENT_CALLBACK_H_

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"

namespace safe_browsing {

class ClientIncidentReport_IncidentData;

// A callback used by external components to add an incident to the incident
// reporting service.
typedef base::Callback<void(scoped_ptr<ClientIncidentReport_IncidentData>)>
    AddIncidentCallback;

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_ADD_INCIDENT_CALLBACK_H_
