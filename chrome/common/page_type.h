// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PAGE_TYPE_H_
#define CHROME_COMMON_PAGE_TYPE_H_
#pragma once

// The type of the page an entry corresponds to.  Used by chrome_frame and the
// automation layer to detect the state of a TabContents.
enum PageType {
  NORMAL_PAGE = 0,
  ERROR_PAGE,
  INTERSTITIAL_PAGE
};

#endif  // CHROME_COMMON_PAGE_TYPE_H_
