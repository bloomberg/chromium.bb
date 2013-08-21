// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_ZYGOTE_FORK_DELEGATE_LINUX_H_
#define CONTENT_PUBLIC_COMMON_ZYGOTE_FORK_DELEGATE_LINUX_H_

#include <unistd.h>

#include <string>
#include <vector>

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

  // Delegate forks, returning a -1 on failure. Outside the
  // suid sandbox, Fork() returns the Linux process ID. Inside
  // the sandbox, returns a positive integer, with PID discovery
  // handled by the sandbox.
  virtual pid_t Fork(const std::vector<int>& fds) = 0;

  // After a successful for, signal the child to indicate that
  // the child's PID has been received. Also communicate the
  // channel switch as a part of acknowledgement message.
  virtual bool AckChild(int fd, const std::string& channel_switch) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_ZYGOTE_FORK_DELEGATE_LINUX_H_
