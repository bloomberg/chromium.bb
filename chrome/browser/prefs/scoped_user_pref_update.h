// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A helper class that assists preferences in firing notifications when lists
// are changed.

#ifndef CHROME_BROWSER_PREFS_SCOPED_USER_PREF_UPDATE_H_
#define CHROME_BROWSER_PREFS_SCOPED_USER_PREF_UPDATE_H_
#pragma once

#include "chrome/browser/prefs/pref_service.h"

class ScopedUserPrefUpdate {
 public:
  ScopedUserPrefUpdate(PrefService* service, const char* path);
  ~ScopedUserPrefUpdate();

 private:
  PrefService* service_;
  std::string path_;
};

#endif  // CHROME_BROWSER_PREFS_SCOPED_USER_PREF_UPDATE_H_
