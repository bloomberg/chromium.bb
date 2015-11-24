// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ERROR_PAGE_COMMON_OFFLINE_PAGE_TYPES_H_
#define COMPONENTS_ERROR_PAGE_COMMON_OFFLINE_PAGE_TYPES_H_

namespace error_page {

enum class OfflinePageStatus {
  // No offline page exists.
  NONE,
  // An offline copy of current page exists.
  HAS_OFFLINE_PAGE,
  // An offline copy of current page does not exist, but the offline copies of
  // other pages exist.
  HAS_OTHER_OFFLINE_PAGES,

  OFFLINE_PAGE_STATUS_LAST = HAS_OTHER_OFFLINE_PAGES,
};

}  // namespace error_page

#endif  // COMPONENTS_ERROR_PAGE_COMMON_OFFLINE_PAGE_TYPES_H_
