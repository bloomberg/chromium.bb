// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// StrictTransportSecurityState maintains an in memory database containing the
// list of hosts that currently have strict transport security enabled. This
// singleton object deals with writing that data out to disk as needed and
// loading it at startup.

// At startup we need to load the strict transport security state from the
// disk. For the moment, we don't want to delay startup for this load, so we
// let the StrictTransportSecurityState run for a while without being loaded.
// This means that it's possible for pages opened very quickly not to get the
// correct strict transport security information.
//
// To load the state, we schedule a Task on the file thread which loads,
// deserialises and configures the StrictTransportSecurityState.
//
// The StrictTransportSecurityState object supports running a callback function
// when it changes. This object registers the callback, pointing at itself.
//
// StrictTransportSecurityState calls...
// StrictTransportSecurityPersister::StateIsDirty
//   since the callback isn't allowed to block or reenter, we schedule a Task
//   on the file thread after some small amount of time
//
// ...
//
// StrictTransportSecurityPersister::SerialiseState
//   copies the current state of the StrictTransportSecurityState, serialises
//   and writes to disk.

#ifndef CHROME_BROWSER_STRICT_TRANSPORT_SECURITY_PERSISTER_H_
#define CHROME_BROWSER_STRICT_TRANSPORT_SECURITY_PERSISTER_H_

#include "base/file_path.h"
#include "base/lock.h"
#include "base/ref_counted.h"
#include "net/base/strict_transport_security_state.h"

class StrictTransportSecurityPersister
    : public base::RefCountedThreadSafe<StrictTransportSecurityPersister>,
      public net::StrictTransportSecurityState::Delegate {
 public:
  StrictTransportSecurityPersister();
  void Initialize(net::StrictTransportSecurityState* state,
                  const FilePath& profile_path);

  // Called by the StrictTransportSecurityState when it changes its state.
  virtual void StateIsDirty(net::StrictTransportSecurityState*);

 private:
  friend class base::RefCountedThreadSafe<StrictTransportSecurityPersister>;

  ~StrictTransportSecurityPersister();

  // a Task callback for when the state needs to be written out.
  void SerialiseState();

  // a Task callback for when the state needs to be loaded from disk at startup.
  void LoadState();

  Lock lock_;  // protects all the members

  // true when the state object has signaled that we're dirty and we haven't
  // serialised the state yet.
  bool state_is_dirty_;

  scoped_refptr<net::StrictTransportSecurityState>
      strict_transport_security_state_;
  // The path to the file in which we store the serialised state.
  FilePath state_file_;
};

#endif  // CHROME_BROWSER_STRICT_TRANSPORT_SECURITY_PERSISTER_H_
