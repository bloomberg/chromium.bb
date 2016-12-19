// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/session_changes.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_utils.h"

namespace ios_internal {

SessionChanges::SessionChanges(
    std::vector<size_t> const& tabHashesInInitialState,
    std::vector<size_t> const& tabHashesInFinalState) {
  ios_internal::MinimalReplacementOperations(tabHashesInInitialState,
                                             tabHashesInFinalState, &updates_,
                                             &deletions_, &insertions_);
}

SessionChanges::~SessionChanges() {}

std::vector<size_t> const& SessionChanges::deletions() const {
  return deletions_;
}
std::vector<size_t> const& SessionChanges::insertions() const {
  return insertions_;
}
std::vector<size_t> const& SessionChanges::updates() const {
  return updates_;
}

bool SessionChanges::hasChanges() const {
  return updates_.size() || deletions_.size() || insertions_.size();
}

}  // namespace ios_internal
