// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A helper class that assists preferences in firing notifications when lists
// are changed.

#ifndef CHROME_BROWSER_SCOPED_PREF_UPDATE_H_
#define CHROME_BROWSER_SCOPED_PREF_UPDATE_H_

#include "chrome/browser/pref_service.h"

class ScopedPrefUpdate {
 public:
  ScopedPrefUpdate(PrefService* service, const wchar_t* path);
  ~ScopedPrefUpdate();

 private:
  PrefService* service_;
  std::wstring path_;
};

#endif
