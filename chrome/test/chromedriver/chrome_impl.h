// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_IMPL_H_

#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/process.h"
#include "chrome/test/chromedriver/chrome.h"

class Status;

class ChromeImpl : public Chrome {
 public:
  ChromeImpl(base::ProcessHandle process, base::ScopedTempDir* user_data_dir);
  virtual ~ChromeImpl();

  // Overridden from Chrome:
  virtual Status Quit() OVERRIDE;

 private:
  base::ProcessHandle process_;
  base::ScopedTempDir user_data_dir_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_IMPL_H_
