// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_H_
#define CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_H_

#include "base/basictypes.h"

class BrowserProcessPlatformPart {
 public:
  BrowserProcessPlatformPart();
  virtual ~BrowserProcessPlatformPart();

  // Called from BrowserProcessImpl::StartTearDown().
  virtual void StartTearDown();

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserProcessPlatformPart);
};

#endif  // CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_H_
