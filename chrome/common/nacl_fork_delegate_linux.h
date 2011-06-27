// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NACL_FORK_DELEGATE_LINUX_H_
#define CHROME_COMMON_NACL_FORK_DELEGATE_LINUX_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/common/zygote_fork_delegate_linux.h"

// The NaClForkDelegate is created during Chrome linux zygote
// initialization, and provides "fork()" functionality with
// NaCl specific process characteristics (specifically address
// space layout) as an alternative to forking the zygote.
// A new delegate is passed in as an argument to ZygoteMain().
class NaClForkDelegate : public ZygoteForkDelegate {
 public:
  NaClForkDelegate();
  ~NaClForkDelegate();

  virtual void Init(bool sandboxed,
                    int browserdesc,
                    int sandboxdesc) OVERRIDE;
  virtual bool CanHelp(const std::string& process_type) OVERRIDE;
  virtual pid_t Fork(const std::vector<int>& fds) OVERRIDE;
  virtual bool AckChild(int fd,
                        const std::string& channel_switch) OVERRIDE;

 private:
  bool sandboxed_;
  int fd_;
  pid_t pid_;
};

#endif  // CHROME_COMMON_NACL_FORK_DELEGATE_LINUX_H_
