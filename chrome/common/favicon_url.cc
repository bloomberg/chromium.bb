// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/favicon_url.h"

FaviconURL::FaviconURL()
  : icon_type(INVALID_ICON) {
}

FaviconURL::FaviconURL(const GURL& url, IconType type)
    : icon_url(url),
      icon_type(type) {
}

FaviconURL::~FaviconURL() {
}
