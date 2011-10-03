// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FRAGMENTATION_CHECKER_WIN_H_
#define CHROME_BROWSER_FRAGMENTATION_CHECKER_WIN_H_
#pragma once

class FilePath;

namespace fragmentation_checker {

const int kMaxExtentCount = 1 << 16;

// Returns the number of extents for the file at |file_path|. The number is
// capped at kMaxExtentCount, files with more extents than that will be counted
// as having kMaxExtentCount extents. On failure, this function returns 0.
int CountFileExtents(const FilePath& file_path);

// Records fragmentation metrics for the current module. This records the number
// of fragments the current module is stored in.
// This will be used to determine whether pursuing more aggressive
// manual defragmentation is worth the effort.
void RecordFragmentationMetricForCurrentModule();

}  // namespace fragmentation_checker

#endif  // CHROME_BROWSER_FRAGMENTATION_CHECKER_WIN_H_
