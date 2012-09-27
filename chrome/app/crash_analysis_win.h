// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_CRASH_ANALYSIS_WIN_H_
#define CHROME_APP_CRASH_ANALYSIS_WIN_H_

struct _EXCEPTION_POINTERS;

class CrashAnalysis {
 public:
  void Analyze(_EXCEPTION_POINTERS* info);
};

#endif  // CHROME_APP_CRASH_ANALYSIS_WIN_H_
