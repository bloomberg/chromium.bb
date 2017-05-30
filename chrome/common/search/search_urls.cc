// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/search/search_urls.h"

#include "content/public/common/url_constants.h"
#include "url/gurl.h"

namespace search {

namespace {

const char kServiceWorkerFileName[] = "newtab-serviceworker.js";

bool MatchesOrigin(const GURL& my_url, const GURL& other_url) {
  return my_url.host_piece() == other_url.host_piece() &&
         my_url.port() == other_url.port() &&
         (my_url.scheme_piece() == other_url.scheme_piece() ||
          (my_url.SchemeIs(url::kHttpsScheme) &&
           other_url.SchemeIs(url::kHttpScheme)));
}

}  // namespace

bool MatchesOriginAndPath(const GURL& my_url, const GURL& other_url) {
  return MatchesOrigin(my_url, other_url) &&
         my_url.path_piece() == other_url.path_piece();
}

bool IsMatchingServiceWorker(const GURL& my_url, const GURL& document_url) {
  // The origin should match.
  if (!MatchesOrigin(my_url, document_url))
    return false;

  // The url filename should be the new tab page ServiceWorker.
  std::string my_filename = my_url.ExtractFileName();
  if (my_filename != kServiceWorkerFileName)
    return false;

  // The paths up to the filenames should be the same.
  std::string my_path_without_filename = my_url.path();
  my_path_without_filename = my_path_without_filename.substr(
      0, my_path_without_filename.length() - my_filename.length());
  std::string document_filename = document_url.ExtractFileName();
  std::string document_path_without_filename = document_url.path();
  document_path_without_filename = document_path_without_filename.substr(
      0, document_path_without_filename.length() - document_filename.length());

  return my_path_without_filename == document_path_without_filename;
}

} // namespace search
