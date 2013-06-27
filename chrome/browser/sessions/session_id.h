// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_ID_H_
#define CHROME_BROWSER_SESSIONS_SESSION_ID_H_

#include "base/basictypes.h"

namespace content {
class WebContents;
}

// Uniquely identifies a tab or window for the duration of a session.
class SessionID {
 public:
  typedef int32 id_type;

  SessionID();
  ~SessionID() {}

  // If the specified WebContents has a SessionTabHelper (probably because it
  // was used as the contents of a tab), returns a tab id. This value is
  // immutable for a given tab. It will be unique across Chrome within the
  // current session, but may be re-used across sessions. Returns -1
  // for a NULL WebContents or if the WebContents has no SessionTabHelper.
  static id_type IdForTab(const content::WebContents* tab);

  // If the specified WebContents has a SessionTabHelper (probably because it
  // was used as the contents of a tab), and has ever been attached to a Browser
  // window, returns Browser::session_id().id() for that Browser. If the tab is
  // being dragged between Browser windows, returns the old window's id value.
  // If the WebContents has a SessionTabHelper but has never been attached to a
  // Browser window, returns an id value that is different from that of any
  // Browser. Returns -1 for a NULL WebContents or if the WebContents has no
  // SessionTabHelper.
  static id_type IdForWindowContainingTab(const content::WebContents* tab);

  // Returns the underlying id.
  void set_id(id_type id) { id_ = id; }
  id_type id() const { return id_; }

 private:
  id_type id_;
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_ID_H_
