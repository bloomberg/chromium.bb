// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_UTILS_H_
#define CHROME_BROWSER_SESSIONS_SESSION_UTILS_H_
#pragma once

#include "chrome/browser/sessions/tab_restore_service.h"

class SessionUtils {
 public:
  // Fill the passed list from the entries passed as argument filtering out the
  // ones with the same title and URL as a previous entry. This avoid filling
  // the list with things that look like duplicates.
  //
  // The |filtering_entries| returned are simply references to the one passed in
  // without copy: There is no transfer of ownership.
  static void FilteredEntries(const TabRestoreService::Entries& entries,
                              TabRestoreService::Entries* filtered_entries);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SessionUtils);
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_UTILS_H_
