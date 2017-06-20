// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PROFILING_MEMLOG_SENDER_H_
#define CHROME_COMMON_PROFILING_MEMLOG_SENDER_H_

namespace base {

class CommandLine;

}  // namespace base

namespace profiling {

void InitMemlogSenderIfNecessary(const base::CommandLine& cmdline);

}  // namespace profiling

#endif  // CHROME_COMMON_PROFILING_MEMLOG_SENDER_H_
