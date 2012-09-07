// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_CONTACTS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_CONTACTS_SERVICE_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"

class Profile;

namespace base {
class Value;
}

namespace contacts {
class Contact;
}

namespace gdata {

class AuthService;
class OperationRunner;

// Interface for fetching a user's Google contacts via the Contacts API
// (described at https://developers.google.com/google-apps/contacts/v3/).
class GDataContactsServiceInterface {
 public:
  typedef base::Callback<void(scoped_ptr<ScopedVector<contacts::Contact> >)>
      SuccessCallback;
  typedef base::Closure FailureCallback;

  virtual ~GDataContactsServiceInterface() {}

  virtual void Initialize() = 0;

  // Downloads all contacts changed at or after |min_update_time| and invokes
  // the appropriate callback asynchronously on the UI thread when complete.  If
  // min_update_time.is_null() is true, all contacts will be returned.
  virtual void DownloadContacts(SuccessCallback success_callback,
                                FailureCallback failure_callback,
                                const base::Time& min_update_time) = 0;

 protected:
  GDataContactsServiceInterface() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(GDataContactsServiceInterface);
};

class GDataContactsService : public GDataContactsServiceInterface {
 public:
  typedef base::Callback<std::string(const std::string&)>
      RewritePhotoUrlCallback;

  explicit GDataContactsService(Profile* profile);
  virtual ~GDataContactsService();

  AuthService* auth_service_for_testing();

  const std::string& cached_my_contacts_group_id_for_testing() const {
    return cached_my_contacts_group_id_;
  }
  void clear_cached_my_contacts_group_id_for_testing() {
    cached_my_contacts_group_id_.clear();
  }

  void set_max_photo_downloads_per_second_for_testing(int max_downloads) {
    max_photo_downloads_per_second_ = max_downloads;
  }
  void set_photo_download_timer_interval_for_testing(base::TimeDelta interval) {
    photo_download_timer_interval_ = interval;
  }
  void set_groups_feed_url_for_testing(const GURL& url) {
    groups_feed_url_for_testing_ = url;
  }
  void set_contacts_feed_url_for_testing(const GURL& url) {
    contacts_feed_url_for_testing_ = url;
  }
  void set_rewrite_photo_url_callback_for_testing(RewritePhotoUrlCallback cb) {
    rewrite_photo_url_callback_for_testing_ = cb;
  }

  // Overridden from GDataContactsServiceInterface:
  virtual void Initialize() OVERRIDE;
  virtual void DownloadContacts(SuccessCallback success_callback,
                                FailureCallback failure_callback,
                                const base::Time& min_update_time) OVERRIDE;

 private:
  class DownloadContactsRequest;

  // Invoked by a download request once it's finished (either successfully or
  // unsuccessfully).
  void OnRequestComplete(DownloadContactsRequest* request);

  Profile* profile_;  // not owned

  scoped_ptr<OperationRunner> runner_;

  // Group ID for the "My Contacts" system contacts group.
  // Cached after a DownloadContactsRequest has completed.
  std::string cached_my_contacts_group_id_;

  // In-progress download requests.  Pointers are owned by this class.
  std::set<DownloadContactsRequest*> requests_;

  // Maximum number of photos we'll try to download per second (per
  // DownloadContacts() request).
  int max_photo_downloads_per_second_;

  // Amount of time that we'll wait between waves of photo download requests.
  // This is usually one second (see |max_photo_downloads_per_second_|) but can
  // be set to a lower value for tests to make them complete more quickly.
  base::TimeDelta photo_download_timer_interval_;

  // If non-empty, URLs that will be used to fetch feeds.
  GURL groups_feed_url_for_testing_;
  GURL contacts_feed_url_for_testing_;

  // Callback that's invoked to rewrite photo URLs for tests.
  // This is needed for tests that serve static feed data from a host/port
  // that's only known at runtime.
  RewritePhotoUrlCallback rewrite_photo_url_callback_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(GDataContactsService);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_CONTACTS_SERVICE_H_
