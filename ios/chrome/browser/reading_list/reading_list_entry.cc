// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/reading_list_entry.h"

#include "base/memory/ptr_util.h"

// The backoff time is the following: 10min, 10min, 1h, 2h, 2h..., starting
// after the first failure.
const net::BackoffEntry::Policy ReadingListEntry::kBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    2,

    // Initial delay for exponential back-off in ms.
    10 * 60 * 1000,  // 10 minutes.

    // Factor by which the waiting time will be multiplied.
    6,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.1,  // 10%.

    // Maximum amount of time we are willing to delay our request in ms.
    2 * 3600 * 1000,  // 2 hours.

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    true,  // Don't use initial delay unless the last request was an error.
};

ReadingListEntry::ReadingListEntry(const GURL& url, const std::string& title)
    : ReadingListEntry(url, title, nullptr){};

ReadingListEntry::ReadingListEntry(const GURL& url,
                                   const std::string& title,
                                   std::unique_ptr<net::BackoffEntry> backoff)
    : url_(url),
      title_(title),
      distilled_state_(WAITING),
      failed_download_counter_(0) {
  if (backoff) {
    backoff_ = std::move(backoff);
  } else {
    backoff_ = base::MakeUnique<net::BackoffEntry>(&kBackoffPolicy);
  }
  DCHECK(!url.is_empty());
  DCHECK(url.is_valid());
}

ReadingListEntry::ReadingListEntry(ReadingListEntry&& entry)
    : url_(std::move(entry.url_)),
      title_(std::move(entry.title_)),
      distilled_url_(std::move(entry.distilled_url_)),
      distilled_state_(std::move(entry.distilled_state_)),
      backoff_(std::move(entry.backoff_)),
      failed_download_counter_(std::move(entry.failed_download_counter_)) {}

ReadingListEntry::~ReadingListEntry() {}

const GURL& ReadingListEntry::URL() const {
  return url_;
}

const std::string& ReadingListEntry::Title() const {
  return title_;
}

ReadingListEntry::DistillationState ReadingListEntry::DistilledState() const {
  return distilled_state_;
}

const GURL& ReadingListEntry::DistilledURL() const {
  return distilled_url_;
}

base::TimeDelta ReadingListEntry::TimeUntilNextTry() const {
  return backoff_->GetTimeUntilRelease();
}

int ReadingListEntry::FailedDownloadCounter() const {
  return failed_download_counter_;
}

ReadingListEntry& ReadingListEntry::operator=(ReadingListEntry&& other) {
  url_ = std::move(other.url_);
  title_ = std::move(other.title_);
  distilled_url_ = std::move(other.distilled_url_);
  distilled_state_ = std::move(other.distilled_state_);
  backoff_ = std::move(other.backoff_);
  failed_download_counter_ = std::move(other.failed_download_counter_);
  return *this;
}

bool ReadingListEntry::operator==(const ReadingListEntry& other) const {
  return url_ == other.url_;
}

void ReadingListEntry::SetTitle(const std::string& title) {
  title_ = title;
}

void ReadingListEntry::SetDistilledURL(const GURL& url) {
  DCHECK(url.is_valid());
  distilled_url_ = url;
  distilled_state_ = PROCESSED;
  backoff_->Reset();
  failed_download_counter_ = 0;
}

void ReadingListEntry::SetDistilledState(DistillationState distilled_state) {
  DCHECK(distilled_state != PROCESSED);  // use SetDistilledURL instead.
  DCHECK(distilled_state != WAITING);
  // Increase time until next retry exponentially if the state change from a
  // non-error state to an error state.
  if ((distilled_state == WILL_RETRY || distilled_state == ERROR) &&
      distilled_state_ != WILL_RETRY && distilled_state_ != ERROR) {
    backoff_->InformOfRequest(false);
    failed_download_counter_++;
  }

  distilled_state_ = distilled_state;
  distilled_url_ = GURL();
}
