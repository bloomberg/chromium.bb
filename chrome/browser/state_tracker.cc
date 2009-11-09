// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/state_tracker.h"

// Number of characters the buffer contains.
static const size_t kSize = 256;

StateTracker::StateTracker()
  : index_(0),
    content_(new char[kSize]) {
  for (size_t i = 0; i < kSize; ++i)
    content_[i] = '\0';
}

void StateTracker::Append(const std::string& text) {
  if (text.size() >= kSize) {
    NOTREACHED();
    return;
  }
  for (size_t i = 0; i < text.size(); ++i) {
    content_[index_] = text[i];
    index_ = (index_ + 1) % kSize;
  }
  content_[index_] = '!';
}

void StateTracker::Crash() {
  volatile char state[kSize];
  for (size_t i = 0; i < kSize; ++i)
    state[i] = content_[i];

  CHECK(false);
}
