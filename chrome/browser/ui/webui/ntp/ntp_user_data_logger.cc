// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/ntp_user_data_logger.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/search/search.h"
#include "content/public/browser/navigation_details.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(NTPUserDataLogger);

NTPUserDataLogger::NTPUserDataLogger(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      number_of_mouseovers_(0) {
}

NTPUserDataLogger::~NTPUserDataLogger() {}

void NTPUserDataLogger::increment_number_of_mouseovers() {
  number_of_mouseovers_++;
}

void NTPUserDataLogger::EmitMouseoverCount() {
  UMA_HISTOGRAM_COUNTS("NewTabPage.NumberOfMouseOvers",
                       number_of_mouseovers_);
  number_of_mouseovers_ = 0;
}

// content::WebContentsObserver override
void NTPUserDataLogger::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  if (!load_details.previous_url.is_valid())
    return;

  if (chrome::MatchesOriginAndPath(ntp_url_, load_details.previous_url))
    EmitMouseoverCount();
}
