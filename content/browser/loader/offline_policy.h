// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_OFFLINE_POLICY
#define CONTENT_BROWSER_LOADER_OFFLINE_POLICY

#include <map>

#include "base/basictypes.h"
#include "content/common/content_export.h"

struct ResourceHostMsg_Request;

namespace net {
class HttpResponseInfo;
class URLRequest;
}

namespace content {

// This class controls under what conditions resources will be fetched
// from cache even if stale rather than from the network.  For example,
// one policy would be that if requests for a particular route (e.g. "tab")
// is unable to reach the server, other requests made with the same route
// can be loaded from cache without first requiring a network timeout.
//
// There is a single OfflinePolicy object per user navigation unit
// (generally a tab).
class CONTENT_EXPORT OfflinePolicy {
 public:
  OfflinePolicy();
  ~OfflinePolicy();

  // Return any additional load flags to be ORed for a request from
  // this route with the given |resource_type|.  |reset_state| indicates
  // that this request should reinitialized the internal state for this
  // policy object (e.g. in the case of a main frame load).
  int GetAdditionalLoadFlags(int current_flags, bool reset_state);

  // Incorporate online/offline information from a successfully started request.
  void UpdateStateForSuccessfullyStartedRequest(
      const net::HttpResponseInfo& response_info);

private:
  enum State { INIT, ONLINE, OFFLINE };

  void RecordAndResetStats();

  bool enabled_;
  State state_;
  int resource_loads_initiated_;
  int resource_loads_successfully_started_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePolicy);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_OFFLINE_POLICY
