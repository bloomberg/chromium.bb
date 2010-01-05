// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_REQUEST_INFO_H_
#define CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_REQUEST_INFO_H_

#include "chrome/browser/privacy_blacklist/blacklist.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request.h"
#include "webkit/glue/resource_type.h"

class BlacklistManager;

// Privacy blacklist-related information attached to each request.
class BlacklistRequestInfo : public URLRequest::UserData {
 public:
  // Key used to access data attached to URLRequest objects.
  static const void* const kURLRequestDataKey;

  BlacklistRequestInfo(const GURL& url, ResourceType::Type resource_type,
                       BlacklistManager* blacklist_manager);
  ~BlacklistRequestInfo();

  ResourceType::Type resource_type() const { return resource_type_; }
  const BlacklistManager* GetBlacklistManager() const {
    return blacklist_manager_.get();
  }

  // Get the blacklist request info stored in |request|, or NULL if there is no
  // one. The object is owned by |request|.
  static BlacklistRequestInfo* FromURLRequest(const URLRequest* request);

 private:
  // URL of the request.
  const GURL url_;

  // Type of the requested resource (main frame, image, etc).
  const ResourceType::Type resource_type_;

  // BlacklistManager used for the request.
  scoped_refptr<BlacklistManager> blacklist_manager_;

  DISALLOW_COPY_AND_ASSIGN(BlacklistRequestInfo);
};

#endif  // CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_REQUEST_INFO_H_
