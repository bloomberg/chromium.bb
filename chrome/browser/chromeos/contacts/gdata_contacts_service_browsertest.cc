// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/contacts/gdata_contacts_service.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "chrome/browser/chromeos/contacts/contact.pb.h"
#include "chrome/browser/chromeos/contacts/contact_test_util.h"
#include "chrome/browser/google_apis/auth_service.h"
#include "chrome/browser/google_apis/time_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "net/test/test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/size.h"

using content::BrowserThread;

namespace contacts {
namespace {

// Path to the files that are served by the test server.
const FilePath::CharType kTestDataPath[] =
    FILE_PATH_LITERAL("chrome/test/data");

// Base URL where feeds are located on the test server.
const char kFeedBaseUrl[] = "files/chromeos/gdata/contacts/";

// Filename of JSON feed containing contact groups.
const char kGroupsFeedFilename[] = "groups.json";

// Width and height of /photo.png on the test server.
const int kPhotoSize = 48;

// Initializes |contact| using the passed-in values.
void InitContact(const std::string& contact_id,
                 const std::string& rfc_3339_update_time,
                 bool deleted,
                 const std::string& full_name,
                 const std::string& given_name,
                 const std::string& additional_name,
                 const std::string& family_name,
                 const std::string& name_prefix,
                 const std::string& name_suffix,
                 contacts::Contact* contact) {
  DCHECK(contact);
  contact->set_contact_id(contact_id);
  base::Time update_time;
  CHECK(google_apis::util::GetTimeFromString(
      rfc_3339_update_time, &update_time))
      << "Unable to parse time \"" << rfc_3339_update_time << "\"";
  contact->set_update_time(update_time.ToInternalValue());
  contact->set_deleted(deleted);
  contact->set_full_name(full_name);
  contact->set_given_name(given_name);
  contact->set_additional_name(additional_name);
  contact->set_family_name(family_name);
  contact->set_name_prefix(name_prefix);
  contact->set_name_suffix(name_suffix);
}

class GDataContactsServiceTest : public InProcessBrowserTest {
 public:
  GDataContactsServiceTest()
      : InProcessBrowserTest(),
        test_server_(net::TestServer::TYPE_GDATA,
                     net::TestServer::kLocalhost,
                     FilePath(kTestDataPath)),
        download_was_successful_(false) {
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    ASSERT_TRUE(test_server_.Start());
    service_.reset(new GDataContactsService(
        browser()->profile()->GetRequestContext(),
        browser()->profile()));
    service_->Initialize();
    service_->auth_service_for_testing()->set_access_token_for_testing(
        net::TestServer::kGDataAuthToken);
    service_->set_rewrite_photo_url_callback_for_testing(
        base::Bind(&GDataContactsServiceTest::RewritePhotoUrl,
                   base::Unretained(this)));
    service_->set_groups_feed_url_for_testing(
        test_server_.GetURL(std::string(kFeedBaseUrl) + kGroupsFeedFilename));
    service_->set_photo_download_timer_interval_for_testing(
        base::TimeDelta::FromMilliseconds(10));
  }

  virtual void CleanUpOnMainThread() {
    service_.reset();
  }

 protected:
  // Downloads contacts from |feed_filename| (within the chromeos/gdata/contacts
  // test data directory).  |min_update_time| is appended to the URL and the
  // resulting contacts are swapped into |contacts|.  Returns false if the
  // download failed.
  bool Download(const std::string& feed_filename,
                const base::Time& min_update_time,
                scoped_ptr<ScopedVector<contacts::Contact> >* contacts) {
    DCHECK(contacts);
    service_->set_contacts_feed_url_for_testing(
        test_server_.GetURL(kFeedBaseUrl + feed_filename));
    service_->DownloadContacts(
        base::Bind(&GDataContactsServiceTest::OnSuccess,
                   base::Unretained(this)),
        base::Bind(&GDataContactsServiceTest::OnFailure,
                   base::Unretained(this)),
        min_update_time);
    content::RunMessageLoop();
    contacts->swap(downloaded_contacts_);
    return download_was_successful_;
  }

  net::TestServer test_server_;
  scoped_ptr<GDataContactsService> service_;

 private:
  // Rewrites |original_url|, a photo URL from a contacts feed, to instead point
  // at a file on |test_server_|.
  std::string RewritePhotoUrl(const std::string& original_url) {
    return test_server_.GetURL(kFeedBaseUrl + GURL(original_url).path()).spec();
  }

