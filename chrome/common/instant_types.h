// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_INSTANT_TYPES_H_
#define CHROME_COMMON_INSTANT_TYPES_H_
#pragma once

// Enum describing the ways instant suggest text can be completed.
enum InstantCompleteBehavior {
  // Complete the suggestion now.
  INSTANT_COMPLETE_NOW,

  // Complete the suggestion after a delay.
  INSTANT_COMPLETE_DELAYED,

  // Never complete the suggestion.
  INSTANT_COMPLETE_NEVER
};

#endif  // CHROME_COMMON_INSTANT_TYPES_H_
