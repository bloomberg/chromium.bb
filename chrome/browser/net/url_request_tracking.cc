// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/url_request_tracking.h"

#include "base/basictypes.h"
#include "net/url_request/url_request.h"

namespace {

// The value is not important, this address is used as the unique key for the
// PID.
const void* kOriginProcessUniqueIDKey = 0;

class UniqueIDData : public net::URLRequest::UserData {
 public:
  explicit UniqueIDData(int id) : id_(id) {}
  virtual ~UniqueIDData() {}

  int id() const { return id_; }
  void set_id(int id) { id_ = id; }

 private:
  int id_;

  DISALLOW_COPY_AND_ASSIGN(UniqueIDData);
};

}  // namespace

namespace chrome_browser_net {

void SetOriginProcessUniqueIDForRequest(int id, net::URLRequest* request) {
  // The request will take ownership.
  request->SetUserData(&kOriginProcessUniqueIDKey, new UniqueIDData(id));
}

int GetOriginProcessUniqueIDForRequest(const net::URLRequest* request) {
  const UniqueIDData* data = static_cast<const UniqueIDData*>(
      request->GetUserData(&kOriginProcessUniqueIDKey));
  if (!data)
    return -1;
  return data->id();
}

}  // namespace chrome_browser_net