  // Handles success for Download().
  void OnSuccess(scoped_ptr<ScopedVector<contacts::Contact> > contacts) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    download_was_successful_ = true;
    downloaded_contacts_.swap(contacts);
    MessageLoop::current()->Quit();
  }

  // Handles failure for Download().
  void OnFailure() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    download_was_successful_ = false;
    downloaded_contacts_.reset(new ScopedVector<contacts::Contact>());
    MessageLoop::current()->Quit();
  }

  // Was the last download successful?  Used to pass the result back from
  // OnSuccess() and OnFailure() to Download().
  bool download_was_successful_;

  // Used to pass downloaded contacts back to Download().
  scoped_ptr<ScopedVector<contacts::Contact> > downloaded_contacts_;
};

}  // namespace

// Test that we report failure for feeds that are broken in various ways.
IN_PROC_BROWSER_TEST_F(GDataContactsServiceTest, BrokenFeeds) {
  scoped_ptr<ScopedVector<contacts::Contact> > contacts;
  EXPECT_FALSE(Download("some_bogus_file", base::Time(), &contacts));
  EXPECT_FALSE(Download("empty.txt", base::Time(), &contacts));
  EXPECT_FALSE(Download("not_json.txt", base::Time(), &contacts));
  EXPECT_FALSE(Download("not_dictionary.json", base::Time(), &contacts));
  EXPECT_FALSE(Download("no_feed.json", base::Time(), &contacts));
  EXPECT_FALSE(Download("no_category.json", base::Time(), &contacts));
  EXPECT_FALSE(Download("wrong_category.json", base::Time(), &contacts));

  // Missing photos should be allowed, though (as this can occur in production).
  EXPECT_TRUE(Download("feed_photo_404.json", base::Time(), &contacts));
  ASSERT_EQ(static_cast<size_t>(1), contacts->size());
  EXPECT_FALSE((*contacts)[0]->has_raw_untrusted_photo());

  // We should report failure when we're unable to download the contact group
  // feed.
  service_->clear_cached_my_contacts_group_id_for_testing();
  service_->set_groups_feed_url_for_testing(
      test_server_.GetURL(std::string(kFeedBaseUrl) + "404"));
  EXPECT_FALSE(Download("feed.json", base::Time(), &contacts));
  EXPECT_TRUE(service_->cached_my_contacts_group_id_for_testing().empty());

  // We should also fail when the "My Contacts" group isn't listed in the group
  // feed.
  service_->clear_cached_my_contacts_group_id_for_testing();
  service_->set_groups_feed_url_for_testing(
      test_server_.GetURL(std::string(kFeedBaseUrl) +
                          "groups_no_my_contacts.json"));
  EXPECT_FALSE(Download("feed.json", base::Time(), &contacts));
  EXPECT_TRUE(service_->cached_my_contacts_group_id_for_testing().empty());
}

