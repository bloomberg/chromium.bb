// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/app_notification.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/common/guid.h"

namespace {

const char* kIsLocalKey = "is_local";
const char* kGuidKey = "guid";
const char* kExtensionIdKey = "extension_id";
const char* kTitleKey = "title";
const char* kBodyKey = "body";
const char* kLinkUrlKey = "link_url";
const char* kLinkTextKey = "link_text";

}  // namespace

AppNotification::AppNotification(bool is_local,
                                 const std::string& guid,
                                 const std::string& extension_id,
                                 const std::string& title,
                                 const std::string& body)
    : is_local_(is_local),
      extension_id_(extension_id),
      title_(title),
      body_(body) {
  guid_ = guid.empty() ? guid::GenerateGUID() : guid;
}

AppNotification::~AppNotification() {}

AppNotification* AppNotification::Copy() {
  AppNotification* copy = new AppNotification(
      this->is_local(), this->guid(), this->extension_id(),
      this->title(), this->body());
  copy->set_link_url(this->link_url());
  copy->set_link_text(this->link_text());
  return copy;
}

void AppNotification::ToDictionaryValue(DictionaryValue* result) {
  CHECK(result);
  result->SetBoolean(kIsLocalKey, is_local_);
  if (!guid_.empty())
    result->SetString(kGuidKey, guid_);
  if (!extension_id_.empty())
    result->SetString(kExtensionIdKey, extension_id_);
  if (!title_.empty())
    result->SetString(kTitleKey, title_);
  if (!body_.empty())
    result->SetString(kBodyKey, body_);
  if (!link_url_.is_empty())
    result->SetString(kLinkUrlKey, link_url_.possibly_invalid_spec());
  if (!link_text_.empty())
    result->SetString(kLinkTextKey, link_text_);
}

// static
AppNotification* AppNotification::FromDictionaryValue(
    const DictionaryValue& value) {
  scoped_ptr<AppNotification> result(new AppNotification(true, "", "", "", ""));

  if (value.HasKey(kIsLocalKey) && !value.GetBoolean(
      kIsLocalKey, &result->is_local_)) {
    return NULL;
  }
  if (value.HasKey(kGuidKey) && !value.GetString(kGuidKey, &result->guid_))
    return NULL;
  if (value.HasKey(kExtensionIdKey) &&
      !value.GetString(kExtensionIdKey, &result->extension_id_))
    return NULL;
  if (value.HasKey(kTitleKey) && !value.GetString(kTitleKey, &result->title_))
    return NULL;
  if (value.HasKey(kBodyKey) && !value.GetString(kBodyKey, &result->body_))
    return NULL;
  if (value.HasKey(kLinkUrlKey)) {
    std::string url;
    if (!value.GetString(kLinkUrlKey, &url))
      return NULL;
    GURL gurl(url);
    if (!gurl.is_valid())
      return NULL;
    result->set_link_url(gurl);
  }
  if (value.HasKey(kLinkTextKey) &&
      !value.GetString(kLinkTextKey, &result->link_text_)) {
    return NULL;
  }

  return result.release();
}

bool AppNotification::Equals(const AppNotification& other) const {
  return (is_local_ == other.is_local_ &&
          guid_ == other.guid_ &&
          extension_id_ == other.extension_id_ &&
          title_ == other.title_ &&
          body_ == other.body_ &&
          link_url_ == other.link_url_ &&
          link_text_ == other.link_text_);
}

AppNotificationList* CopyAppNotificationList(
    const AppNotificationList& source) {
  AppNotificationList* copy = new AppNotificationList();

  for (AppNotificationList::const_iterator iter = source.begin();
      iter != source.end(); ++iter) {
    copy->push_back(linked_ptr<AppNotification>(iter->get()->Copy()));
  }
  return copy;
}
