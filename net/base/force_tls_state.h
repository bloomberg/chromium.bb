// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_FORCE_TLS_STATE_H_
#define NET_BASE_FORCE_TLS_STATE_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/lock.h"
#include "base/ref_counted.h"
#include "base/time.h"

class GURL;

namespace net {

// ForceTLSState
//
// Tracks which hosts have enabled ForceTLS.  After a host enables ForceTLS,
// then we refuse to talk to the host over HTTP, treat all certificate errors as
// fatal, and refuse to load any mixed content.
//
class ForceTLSState : public base::RefCountedThreadSafe<ForceTLSState> {
 public:
  ForceTLSState();

  // Called when we see an X-Force-TLS header that we should process.  Modifies
  // our state as instructed by the header.
  void DidReceiveHeader(const GURL& url, const std::string& value);

  // Enable ForceTLS for |host|.
  void EnableHost(const std::string& host, base::Time expiry,
                  bool include_subdomains);

  // Returns whether |host| has had ForceTLS enabled.
  bool IsEnabledForHost(const std::string& host);

  // Returns |true| if |value| parses as a valid X-Force-TLS header value.
  // The values of max-age and and includeSubDomains are returned in |max_age|
  // and |include_subdomains|, respectively.  The out parameters are not
  // modified if the function returns |false|.
  static bool ParseHeader(const std::string& value,
                          int* max_age,
                          bool* include_subdomains);

  struct State {
    base::Time expiry;  // the absolute time (UTC) when this record expires
    bool include_subdomains;  // subdomains included?
  };

  class Delegate {
   public:
    // This function may not block and may be called with internal locks held.
    // Thus it must not reenter the ForceTLSState object.
    virtual void StateIsDirty(ForceTLSState* state) = 0;
  };

  void SetDelegate(Delegate*);

  bool Serialise(std::string* output);
  bool Deserialise(const std::string& state);

 private:
  // If we have a callback configured, call it to let our serialiser know that
  // our state is dirty.
  void DirtyNotify();

  // The set of hosts that have enabled ForceTLS.
  std::map<std::string, State> enabled_hosts_;

  // Protect access to our data members with this lock.
  Lock lock_;

  // Our delegate who gets notified when we are dirtied, or NULL.
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ForceTLSState);
};

}  // namespace net

#endif  // NET_BASE_FORCE_TLS_STATE_H_
