// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process_platform_part_chromeos.h"

#include "chrome/browser/chromeos/memory/oom_priority_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"

BrowserProcessPlatformPart::BrowserProcessPlatformPart()
    : created_profile_helper_(false) {
}

BrowserProcessPlatformPart::~BrowserProcessPlatformPart() {
}

void BrowserProcessPlatformPart::StartTearDown() {
  profile_helper_.reset();
}

chromeos::OomPriorityManager*
    BrowserProcessPlatformPart::oom_priority_manager() {
  DCHECK(CalledOnValidThread());
  if (!oom_priority_manager_.get())
    oom_priority_manager_.reset(new chromeos::OomPriorityManager());
  return oom_priority_manager_.get();
}

chromeos::ProfileHelper* BrowserProcessPlatformPart::profile_helper() {
  DCHECK(CalledOnValidThread());
  if (!created_profile_helper_)
    CreateProfileHelper();
  return profile_helper_.get();
}

void BrowserProcessPlatformPart::CreateProfileHelper() {
  DCHECK(!created_profile_helper_ && profile_helper_.get() == NULL);
  created_profile_helper_ = true;
  profile_helper_.reset(new chromeos::ProfileHelper());
}
