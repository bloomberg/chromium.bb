// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/file_browser_handler.h"

#include "chrome/common/extensions/url_pattern.h"
#include "googleurl/src/gurl.h"

FileBrowserHandler::FileBrowserHandler() {
}

FileBrowserHandler::~FileBrowserHandler() {
}

void FileBrowserHandler::AddPattern(const URLPattern& pattern) {
  patterns_.push_back(pattern);
}

void FileBrowserHandler::ClearPatterns() {
  patterns_.clear();
}

bool FileBrowserHandler::MatchesURL(const GURL& url) const {
  for (PatternList::const_iterator pattern = patterns_.begin();
       pattern != patterns_.end(); ++pattern) {
    if (pattern->MatchesUrl(url))
      return true;
  }
  return false;
}

