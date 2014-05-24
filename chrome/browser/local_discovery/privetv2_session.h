// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_PRIVETV2_SESSION_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_PRIVETV2_SESSION_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"

namespace local_discovery {

class PrivetHTTPClient;

class PrivetV2Session {
 public:
  typedef base::Callback<
      void(bool success, const base::DictionaryValue& response)>
      RequestCallback;

  class Delegate {
   public:
    typedef base::Callback<void(bool confirm)> ConfirmationCallback;

    virtual ~Delegate() {}

    virtual void OnSetupConfirmationNeeded(
        const std::string& confirmation_code,
        const ConfirmationCallback& callback) = 0;

    virtual void OnSessionEstablished() = 0;

    virtual void OnCannotEstablishSession() = 0;
  };

  class Request {
   public:
    virtual ~Request() {}

    virtual void Start() = 0;
  };

  virtual ~PrivetV2Session() {}

  static scoped_ptr<PrivetV2Session> Create(PrivetHTTPClient* client);

  // Establish a session, will call |OnSetupConfirmationNeeded| and then
  // |OnSessionEstablished|.
  virtual void Start() = 0;

  // Create a single /privet/v2/session/call request.
  virtual scoped_ptr<Request> CreateRequest(
      const char* api_name,
      const base::DictionaryValue& request,
      const RequestCallback& callback);
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_PRIVETV2_SESSION_H_
