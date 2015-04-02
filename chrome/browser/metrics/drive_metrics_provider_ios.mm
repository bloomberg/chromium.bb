// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/drive_metrics_provider.h"

// static
bool DriveMetricsProvider::HasSeekPenalty(const base::FilePath& path,
                                          bool* has_seek_penalty) {
  *has_seek_penalty = false;
  return true;
}
