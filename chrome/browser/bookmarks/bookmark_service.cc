// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_service.h"

#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"

// static
BookmarkService* BookmarkService::FromBrowserContext(
    content::BrowserContext* browser_context) {
  return BookmarkModelFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
}
