// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/url_request_tracking.h"

#include "base/basictypes.h"
#include "net/url_request/url_request.h"

namespace {

// The value is not important, this address is used as the unique key for the
// PID.
const void* kOriginPidKey = 0;

class OriginPidData : public net::URLRequest::UserData {
 public:
  explicit OriginPidData(int pid) : pid_(pid) {}
  virtual ~OriginPidData() {}

  int pid() const { return pid_; }
  void set_pid(int pid) { pid_ = pid; }

 private:
  int pid_;

  DISALLOW_COPY_AND_ASSIGN(OriginPidData);
};

}  // namespace

namespace chrome_browser_net {

void SetOriginPIDForRequest(int pid, net::URLRequest* request) {
  // The request will take ownership.
  request->SetUserData(&kOriginPidKey, new OriginPidData(pid));
}

int GetOriginPIDForRequest(const net::URLRequest* request) {
  const OriginPidData* data = static_cast<const OriginPidData*>(
      request->GetUserData(&kOriginPidKey));
  if (!data)
    return 0;
  return data->pid();
}

}  // namespace chrome_browser_net
