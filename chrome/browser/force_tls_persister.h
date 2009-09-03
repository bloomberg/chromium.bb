// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ForceTLSState maintains an in memory database containing the list of hosts
// that currently have ForceTLS enabled. This singleton object deals with
// writing that data out to disk as needed and loading it at startup.

// At startup we need to load the ForceTLS state from the disk. For the moment,
// we don't want to delay startup for this load, so we let the ForceTLSState
// run for a while without being loaded. This means that it's possible for
// pages opened very quickly not to get the correct ForceTLS information.
//
// To load the state, we schedule a Task on the file thread which loads,
// deserialises and configures the ForceTLSState.
//
// The ForceTLSState object supports running a callback function when it
// changes. This object registers the callback, pointing at itself.
//
// ForceTLSState calls...
// ForceTLSPersister::StateIsDirty
//   since the callback isn't allowed to block or reenter, we schedule a Task
//   on |file_thread_| after some small amount of time
//
// ...
//
// ForceTLSPersister::SerialiseState
//   copies the current state of the ForceTLSState, serialises and writes to
//   disk.

#include "base/lock.h"
#include "base/ref_counted.h"
#include "net/base/force_tls_state.h"

namespace base {
class Thread;
}

class ForceTLSPersister : public base::RefCountedThreadSafe<ForceTLSPersister>,
                          public net::ForceTLSState::Delegate {
 public:
  ForceTLSPersister(net::ForceTLSState* state, base::Thread* file_thread);

  // Called by the ForceTLSState when it changes its state.
  virtual void StateIsDirty(net::ForceTLSState*);

 private:
  // a Task callback for when the state needs to be written out.
  void SerialiseState();

  // a Task callback for when the state needs to be loaded from disk at startup.
  void LoadState();

  Lock lock_;  // protects all the members

  // true when the state object has signaled that we're dirty and we haven't
  // serialised the state yet.
  bool state_is_dirty_;

  scoped_refptr<net::ForceTLSState> force_tls_state_;

  // This is a thread which can perform file access.
  base::Thread* const file_thread_;
};
