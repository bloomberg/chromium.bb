// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_PROFILING_MAIN_H_
#define CHROME_PROFILING_PROFILING_MAIN_H_

namespace base {
class CommandLine;
}

namespace profiling {

int ProfilingMain(const base::CommandLine& cmdline);

}  // namespace profiling

#endif  // CHROME_PROFILING_PROFILING_MAIN_H_
