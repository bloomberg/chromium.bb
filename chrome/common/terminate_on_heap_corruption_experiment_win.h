// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_TERMINATE_ON_HEAP_CORRUPTION_EXPERIMENT_WIN_H_
#define CHROME_COMMON_TERMINATE_ON_HEAP_CORRUPTION_EXPERIMENT_WIN_H_

bool ShouldExperimentallyDisableTerminateOnHeapCorruption();
void InitializeDisableTerminateOnHeapCorruptionExperiment();

#endif  // CHROME_COMMON_TERMINATE_ON_HEAP_CORRUPTION_EXPERIMENT_WIN_H_
