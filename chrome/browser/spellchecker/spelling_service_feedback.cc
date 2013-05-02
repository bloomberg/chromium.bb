// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spelling_service_feedback.h"

#include "chrome/common/spellcheck_common.h"
#include "chrome/common/spellcheck_messages.h"
#include "content/public/browser/render_process_host.h"

SpellingServiceFeedback::SpellingServiceFeedback() {
  timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(
          chrome::spellcheck_common::kFeedbackIntervalSeconds),
      this,
      &SpellingServiceFeedback::RequestDocumentMarkers);
}

SpellingServiceFeedback::~SpellingServiceFeedback() {
}

void SpellingServiceFeedback::OnReceiveDocumentMarkers(
    int render_process_id,
    const std::vector<uint32>& markers) const {
}

void SpellingServiceFeedback::RequestDocumentMarkers() {
  for (content::RenderProcessHost::iterator i(
          content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd();
       i.Advance()) {
    i.GetCurrentValue()->Send(new SpellCheckMsg_RequestDocumentMarkers());
  }
}
