// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_GDATA_CONTACTS_REQUESTS_H_
#define CHROME_BROWSER_GOOGLE_APIS_GDATA_CONTACTS_REQUESTS_H_

#include <string>

#include "base/time/time.h"
#include "chrome/browser/google_apis/base_requests.h"

namespace google_apis {

//========================== GetContactGroupsRequest =========================

// This class fetches a JSON feed containing a user's contact groups.
class GetContactGroupsRequest : public GetDataRequest {
 public:
  GetContactGroupsRequest(RequestSender* runner,
                          const GetDataCallback& callback);
  virtual ~GetContactGroupsRequest();

  void set_feed_url_for_testing(const GURL& url) {
    feed_url_for_testing_ = url;
  }

 protected:
  // Overridden from GetDataRequest.
  virtual GURL GetURL() const OVERRIDE;

 private:
  // If non-empty, URL of the feed to fetch.
  GURL feed_url_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(GetContactGroupsRequest);
};

//============================ GetContactsRequest ============================

// This class fetches a JSON feed containing a user's contacts.
class GetContactsRequest : public GetDataRequest {
 public:
  GetContactsRequest(RequestSender* runner,
                     const std::string& group_id,
                     const base::Time& min_update_time,
                     const GetDataCallback& callback);
  virtual ~GetContactsRequest();

  void set_feed_url_for_testing(const GURL& url) {
    feed_url_for_testing_ = url;
  }

 protected:
  // Overridden from GetDataRequest.
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

  DISALLOW_COPY_AND_ASSIGN(GetContactsRequest);
};

//========================== GetContactPhotoRequest ==========================

// This class fetches a contact's photo.
class GetContactPhotoRequest : public UrlFetchRequestBase {
 public:
  GetContactPhotoRequest(RequestSender* runner,
                         const GURL& photo_url,
                         const GetContentCallback& callback);
  virtual ~GetContactPhotoRequest();

 protected:
  // Overridden from UrlFetchRequestBase.
  virtual GURL GetURL() const OVERRIDE;
  virtual void ProcessURLFetchResults(const net::URLFetcher* source) OVERRIDE;
  virtual void RunCallbackOnPrematureFailure(GDataErrorCode code) OVERRIDE;

 private:
  // Location of the photo to fetch.
  GURL photo_url_;

  // Callback to which the photo data is passed.
  GetContentCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(GetContactPhotoRequest);
};

}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_GDATA_CONTACTS_REQUESTS_H_
