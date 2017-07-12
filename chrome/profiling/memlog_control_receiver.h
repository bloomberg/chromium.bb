// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_MEMLOG_CONTROL_RECEIVER_H_
#define CHROME_PROFILING_MEMLOG_CONTROL_RECEIVER_H_

namespace profiling {

class MemlogControlReceiver {
 public:
  virtual ~MemlogControlReceiver() {}

  virtual void OnStartMojoControl() = 0;
};

}  // namespace profiling

#endif  // CHROME_PROFILING_MEMLOG_CONTROL_RECEIVER_H_
