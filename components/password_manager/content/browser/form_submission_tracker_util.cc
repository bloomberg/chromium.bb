// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/content/browser/form_submission_tracker_util.h"

#include "base/logging.h"
#include "components/password_manager/core/browser/form_submission_observer.h"

namespace password_manager {

void NotifyDidNavigateMainFrame(bool is_renderer_initiated,
                                ui::PageTransition transition,
                                FormSubmissionObserver* observer) {
  DCHECK(observer);

  // Password manager isn't interested in
  // - user initiated navigations (e.g. click on the bookmark),
  // - hyperlink navigations.
  bool form_may_be_submitted =
      is_renderer_initiated &&
      !ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_LINK);

  observer->DidNavigateMainFrame(form_may_be_submitted);
}

}  // namespace password_manager