// Check that we're able to download an empty feed and a normal-looking feed
// with two regular contacts and one deleted one.
IN_PROC_BROWSER_TEST_F(GDataContactsServiceTest, Download) {
  scoped_ptr<ScopedVector<contacts::Contact> > contacts;
  EXPECT_TRUE(Download("no_entries.json", base::Time(), &contacts));
  EXPECT_TRUE(contacts->empty());

  EXPECT_TRUE(Download("feed.json", base::Time(), &contacts));

  // Check that we got the group ID for the "My Contacts" group that's hardcoded
  // in the groups feed.
  EXPECT_EQ(
      "http://www.google.com/m8/feeds/groups/test.user%40gmail.com/base/6",
      service_->cached_my_contacts_group_id_for_testing());

  // All of these expected values are hardcoded in the feed.
  scoped_ptr<contacts::Contact> contact1(new contacts::Contact);
  InitContact("http://example.com/1",
              "2012-06-04T15:53:36.023Z",
              false, "Joe Contact", "Joe", "", "Contact", "", "",
              contact1.get());
  contacts::test::SetPhoto(gfx::Size(kPhotoSize, kPhotoSize), contact1.get());
  contacts::test::AddEmailAddress(
      "joe.contact@gmail.com",
      contacts::Contact_AddressType_Relation_OTHER, "", true, contact1.get());
  contacts::test::AddPostalAddress(
      "345 Spear St\nSan Francisco CA 94105",
      contacts::Contact_AddressType_Relation_HOME, "", false, contact1.get());

  scoped_ptr<contacts::Contact> contact2(new contacts::Contact);
  InitContact("http://example.com/2",
              "2012-06-21T16:20:13.208Z",
              false, "Dr. Jane Liz Doe Sr.", "Jane", "Liz", "Doe", "Dr.", "Sr.",
              contact2.get());
  contacts::test::AddEmailAddress(
      "jane.doe@gmail.com",
      contacts::Contact_AddressType_Relation_HOME, "", true, contact2.get());
  contacts::test::AddEmailAddress(
      "me@privacy.net",
      contacts::Contact_AddressType_Relation_WORK, "", false, contact2.get());
  contacts::test::AddEmailAddress(
      "foo@example.org",
      contacts::Contact_AddressType_Relation_OTHER, "Fake", false,
      contact2.get());
  contacts::test::AddPhoneNumber(
      "123-456-7890",
      contacts::Contact_AddressType_Relation_MOBILE, "", false,
      contact2.get());
  contacts::test::AddPhoneNumber(
      "234-567-8901",
      contacts::Contact_AddressType_Relation_OTHER, "grandcentral", false,
      contact2.get());
  contacts::test::AddPostalAddress(
      "100 Elm St\nSan Francisco, CA 94110",
      contacts::Contact_AddressType_Relation_HOME, "", false, contact2.get());
  contacts::test::AddInstantMessagingAddress(
      "foo@example.org",
      contacts::Contact_InstantMessagingAddress_Protocol_GOOGLE_TALK,
      contacts::Contact_AddressType_Relation_OTHER, "", false,
      contact2.get());
  contacts::test::AddInstantMessagingAddress(
      "12345678",
      contacts::Contact_InstantMessagingAddress_Protocol_ICQ,
      contacts::Contact_AddressType_Relation_OTHER, "", false,
      contact2.get());

  scoped_ptr<contacts::Contact> contact3(new contacts::Contact);
  InitContact("http://example.com/3",
              "2012-07-23T23:07:06.133Z",
              true, "", "", "", "", "", "",
              contact3.get());

  EXPECT_EQ(contacts::test::VarContactsToString(
                3, contact1.get(), contact2.get(), contact3.get()),
            contacts::test::ContactsToString(*contacts));
}

// Download a feed containing more photos than we're able to download in
// parallel to check that we still end up with all the photos.
IN_PROC_BROWSER_TEST_F(GDataContactsServiceTest, ParallelPhotoDownload) {
  // The feed used for this test contains 8 contacts.
  const int kNumContacts = 8;
  service_->set_max_photo_downloads_per_second_for_testing(6);
  scoped_ptr<ScopedVector<contacts::Contact> > contacts;
  EXPECT_TRUE(Download("feed_multiple_photos.json", base::Time(), &contacts));
  ASSERT_EQ(static_cast<size_t>(kNumContacts), contacts->size());

  ScopedVector<contacts::Contact> expected_contacts;
  for (int i = 0; i < kNumContacts; ++i) {
    contacts::Contact* contact = new contacts::Contact;
    InitContact(base::StringPrintf("http://example.com/%d", i + 1),
                "2012-06-04T15:53:36.023Z",
                false, "", "", "", "", "", "", contact);
    contacts::test::SetPhoto(gfx::Size(kPhotoSize, kPhotoSize), contact);
    expected_contacts.push_back(contact);
  }
  EXPECT_EQ(contacts::test::ContactsToString(expected_contacts),
            contacts::test::ContactsToString(*contacts));
}

IN_PROC_BROWSER_TEST_F(GDataContactsServiceTest, UnicodeStrings) {
  scoped_ptr<ScopedVector<contacts::Contact> > contacts;
  EXPECT_TRUE(Download("feed_unicode.json", base::Time(), &contacts));

  // All of these expected values are hardcoded in the feed.
  scoped_ptr<contacts::Contact> contact1(new contacts::Contact);
  InitContact("http://example.com/1", "2012-06-04T15:53:36.023Z",
              false, "\xE5\xAE\x89\xE8\x97\xA4\x20\xE5\xBF\xA0\xE9\x9B\x84",
              "\xE5\xBF\xA0\xE9\x9B\x84", "", "\xE5\xAE\x89\xE8\x97\xA4",
              "", "", contact1.get());
  scoped_ptr<contacts::Contact> contact2(new contacts::Contact);
  InitContact("http://example.com/2", "2012-06-21T16:20:13.208Z",
              false, "Bob Smith", "Bob", "", "Smith", "", "",
              contact2.get());
  EXPECT_EQ(contacts::test::VarContactsToString(
                2, contact1.get(), contact2.get()),
            contacts::test::ContactsToString(*contacts));
}

}  // namespace contacts
