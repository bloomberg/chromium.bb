// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_THUMBNAIL_SUPPORT_H_
#define CHROME_COMMON_THUMBNAIL_SUPPORT_H_
#pragma once

// TODO(mazda): Remove this file once in-browser thumbnailing is supported on
// all platforms.

// Returns true if in-browser thumbnailing is supported and not disabled by the
// command line flag.
bool ShouldEnableInBrowserThumbnailing();

#endif  // CHROME_COMMON_THUMBNAIL_SUPPORT_H_
