// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_NUDGE_SOURCE_H_
#define CHROME_BROWSER_SYNC_ENGINE_NUDGE_SOURCE_H_
#pragma once

namespace browser_sync {

enum NudgeSource {
  NUDGE_SOURCE_UNKNOWN = 0,
  // We received an invalidation message and are nudging to check for updates.
  NUDGE_SOURCE_NOTIFICATION,
  // A local change occurred (e.g. bookmark moved).
  NUDGE_SOURCE_LOCAL,
  // A previous sync cycle did not fully complete (e.g. HTTP error).
  NUDGE_SOURCE_CONTINUATION,
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_NUDGE_SOURCE_H_
