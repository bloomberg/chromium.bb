// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_URL_AND_TITLE_H_
#define COMPONENTS_BOOKMARKS_BROWSER_URL_AND_TITLE_H_

#include "base/strings/string16.h"
#include "url/gurl.h"

namespace bookmarks {

struct UrlAndTitle {
  GURL url;
  base::string16 title;
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_URL_AND_TITLE_H_
