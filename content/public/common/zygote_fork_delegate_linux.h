// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_ZYGOTE_FORK_DELEGATE_LINUX_H_
#define CONTENT_PUBLIC_COMMON_ZYGOTE_FORK_DELEGATE_LINUX_H_

#include <unistd.h>

#include <string>
#include <vector>

// TODO(jln) base::TerminationStatus should be forward declared when switching
// to C++11.
#include "base/process/kill.h"

namespace content {

// The ZygoteForkDelegate allows the Chrome Linux zygote to delegate
// fork operations to another class that knows how to do some
// specialized version of fork.
class ZygoteForkDelegate {
 public:
  // A ZygoteForkDelegate is created during Chrome linux zygote
  // initialization, and provides "fork()" functionality as an
  // alternative to forking the zygote.  A new delegate is passed in
  // as an argument to ZygoteMain().
  virtual ~ZygoteForkDelegate() {}

  // Initialization happens in the zygote after it has been
  // started by ZygoteMain.
  virtual void Init(int sandboxdesc) = 0;

  // After Init, supply a UMA_HISTOGRAM_ENUMERATION the delegate
  // would like to supply on the first fork.
  virtual void InitialUMA(std::string* uma_name,
                          int* uma_sample,
                          int* uma_boundary_value) = 0;

  // Returns 'true' if the delegate would like to handle a given fork
  // request.  Otherwise returns false.  Optionally, fills in uma_name et al
  // with a report the helper wants to make via UMA_HISTOGRAM_ENUMERATION.
  virtual bool CanHelp(const std::string& process_type, std::string* uma_name,
                       int* uma_sample, int* uma_boundary_value) = 0;

  // Indexes of FDs in the vector passed to Fork().
  enum {
    // Used to pass in the descriptor for talking to the Browser
    kBrowserFDIndex,
    // The next two are used in the protocol for discovering the
    // child processes real PID from within the SUID sandbox. See
    // http://code.google.com/p/chromium/wiki/LinuxZygote
    kDummyFDIndex,
    kParentFDIndex,
    kNumPassedFDs  // Number of FDs in the vector passed to Fork().
  };

  // Delegate forks, returning a -1 on failure. Outside the
  // suid sandbox, Fork() returns the Linux process ID.
  // This method is not aware of any potential pid namespaces, so it'll
  // return a raw pid just like fork() would.
  virtual pid_t Fork(const std::string& process_type,
                     const std::vector<int>& fds) = 0;

  // After a successful fork, signal the child to indicate that
  // the child's PID has been received. Also communicate the
  // channel switch as a part of acknowledgement message.
  virtual bool AckChild(int fd, const std::string& channel_switch) = 0;

  // The fork delegate must also assume the role of waiting for its children
  // since the caller will not be their parents and cannot do it. |pid| here
  // should be a pid that has been returned by the Fork() method. i.e. This
  // method is completely unaware of eventual PID namespaces due to sandboxing.
  // |known_dead| indicates that the process is already dead and that a
  // blocking wait() should be performed. In this case, GetTerminationStatus()
  // will send a SIGKILL to the target process first.
  virtual bool GetTerminationStatus(pid_t pid, bool known_dead,
                                    base::TerminationStatus* status,
                                    int* exit_code) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_ZYGOTE_FORK_DELEGATE_LINUX_H_
