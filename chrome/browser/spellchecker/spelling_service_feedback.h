// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPELLCHECKER_SPELLING_SERVICE_FEEDBACK_H_
#define CHROME_BROWSER_SPELLCHECKER_SPELLING_SERVICE_FEEDBACK_H_

#include <vector>

#include "base/timer.h"

// Manages sending feedback to the spelling service.
class SpellingServiceFeedback {
 public:
  SpellingServiceFeedback();
  ~SpellingServiceFeedback();

  // Receives document markers for renderer with process ID |render_process_id|.
  // Called when the renderer responds to RequestDocumentMarkers() call.
  void OnReceiveDocumentMarkers(
      int render_process_id,
      const std::vector<uint32>& markers) const;

 private:
  // Requests the document markers from all of the renderers. Called
  // periodically when |timer_| fires.
  void RequestDocumentMarkers();

  // A timer to periodically request a list of document spelling markers from
  // all of the renderers. The timer runs while an instance of this class is
  // alive.
  base::RepeatingTimer<SpellingServiceFeedback> timer_;

  DISALLOW_COPY_AND_ASSIGN(SpellingServiceFeedback);
};

#endif  // CHROME_BROWSER_SPELLCHECKER_SPELLING_SERVICE_FEEDBACK_H_
