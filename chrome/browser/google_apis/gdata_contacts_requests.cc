// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/gdata_contacts_requests.h"

#include "chrome/browser/google_apis/time_util.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace google_apis {

namespace {

// URL requesting all contact groups.
const char kGetContactGroupsURL[] =
    "https://www.google.com/m8/feeds/groups/default/full?alt=json";

// URL requesting all contacts.
// TODO(derat): Per https://goo.gl/AufHP, "The feed may not contain all of the
// user's contacts, because there's a default limit on the number of results
// returned."  Decide if 10000 is reasonable or not.
const char kGetContactsURL[] =
    "https://www.google.com/m8/feeds/contacts/default/full"
    "?alt=json&showdeleted=true&max-results=10000";

// Query parameter optionally appended to |kGetContactsURL| to return contacts
// from a specific group (as opposed to all contacts).
const char kGetContactsGroupParam[] = "group";

// Query parameter optionally appended to |kGetContactsURL| to return only
// recently-updated contacts.
const char kGetContactsUpdatedMinParam[] = "updated-min";

}  // namespace

//========================== GetContactGroupsRequest =========================

GetContactGroupsRequest::GetContactGroupsRequest(
    RequestSender* runner,
    const GetDataCallback& callback)
    : GetDataRequest(runner, callback) {
}

GetContactGroupsRequest::~GetContactGroupsRequest() {}

GURL GetContactGroupsRequest::GetURL() const {
  return !feed_url_for_testing_.is_empty() ?
         feed_url_for_testing_ :
         GURL(kGetContactGroupsURL);
}

//============================ GetContactsRequest ============================

GetContactsRequest::GetContactsRequest(
    RequestSender* runner,
    const std::string& group_id,
    const base::Time& min_update_time,
    const GetDataCallback& callback)
    : GetDataRequest(runner, callback),
      group_id_(group_id),
      min_update_time_(min_update_time) {
}

GetContactsRequest::~GetContactsRequest() {}

GURL GetContactsRequest::GetURL() const {
  if (!feed_url_for_testing_.is_empty())
    return GURL(feed_url_for_testing_);

  GURL url(kGetContactsURL);

  if (!group_id_.empty()) {
    url = net::AppendQueryParameter(url, kGetContactsGroupParam, group_id_);
  }
  if (!min_update_time_.is_null()) {
    std::string time_rfc3339 = util::FormatTimeAsString(min_update_time_);
    url = net::AppendQueryParameter(
        url, kGetContactsUpdatedMinParam, time_rfc3339);
  }
  return url;
}

//========================== GetContactPhotoRequest ==========================

GetContactPhotoRequest::GetContactPhotoRequest(
    RequestSender* runner,
    const GURL& photo_url,
    const GetContentCallback& callback)
    : UrlFetchRequestBase(runner),
      photo_url_(photo_url),
      callback_(callback) {
}

GetContactPhotoRequest::~GetContactPhotoRequest() {}

GURL GetContactPhotoRequest::GetURL() const {
  return photo_url_;
}

void GetContactPhotoRequest::ProcessURLFetchResults(
    const net::URLFetcher* source) {
  GDataErrorCode code = GetErrorCode(source);
  scoped_ptr<std::string> data(new std::string);
  source->GetResponseAsString(data.get());
  callback_.Run(code, data.Pass());
  OnProcessURLFetchResultsComplete();
}

void GetContactPhotoRequest::RunCallbackOnPrematureFailure(
    GDataErrorCode code) {
  scoped_ptr<std::string> data(new std::string);
  callback_.Run(code, data.Pass());
}

}  // namespace google_apis
