// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_ID_H_
#define CHROME_BROWSER_SESSIONS_SESSION_ID_H_

#include "base/basictypes.h"

class Browser;

namespace content {
class WebContents;
}

// Uniquely identifies a tab or window for the duration of a session.
class SessionID {
 public:
  typedef int32 id_type;

  SessionID();
  ~SessionID() {}

  // This value is immutable for a given tab. It will be unique across Chrome
  // within the current session, but may be re-used across sessions. Returns -1
  // for NULL, and will never return -1 for a valid tab.
  static id_type IdForTab(const content::WebContents* tab);

  // If the tab has ever been attached to a window, this is the value of
  // Browser::session_id().id() for some Browser object. Returns -1 for NULL, or
  // another value that's not equal to any Browser's id for a tab that has never
  // been attached to a window. IdForWindowContainingTab() returns the old
  // window for a tab that's currently being dragged between windows.
  static id_type IdForWindowContainingTab(const content::WebContents* tab);

  // Returns the underlying id.
  void set_id(id_type id) { id_ = id; }
  id_type id() const { return id_; }

 private:
  id_type id_;
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_ID_H_
