// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_CHROME_CONTENT_SETTINGS_CLIENT_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_CHROME_CONTENT_SETTINGS_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/content_settings/local_shared_objects_container.h"
#include "components/content_settings/core/browser/content_settings_client.h"
#include "content/public/browser/web_contents_user_data.h"

// Chrome-specific implementation of the ContentSettingsClient interface.
class ChromeContentSettingsClient
    : public content_settings::ContentSettingsClient,
      public content::WebContentsUserData<ChromeContentSettingsClient> {
 public:
  ~ChromeContentSettingsClient() override;

  // ContentSettingsClient implementation.
  void OnCookiesRead(const GURL& url,
                     const GURL& first_party_url,
                     const net::CookieList& cookie_list,
                     bool blocked_by_policy) override;
  void OnCookieChanged(const GURL& url,
                       const GURL& first_party_url,
                       const std::string& cookie_line,
                       const net::CookieOptions& options,
                       bool blocked_by_policy) override;
  void OnFileSystemAccessed(const GURL& url, bool blocked_by_policy) override;
  void OnIndexedDBAccessed(const GURL& url,
                           const base::string16& description,
                           bool blocked_by_policy) override;
  void OnLocalStorageAccessed(const GURL& url,
                              bool local,
                              bool blocked_by_policy) override;
  void OnWebDatabaseAccessed(const GURL& url,
                             const base::string16& name,
                             const base::string16& display_name,
                             bool blocked_by_policy) override;
  const LocalSharedObjectsCounter& local_shared_objects(
      AccessType type) const override;

  // Creates a new copy of a CookiesTreeModel for all allowed (or blocked,
  // depending on |type|) local shared objects.
  std::unique_ptr<CookiesTreeModel> CreateCookiesTreeModel(
      AccessType type) const;

 private:
  friend class content::WebContentsUserData<ChromeContentSettingsClient>;

  explicit ChromeContentSettingsClient(content::WebContents* contents);

  // Stores the blocked/allowed site data.
  LocalSharedObjectsContainer allowed_local_shared_objects_;
  LocalSharedObjectsContainer blocked_local_shared_objects_;

  DISALLOW_COPY_AND_ASSIGN(ChromeContentSettingsClient);
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_CHROME_CONTENT_SETTINGS_CLIENT_H_
