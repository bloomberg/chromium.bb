// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A handful of resource-like constants related to the Content application.

#ifndef CONTENT_COMMON_CHROME_CONSTANTS_H_
#define CONTENT_COMMON_CHROME_CONSTANTS_H_
#pragma once

#include <stddef.h>         // For size_t

namespace content {

// The maximum number of characters in the URL that we're willing to accept
// in the browser process. It is set low enough to avoid damage to the browser
// but high enough that a web site can abuse location.hash for a little storage.
// We have different values for "max accepted" and "max displayed" because
// a data: URI may be legitimately massive, but the full URI would kill all
// known operating systems if you dropped it into a UI control.
extern const size_t kMaxURLChars;
extern const size_t kMaxURLDisplayChars;

}  // namespace content

#endif  // CONTENT_COMMON_CHROME_CONSTANTS_H_
