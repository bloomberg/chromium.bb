// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/page_transition_util.h"

namespace arc {

bool ShouldIgnoreNavigation(ui::PageTransition page_transition,
                            bool allow_form_submit) {
  // Mask out server-sided redirects only.
  page_transition = ui::PageTransitionFromInt(
      page_transition & ~ui::PAGE_TRANSITION_SERVER_REDIRECT);

  if (!ui::PageTransitionCoreTypeIs(page_transition,
                                    ui::PAGE_TRANSITION_LINK) &&
      !(allow_form_submit &&
        ui::PageTransitionCoreTypeIs(page_transition,
                                     ui::PAGE_TRANSITION_FORM_SUBMIT))) {
    // Do not handle the |url| if this event wasn't spawned by the user clicking
    // on a link.
    return true;
  }

  if (ui::PageTransitionGetQualifier(page_transition) != 0) {
    // Qualifiers indicate that this navigation was the result of a click on a
    // forward/back button, or typing in the URL bar, or a client-side redirect,
    // etc.  Don't handle any of those types of navigations.
    return true;
  }

  return false;
}

}  // namespace arc
