// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_PREVIEWS_EXPERIMENTS_H_
#define COMPONENTS_PREVIEWS_PREVIEWS_EXPERIMENTS_H_

namespace previews {

// Returns true if any client-side previews experiment is active.
bool IsIncludedInClientSidePreviewsExperimentsFieldTrial();

// Returns true if the field trial that should enable offline pages for
// prohibitvely slow networks is active.
bool IsOfflinePreviewsEnabled();

// Sets the appropriate state for field trial and variations to imitate the
// offline pages field trial.
bool EnableOfflinePreviewsForTesting();

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_PREVIEWS_EXPERIMENTS_H_
