// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_COMMON_ANDROID_BOOKMARK_TYPE_H_
#define COMPONENTS_BOOKMARKS_COMMON_ANDROID_BOOKMARK_TYPE_H_

namespace bookmarks {

enum BookmarkType {
#define DEFINE_BOOKMARK_TYPE(name, value) name = value,
#include "components/bookmarks/common/android/bookmark_type_list.h"
#undef DEFINE_BOOKMARK_TYPE
};

}

#endif  // COMPONENTS_BOOKMARKS_COMMON_ANDROID_BOOKMARK_TYPE_H_
