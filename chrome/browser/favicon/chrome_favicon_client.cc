// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/chrome_favicon_client.h"

#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "components/bookmarks/browser/bookmark_model.h"

class Profile;

ChromeFaviconClient::ChromeFaviconClient(Profile* profile) : profile_(profile) {
}

ChromeFaviconClient::~ChromeFaviconClient() {
}

FaviconService* ChromeFaviconClient::GetFaviconService() {
  return FaviconServiceFactory::GetForProfile(profile_,
                                              Profile::EXPLICIT_ACCESS);
}

bool ChromeFaviconClient::IsBookmarked(const GURL& url) {
  BookmarkModel* bookmark_model = BookmarkModelFactory::GetForProfile(profile_);
  return bookmark_model && bookmark_model->IsBookmarked(url);
}
