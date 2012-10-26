// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_CRASH_ANALYSIS_WIN_H_
#define CHROME_APP_CRASH_ANALYSIS_WIN_H_

#include <windows.h>

#include <vector>

struct _EXCEPTION_POINTERS;

class CrashAnalysis {
 public:
  CrashAnalysis();
  void Analyze(_EXCEPTION_POINTERS* info);

 private:
  std::vector<MEMORY_BASIC_INFORMATION> memory_regions_;
  size_t exec_pages_;
};

#endif  // CHROME_APP_CRASH_ANALYSIS_WIN_H_
