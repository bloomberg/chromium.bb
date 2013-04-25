// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_CHROMEOS_H_
#define CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_CHROMEOS_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"

namespace chromeos {
class OomPriorityManager;
class ProfileHelper;
}

class BrowserProcessPlatformPart : public base::NonThreadSafe {
 public:
  BrowserProcessPlatformPart();
  virtual ~BrowserProcessPlatformPart();

  // Called from BrowserProcessImpl::StartTearDown().
  virtual void StartTearDown();

  // Returns the out-of-memory priority manager.
  virtual chromeos::OomPriorityManager* oom_priority_manager();

  // Returns the ProfileHelper instance that is used to identify
  // users and their profiles in Chrome OS multi user session.
  virtual chromeos::ProfileHelper* profile_helper();

 protected:
  virtual void CreateProfileHelper();

  bool created_profile_helper_;
  scoped_ptr<chromeos::ProfileHelper> profile_helper_;

 private:
  scoped_ptr<chromeos::OomPriorityManager> oom_priority_manager_;

  DISALLOW_COPY_AND_ASSIGN(BrowserProcessPlatformPart);
};

#endif  // CHROME_BROWSER_BROWSER_PROCESS_PLATFORM_PART_CHROMEOS_H_
