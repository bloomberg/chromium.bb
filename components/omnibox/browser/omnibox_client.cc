// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_client.h"

#include "base/strings/string_util.h"
#include "ui/gfx/image/image.h"

std::unique_ptr<OmniboxNavigationObserver>
OmniboxClient::CreateOmniboxNavigationObserver(
    const base::string16& text,
    const AutocompleteMatch& match,
    const AutocompleteMatch& alternate_nav_match) {
  return nullptr;
}

bool OmniboxClient::CurrentPageExists() const {
  return true;
}

const GURL& OmniboxClient::GetURL() const {
  return GURL::EmptyGURL();
}

const base::string16& OmniboxClient::GetTitle() const {
  return base::EmptyString16();
}

gfx::Image OmniboxClient::GetFavicon() const {
  return gfx::Image();
}

bool OmniboxClient::IsInstantNTP() const {
  return false;
}

bool OmniboxClient::IsSearchResultsPage() const {
  return false;
}

bool OmniboxClient::IsLoading() const {
  return false;
}

bool OmniboxClient::IsPasteAndGoEnabled() const {
  return false;
}

bool OmniboxClient::IsNewTabPage(const GURL& url) const {
  return false;
}

bool OmniboxClient::IsHomePage(const GURL& url) const {
  return false;
}

bookmarks::BookmarkModel* OmniboxClient::GetBookmarkModel() {
  return nullptr;
}

TemplateURLService* OmniboxClient::GetTemplateURLService() {
  return nullptr;
}

AutocompleteClassifier* OmniboxClient::GetAutocompleteClassifier() {
  return nullptr;
}

gfx::Image OmniboxClient::GetIconIfExtensionMatch(
    const AutocompleteMatch& match) const {
  return gfx::Image();
}

bool OmniboxClient::ProcessExtensionKeyword(
    const TemplateURL* template_url,
    const AutocompleteMatch& match,
    WindowOpenDisposition disposition,
    OmniboxNavigationObserver* observer) {
  return false;
}
