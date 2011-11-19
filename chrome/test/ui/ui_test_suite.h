// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_UI_UI_TEST_SUITE_H_
#define CHROME_TEST_UI_UI_TEST_SUITE_H_
#pragma once

#include "base/process.h"
#include "chrome/test/base/chrome_test_suite.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif

class UITestSuite : public ChromeTestSuite {
 public:
  UITestSuite(int argc, char** argv);

 protected:
  virtual void Initialize() OVERRIDE;
  virtual void Shutdown() OVERRIDE;

 private:
#if defined(OS_WIN)
  void LoadCrashService();

  base::ProcessHandle crash_service_;

  // JobObject used to clean up orphaned child processes.
  base::win::ScopedHandle job_handle_;
#endif
};

#endif  // CHROME_TEST_UI_UI_TEST_SUITE_H_
