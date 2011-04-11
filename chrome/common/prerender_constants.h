// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants related to the prerendering feature in chrome.

#ifndef CHROME_COMMON_PRERENDER_CONSTANTS_H_
#define CHROME_COMMON_PRERENDER_CONSTANTS_H_
#pragma once

namespace prerender {

enum PrerenderCancellationReason {
  PRERENDER_CANCELLATION_REASON_FIRST_UNUSED,
  PRERENDER_CANCELLATION_REASON_HTML5_MEDIA,
  PRERENDER_CANCELLATION_REASON_LAST_UNUSED,
};

}  // namespace prerender

#endif  // CHROME_COMMON_PRERENDER_CONSTANTS_H_
