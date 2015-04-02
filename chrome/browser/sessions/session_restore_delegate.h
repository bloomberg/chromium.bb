// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_RESTORE_DELEGATE_H_
#define CHROME_BROWSER_SESSIONS_SESSION_RESTORE_DELEGATE_H_

#include <vector>

#include "base/time/time.h"

namespace content {
class WebContents;
}

// SessionRestoreDelegate is responsible for creating the tab loader and the
// stats tracker.
class SessionRestoreDelegate {
 public:
  struct RestoredTab {
    content::WebContents* contents;
    bool is_active;
  };

  static void RestoreTabs(const std::vector<RestoredTab>& tabs,
                          const base::TimeTicks& restore_started);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SessionRestoreDelegate);
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_RESTORE_DELEGATE_H_
