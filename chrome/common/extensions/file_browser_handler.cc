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
  url_set_.AddPattern(pattern);
}

void FileBrowserHandler::ClearPatterns() {
  url_set_.ClearPatterns();
}

bool FileBrowserHandler::MatchesURL(const GURL& url) const {
  return url_set_.MatchesURL(url);
}
