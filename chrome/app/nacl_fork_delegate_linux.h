// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_NACL_FORK_DELEGATE_LINUX_H_
#define CHROME_APP_NACL_FORK_DELEGATE_LINUX_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/public/common/zygote_fork_delegate_linux.h"

// The NaClForkDelegate is created during Chrome linux zygote
// initialization, and provides "fork()" functionality with
// NaCl specific process characteristics (specifically address
// space layout) as an alternative to forking the zygote.
// A new delegate is passed in as an argument to ZygoteMain().
class NaClForkDelegate : public content::ZygoteForkDelegate {
 public:
  NaClForkDelegate();
  virtual ~NaClForkDelegate();

  virtual void Init(int sandboxdesc) OVERRIDE;
  virtual void InitialUMA(std::string* uma_name,
                          int* uma_sample,
                          int* uma_boundary_value) OVERRIDE;
  virtual bool CanHelp(const std::string& process_type, std::string* uma_name,
                          int* uma_sample, int* uma_boundary_value) OVERRIDE;
  virtual pid_t Fork(const std::vector<int>& fds) OVERRIDE;
  virtual bool AckChild(int fd,
                        const std::string& channel_switch) OVERRIDE;

 private:
  // These values are reported via UMA and hence they become permanent
  // constants.  Old values cannot be reused, only new ones added.
  enum NaClHelperStatus {
    kNaClHelperUnused = 0,
    kNaClHelperMissing = 1,
    kNaClHelperBootstrapMissing = 2,
    kNaClHelperValgrind = 3,
    kNaClHelperLaunchFailed = 4,
    kNaClHelperAckFailed = 5,
    kNaClHelperSuccess = 6,
    kNaClHelperStatusBoundary  // Must be one greater than highest value used.
  };

  NaClHelperStatus status_;
  int fd_;
};

#endif  // CHROME_APP_NACL_FORK_DELEGATE_LINUX_H_
