// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/crash_analysis_win.h"

#include "breakpad/src/client/windows/handler/exception_handler.h"
#include "chrome/app/breakpad_win.h"

extern "C" void DumpProcessAbnormalSignature();

void CrashAnalysis::Analyze(_EXCEPTION_POINTERS* info) {
  // TODO(cdn): Perform analysis and determine whether the process
  // state is abnormal.
  if (false)
    DumpProcessAbnormalSignature();

  return;
}
