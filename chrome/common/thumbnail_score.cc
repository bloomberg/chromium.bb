// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/thumbnail_score.h"

#include "base/logging.h"
#include "base/stringprintf.h"

using base::Time;
using base::TimeDelta;

const TimeDelta ThumbnailScore::kUpdateThumbnailTime = TimeDelta::FromDays(1);
const double ThumbnailScore::kThumbnailMaximumBoringness = 0.94;
// Per crbug.com/65936#c4, 91.83% of thumbnail scores are less than 0.70.
const double ThumbnailScore::kThumbnailInterestingEnoughBoringness = 0.70;
const double ThumbnailScore::kThumbnailDegradePerHour = 0.01;

// Calculates a numeric score from traits about where a snapshot was
// taken. We store the raw components in the database because I'm sure
// this will evolve and I don't want to break databases.
static int GetThumbnailType(bool good_clipping, bool at_top) {
  if (good_clipping && at_top) {
    return 0;
  } else if (good_clipping && !at_top) {
    return 1;
  } else if (!good_clipping && at_top) {
    return 2;
  } else if (!good_clipping && !at_top) {
    return 3;
  } else {
    NOTREACHED();
    return -1;
  }
}

ThumbnailScore::ThumbnailScore()
    : boring_score(1.0),
      good_clipping(false),
      at_top(false),
      time_at_snapshot(Time::Now()),
      redirect_hops_from_dest(0) {
}

ThumbnailScore::ThumbnailScore(double score, bool clipping, bool top)
    : boring_score(score),
      good_clipping(clipping),
      at_top(top),
      time_at_snapshot(Time::Now()),
      redirect_hops_from_dest(0) {
}

ThumbnailScore::ThumbnailScore(double score, bool clipping, bool top,
                               const Time& time)
    : boring_score(score),
      good_clipping(clipping),
      at_top(top),
      time_at_snapshot(time),
      redirect_hops_from_dest(0) {
}

ThumbnailScore::~ThumbnailScore() {
}

bool ThumbnailScore::Equals(const ThumbnailScore& rhs) const {
  // When testing equality we use ToTimeT() because that's the value
  // stuck in the SQL database, so we need to test equivalence with
  // that lower resolution.
  return boring_score == rhs.boring_score &&
      good_clipping == rhs.good_clipping &&
      at_top == rhs.at_top &&
      time_at_snapshot.ToTimeT() == rhs.time_at_snapshot.ToTimeT() &&
      redirect_hops_from_dest == rhs.redirect_hops_from_dest;
}

std::string ThumbnailScore::ToString() const {
  return StringPrintf("boring_score: %f, at_top %d, good_clipping %d, "
                      "time_at_snapshot: %f, redirect_hops_from_dest: %d",
                      boring_score,
                      at_top,
                      good_clipping,
                      time_at_snapshot.ToDoubleT(),
                      redirect_hops_from_dest);
}

bool ShouldReplaceThumbnailWith(const ThumbnailScore& current,
                                const ThumbnailScore& replacement) {
  int current_type = GetThumbnailType(current.good_clipping, current.at_top);
  int replacement_type = GetThumbnailType(replacement.good_clipping,
                                          replacement.at_top);
  if (replacement_type < current_type) {
    // If we have a better class of thumbnail, add it if it meets
    // certain minimum boringness.
    return replacement.boring_score <
        ThumbnailScore::kThumbnailMaximumBoringness;
  } else if (replacement_type == current_type) {
    // It's much easier to do the scaling below when we're dealing with "higher
    // is better." Then we can decrease the score by dividing by a fraction.
    const double kThumbnailMinimumInterestingness =
        1.0 - ThumbnailScore::kThumbnailMaximumBoringness;
    double current_interesting_score = 1.0 - current.boring_score;
    double replacement_interesting_score = 1.0 - replacement.boring_score;

    // Degrade the score of each thumbnail to account for how many redirects
    // they are away from the destination. 1/(x+1) gives a scaling factor of
    // one for x = 0, and asymptotically approaches 0 for larger values of x.
    current_interesting_score *=
        1.0 / (current.redirect_hops_from_dest + 1);
    replacement_interesting_score *=
        1.0 / (replacement.redirect_hops_from_dest + 1);

    // Degrade the score and prefer the newer one based on how long apart the
    // two thumbnails were taken. This means we'll eventually replace an old
    // good one with a new worse one assuming enough time has passed.
    TimeDelta time_between_thumbnails =
        replacement.time_at_snapshot - current.time_at_snapshot;
    current_interesting_score -= time_between_thumbnails.InHours() *
         ThumbnailScore::kThumbnailDegradePerHour;

    if (current_interesting_score < kThumbnailMinimumInterestingness)
      current_interesting_score = kThumbnailMinimumInterestingness;
    if (replacement_interesting_score > current_interesting_score)
      return true;
  }

  // If the current thumbnail doesn't meet basic boringness
  // requirements, but the replacement does, always replace the
  // current one even if we're using a worse thumbnail type.
  return current.boring_score >= ThumbnailScore::kThumbnailMaximumBoringness &&
      replacement.boring_score < ThumbnailScore::kThumbnailMaximumBoringness;
}

bool ThumbnailScore::ShouldConsiderUpdating() {
  const TimeDelta time_elapsed = Time::Now() - time_at_snapshot;
  // Consider the current thumbnail to be new and interesting enough if
  // the following critera are met.
  const bool new_and_interesting_enough =
      (time_elapsed < kUpdateThumbnailTime &&
       good_clipping && at_top &&
       boring_score < kThumbnailInterestingEnoughBoringness);
  // We want to generate a new thumbnail when the current thumbnail is
  // sufficiently old or uninteresting.
  return !new_and_interesting_enough;
}
