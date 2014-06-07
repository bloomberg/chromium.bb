// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_PRIVETV3_SESSION_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_PRIVETV3_SESSION_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class DictionaryValue;
}

namespace local_discovery {

class PrivetHTTPClient;

// Manages secure communication between browser and local Privet device.
class PrivetV3Session {
 public:
  typedef base::Callback<
      void(bool success, const base::DictionaryValue& response)>
      RequestCallback;

  // Delegate to be implemented by client code.
  class Delegate {
   public:
    typedef base::Callback<void(bool confirm)> ConfirmationCallback;

    virtual ~Delegate();

    // Called when client code should prompt user to check |confirmation_code|.
    virtual void OnSetupConfirmationNeeded(
        const std::string& confirmation_code,
        const ConfirmationCallback& callback) = 0;

    // Called when session successfully establish and client code my call
    // |CreateRequest| method.
    virtual void OnSessionEstablished() = 0;

    // Called when session setup fails.
    virtual void OnCannotEstablishSession() = 0;
  };

  // Represents request in progress using secure session.
  class Request {
   public:
    virtual ~Request();
    virtual void Start() = 0;
  };

  PrivetV3Session(scoped_ptr<PrivetHTTPClient> client, Delegate* delegate);
  ~PrivetV3Session();

  // Establishes a session, will call |OnSetupConfirmationNeeded| and then
  // |OnSessionEstablished|.
  void Start();

  // Create a single /privet/v3/session/call request.
  // Must be called only after receiving |OnSessionEstablished|.
  scoped_ptr<Request> CreateRequest(const std::string& api_name,
                                    const base::DictionaryValue& request,
                                    const RequestCallback& callback);

 private:
  DISALLOW_COPY_AND_ASSIGN(PrivetV3Session);
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_PRIVETV3_SESSION_H_
