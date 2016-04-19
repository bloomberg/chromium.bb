// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/chrome_content_settings_client.h"
#include "chrome/browser/profiles/profile.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ChromeContentSettingsClient);

ChromeContentSettingsClient::~ChromeContentSettingsClient() {
}

void ChromeContentSettingsClient::OnCookiesRead(
    const GURL& url,
    const GURL& first_party_url,
    const net::CookieList& cookie_list,
    bool blocked_by_policy) {
  // TODO(vabr): This is just a dummy implementation, replace with methods from
  // TabSpecificContentSettings.
}

void ChromeContentSettingsClient::OnCookieChanged(
    const GURL& url,
    const GURL& first_party_url,
    const std::string& cookie_line,
    const net::CookieOptions& options,
    bool blocked_by_policy) {
  // TODO(vabr): This is just a dummy implementation, replace with methods from
  // TabSpecificContentSettings.
}

void ChromeContentSettingsClient::OnFileSystemAccessed(const GURL& url,
                                                       bool blocked_by_policy) {
  // TODO(vabr): This is just a dummy implementation, replace with methods from
  // TabSpecificContentSettings.
}

void ChromeContentSettingsClient::OnIndexedDBAccessed(
    const GURL& url,
    const base::string16& description,
    bool blocked_by_policy) {
  // TODO(vabr): This is just a dummy implementation, replace with methods from
  // TabSpecificContentSettings.
}

void ChromeContentSettingsClient::OnLocalStorageAccessed(
    const GURL& url,
    bool local,
    bool blocked_by_policy) {
  // TODO(vabr): This is just a dummy implementation, replace with methods from
  // TabSpecificContentSettings.
}

void ChromeContentSettingsClient::OnWebDatabaseAccessed(
    const GURL& url,
    const base::string16& name,
    const base::string16& display_name,
    bool blocked_by_policy) {
  // TODO(vabr): This is just a dummy implementation, replace with methods from
  // TabSpecificContentSettings.
}

const LocalSharedObjectsCounter&
ChromeContentSettingsClient::local_shared_objects(AccessType type) const {
  switch (type) {
    case ALLOWED:
      return allowed_local_shared_objects_;
    case BLOCKED:
      return blocked_local_shared_objects_;
  }
  // Some compilers don't believe this is not reachable, so let's return a dummy
  // value.
  NOTREACHED();
  return blocked_local_shared_objects_;
}

std::unique_ptr<CookiesTreeModel>
ChromeContentSettingsClient::CreateCookiesTreeModel(AccessType type) const {
  switch (type) {
    case ALLOWED:
      return allowed_local_shared_objects_.CreateCookiesTreeModel();
    case BLOCKED:
      return blocked_local_shared_objects_.CreateCookiesTreeModel();
  }
  // Some compilers don't believe this is not reachable, so let's return a dummy
  // value.
  NOTREACHED();
  return std::unique_ptr<CookiesTreeModel>();
}

ChromeContentSettingsClient::ChromeContentSettingsClient(
    content::WebContents* contents)
    : allowed_local_shared_objects_(
          Profile::FromBrowserContext(contents->GetBrowserContext())),
      blocked_local_shared_objects_(
          Profile::FromBrowserContext(contents->GetBrowserContext())) {
}
