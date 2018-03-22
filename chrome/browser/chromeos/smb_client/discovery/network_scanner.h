// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SMB_CLIENT_DISCOVERY_NETWORK_SCANNER_H_
#define CHROME_BROWSER_CHROMEOS_SMB_CLIENT_DISCOVERY_NETWORK_SCANNER_H_

#include <map>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/smb_client/discovery/host_locator.h"

namespace chromeos {
namespace smb_client {

// Holds the number of in-flight requests and the callback to call once all the
// HostLocators are finished. Also holds the hosts found from the HostLocators
// that have already returned.
struct RequestInfo {
  uint32_t remaining_requests;
  FindHostsCallback callback;
  HostMap hosts_found;

  RequestInfo(uint32_t remaining_requests, FindHostsCallback callback);
  RequestInfo(RequestInfo&& other);
  ~RequestInfo();

  DISALLOW_COPY_AND_ASSIGN(RequestInfo);
};

// NetworkScanner discovers SMB hosts in the local network by querying
// registered HostLocators and aggregating their results.
class NetworkScanner : public base::SupportsWeakPtr<NetworkScanner> {
 public:
  NetworkScanner();
  ~NetworkScanner();

  // Query the registered HostLocators and return all the hosts found.
  // |callback| is called once all the HostLocators have responded with their
  // results. If there are no locators, the callback is fired immediately with
  // an empty result and success set to false.
  void FindHostsInNetwork(FindHostsCallback callback);

  // Registeres a |locator| to be queried when FindHostsInNetwork() is called.
  void RegisterHostLocator(std::unique_ptr<HostLocator> locator);

 private:
  // Callback handler for HostLocator::FindHosts().
  void OnHostsFound(uint32_t request_id, bool success, const HostMap& host_map);

  // Adds |host_map| hosts to current results. The host will not be added if the
  // hostname already exists in results, and if the IP address does not match,
  // it will be logged.
  void AddHostsToResults(uint32_t request_id, const HostMap& host_map);

  // Adds a new request to track and saves |callback| to be called when the
  // request is finished. Returns the request id.
  uint32_t AddNewRequest(FindHostsCallback callback);

  // Called after a HostLocator returns with results and decrements the count of
  // requests in RequestInfo for |request_id|. Fires the callback for if
  // there are no more requests and deletes the corresponding RequestInfo.
  void FireCallbackIfFinished(uint32_t request_id);

  std::vector<std::unique_ptr<HostLocator>> locators_;

  // Used for tracking in-flight requests to HostLocators. The key is the
  // request id, and the value is the RequestInfo struct.
  std::map<uint32_t, RequestInfo> requests_;

  uint32_t next_request_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(NetworkScanner);
};

}  // namespace smb_client
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SMB_CLIENT_DISCOVERY_NETWORK_SCANNER_H_
