// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_restore.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_types.h"
#include "webkit/glue/window_open_disposition.h"

// static
void SessionRestore::RestoreForeignSessionTab(
    content::WebContents* source_web_contents,,
    const SessionTab& session_tab,
    WindowOpenDisposition disposition) {
  NOTIMPLEMENTED() << "TODO(yfriedman): Upstream this.";
}

// static
void SessionRestore::RestoreForeignSessionWindows(
    Profile* profile,
    std::vector<const SessionWindow*>::const_iterator begin,
    std::vector<const SessionWindow*>::const_iterator end) {
  NOTREACHED();
}
