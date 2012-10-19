// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CONTACTS_GDATA_CONTACTS_SERVICE_STUB_H_
#define CHROME_BROWSER_CHROMEOS_CONTACTS_GDATA_CONTACTS_SERVICE_STUB_H_

#include "chrome/browser/chromeos/contacts/gdata_contacts_service.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/time.h"

namespace contacts {
typedef std::vector<const contacts::Contact*> ContactPointers;

// "Stub" implementation of GDataContactsServiceInterface used for testing.
// Returns a pre-set list of contacts in response to DownloadContacts() calls.
class GDataContactsServiceStub : public GDataContactsServiceInterface {
 public:
  GDataContactsServiceStub();
  virtual ~GDataContactsServiceStub();

  int num_download_requests() const { return num_download_requests_; }
  int num_download_requests_with_wrong_timestamps() const {
    return num_download_requests_with_wrong_timestamps_;
  }
  void reset_stats() {
    num_download_requests_ = 0;
    num_download_requests_with_wrong_timestamps_ = 0;
  }
  void set_download_should_succeed(bool succeed) {
    download_should_succeed_ = succeed;
  }

  // Sets the contacts that will be returned by DownloadContacts(), assuming
  // that the request's |min_update_time| matches |expected_min_update_time|.
  void SetContacts(const contacts::ContactPointers& contacts,
                   const base::Time& expected_min_update_time);

  // Overridden from GDataContactsServiceInterface:
  virtual void Initialize() OVERRIDE;
  virtual void DownloadContacts(SuccessCallback success_callback,
                                FailureCallback failure_callback,
                                const base::Time& min_update_time) OVERRIDE;

 private:
  // How many times has DownloadContacts() been called?
  int num_download_requests_;

  // How many times has DownloadContacts() been called with a |min_update_time|
  // parameter that doesn't match |expected_min_update_time_|?
  int num_download_requests_with_wrong_timestamps_;

  // Should calls to DownloadContacts() succeed?
  bool download_should_succeed_;

  // Contacts to be returned by calls to DownloadContacts().
  ScopedVector<contacts::Contact> contacts_;

  // |min_update_time| value that we expect to be passed to DownloadContacts().
  // If a different value is passed, we'll log an error and report failure.
  base::Time expected_min_update_time_;

  DISALLOW_COPY_AND_ASSIGN(GDataContactsServiceStub);
};

}  // namespace contacts

#endif  // CHROME_BROWSER_CHROMEOS_CONTACTS_GDATA_CONTACTS_SERVICE_STUB_H_
