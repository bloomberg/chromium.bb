// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROME_TEST_SUITE_H_
#define CHROME_TEST_BASE_CHROME_TEST_SUITE_H_

#include <string>

#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/test/content_test_suite_base.h"

#if defined(OS_WIN)
#include "ui/base/win/scoped_ole_initializer.h"
#endif

namespace base {
class StatsTable;
}

class ChromeTestSuite : public content::ContentTestSuiteBase {
 public:
  ChromeTestSuite(int argc, char** argv);
  virtual ~ChromeTestSuite();

 protected:
  virtual void Initialize() OVERRIDE;
  virtual void Shutdown() OVERRIDE;

  virtual content::ContentClient* CreateClientForInitialization() OVERRIDE;

  void SetBrowserDirectory(const FilePath& browser_dir) {
    browser_dir_ = browser_dir;
  }

  // Alternative path to browser binaries.
  FilePath browser_dir_;

  std::string stats_filename_;
  scoped_ptr<base::StatsTable> stats_table_;

#if defined(OS_WIN)
  ui::ScopedOleInitializer ole_initializer_;
#endif
};

#endif  // CHROME_TEST_BASE_CHROME_TEST_SUITE_H_
