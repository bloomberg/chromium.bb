// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_GDATA_CONTACTS_OPERATIONS_H_
#define CHROME_BROWSER_GOOGLE_APIS_GDATA_CONTACTS_OPERATIONS_H_

#include <string>

#include "chrome/browser/google_apis/base_operations.h"

namespace net {
class URLRequestContextGetter;
}  // namespace net

namespace google_apis {

//========================== GetContactGroupsOperation =========================

// This class fetches a JSON feed containing a user's contact groups.
class GetContactGroupsOperation : public GetDataOperation {
 public:
  GetContactGroupsOperation(
      OperationRegistry* registry,
      net::URLRequestContextGetter* url_request_context_getter,
      const GetDataCallback& callback);
  virtual ~GetContactGroupsOperation();

  void set_feed_url_for_testing(const GURL& url) {
    feed_url_for_testing_ = url;
  }

 protected:
  // Overridden from GetDataOperation.
  virtual GURL GetURL() const OVERRIDE;

 private:
  // If non-empty, URL of the feed to fetch.
  GURL feed_url_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(GetContactGroupsOperation);
};

//============================ GetContactsOperation ============================

// This class fetches a JSON feed containing a user's contacts.
class GetContactsOperation : public GetDataOperation {
 public:
  GetContactsOperation(
      OperationRegistry* registry,
      net::URLRequestContextGetter* url_request_context_getter,
      const std::string& group_id,
      const base::Time& min_update_time,
      const GetDataCallback& callback);
  virtual ~GetContactsOperation();

  void set_feed_url_for_testing(const GURL& url) {
    feed_url_for_testing_ = url;
  }

 protected:
  // Overridden from GetDataOperation.
  virtual GURL GetURL() const OVERRIDE;

 private:
  // If non-empty, URL of the feed to fetch.
  GURL feed_url_for_testing_;

  // If non-empty, contains the ID of the group whose contacts should be
  // returned.  Group IDs generally look like this:
  // http://www.google.com/m8/feeds/groups/user%40gmail.com/base/6
  std::string group_id_;

  // If is_null() is false, contains a minimum last-updated time that will be
  // used to filter contacts.
  base::Time min_update_time_;

  DISALLOW_COPY_AND_ASSIGN(GetContactsOperation);
};

//========================== GetContactPhotoOperation ==========================

// This class fetches a contact's photo.
class GetContactPhotoOperation : public UrlFetchOperationBase {
 public:
  GetContactPhotoOperation(
      OperationRegistry* registry,
      net::URLRequestContextGetter* url_request_context_getter,
      const GURL& photo_url,
      const GetContentCallback& callback);
  virtual ~GetContactPhotoOperation();

 protected:
  // Overridden from UrlFetchOperationBase.
  virtual GURL GetURL() const OVERRIDE;
  virtual void ProcessURLFetchResults(const net::URLFetcher* source) OVERRIDE;
  virtual void RunCallbackOnPrematureFailure(GDataErrorCode code) OVERRIDE;

 private:
  // Location of the photo to fetch.
  GURL photo_url_;

  // Callback to which the photo data is passed.
  GetContentCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(GetContactPhotoOperation);
};

}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_GDATA_CONTACTS_OPERATIONS_H_
