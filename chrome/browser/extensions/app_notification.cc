// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/app_notification.h"

#include "base/memory/scoped_ptr.h"

AppNotification::AppNotification(const std::string& title,
                                 const std::string& body)
    : title_(title), body_(body) {}

AppNotification::~AppNotification() {}

namespace {

const char* kTitleKey = "title";
const char* kBodyKey = "body";
const char* kLinkUrlKey = "link_url";
const char* kLinkTextKey = "link_text";

}  // namespace

void AppNotification::ToDictionaryValue(DictionaryValue* result) {
  CHECK(result);
  if (!title_.empty())
    result->SetString(kTitleKey, title_);
  if (!body_.empty())
    result->SetString(kBodyKey, body_);
  if (!link_url_.is_empty()) {
    result->SetString(kLinkUrlKey, link_url_.possibly_invalid_spec());
    result->SetString(kLinkTextKey, link_text_);
  }
}

// static
AppNotification* AppNotification::FromDictionaryValue(
    const DictionaryValue& value) {

  scoped_ptr<AppNotification> result(new AppNotification("", ""));

  if (value.HasKey(kTitleKey) && !value.GetString(kTitleKey, &result->title_))
    return NULL;
  if (value.HasKey(kBodyKey) && !value.GetString(kBodyKey, &result->body_))
    return NULL;
  if (value.HasKey(kLinkUrlKey)) {
    if (!value.HasKey(kLinkTextKey))
      return NULL;
    std::string url;
    if (!value.GetString(kLinkUrlKey, &url) ||
        !value.GetString(kLinkTextKey, &result->link_text_))
      return NULL;
    GURL gurl(url);
    if (!gurl.is_valid())
      return NULL;
    result->set_link_url(gurl);
  }

  return result.release();
}

bool AppNotification::Equals(const AppNotification& other) const {
  return (title_ == other.title_ &&
          body_ == other.body_ &&
          link_url_ == other.link_url_ &&
          link_text_ == other.link_text_);
}
