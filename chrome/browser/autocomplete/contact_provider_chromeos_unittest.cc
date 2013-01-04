// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/contact_provider_chromeos.h"

#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_input.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "chrome/browser/chromeos/contacts/contact.pb.h"
#include "chrome/browser/chromeos/contacts/contact_manager_stub.h"
#include "chrome/browser/chromeos/contacts/contact_test_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace {

// Initializes |contact| with the passed-in data.
void InitContact(const std::string& contact_id,
                 const std::string& full_name,
                 const std::string& given_name,
                 const std::string& family_name,
                 contacts::Contact* contact) {
  contact->set_contact_id(contact_id);
  contact->set_full_name(full_name);
  contact->set_given_name(given_name);
  contact->set_family_name(family_name);
}

}  // namespace

class ContactProviderTest : public testing::Test {
 public:
  ContactProviderTest() : ui_thread_(BrowserThread::UI, &message_loop_) {}
  virtual ~ContactProviderTest() {}

 protected:
  // testing::Test implementation.
  virtual void SetUp() OVERRIDE {
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("test_profile");
    contact_manager_.reset(new contacts::ContactManagerStub(profile_));
    contact_provider_ =
        new ContactProvider(NULL, profile_, contact_manager_->GetWeakPtr());
  }

  // Starts a (synchronous) query for |utf8_text| in |contact_provider_|.
  void StartQuery(const std::string& utf8_text) {
    contact_provider_->Start(
        AutocompleteInput(UTF8ToUTF16(utf8_text),
                          string16::npos,  // cursor_position
                          string16(),      // desired_tld
                          false,           // prevent_inline_autocomplete
                          false,           // prefer_keyword
                          false,           // allow_exact_keyword_match
                          AutocompleteInput::ALL_MATCHES),
        false);  // minimal_changes
  }

  // Returns the contact ID in |match|'s additional info, or an empty string if
  // no ID is present.
  std::string GetContactIdFromMatch(const AutocompleteMatch& match) {
    AutocompleteMatch::AdditionalInfo::const_iterator it =
        match.additional_info.find(ContactProvider::kMatchContactIdKey);
    return it != match.additional_info.end() ? it->second : std::string();
  }

  // Returns pointers to all of the Contact objects referenced in
  // |contact_provider_|'s current results.
  contacts::ContactPointers GetMatchedContacts() {
    contacts::ContactPointers contacts;
    const ACMatches& matches = contact_provider_->matches();
    for (size_t i = 0; i < matches.size(); ++i) {
      const contacts::Contact* contact = contact_manager_->GetContactById(
          profile_, GetContactIdFromMatch(matches[i]));
      DCHECK(contact) << "Unable to find contact for match " << i;
      contacts.push_back(contact);
    }
    return contacts;
  }

  // Returns a semicolon-separated string containing string representations (as
  // provided by AutocompleteMatch::ClassificationsToString()) of the
  // |contents_class| fields of all current matches.  Results are sorted by
  // contact ID.
  std::string GetMatchClassifications() {
    typedef std::map<std::string, std::string> StringMap;
    StringMap contact_id_classifications;
    const ACMatches& matches = contact_provider_->matches();
    for (size_t i = 0; i < matches.size(); ++i) {
      std::string id = GetContactIdFromMatch(matches[i]);
      DCHECK(!id.empty()) << "Match " << i << " lacks contact ID";
      contact_id_classifications[id] = AutocompleteMatch::
          ClassificationsToString(matches[i].contents_class);
    }

    std::string result;
    for (StringMap::const_iterator it = contact_id_classifications.begin();
         it != contact_id_classifications.end(); ++it) {
      if (!result.empty())
        result += ";";
      result += it->second;
    }
    return result;
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;

  scoped_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* profile_;

  scoped_ptr<contacts::ContactManagerStub> contact_manager_;
  scoped_refptr<ContactProvider> contact_provider_;
};

TEST_F(ContactProviderTest, BasicMatching) {
  const std::string kContactId1 = "contact_1";
  scoped_ptr<contacts::Contact> contact1(new contacts::Contact);
  InitContact(kContactId1, "Bob Smith", "Bob", "Smith", contact1.get());

  const std::string kContactId2 = "contact_2";
  scoped_ptr<contacts::Contact> contact2(new contacts::Contact);
  InitContact(kContactId2, "Dr. Jane Smith", "Jane", "Smith", contact2.get());

  contacts::ContactPointers contacts;
  contacts.push_back(contact1.get());
  contacts.push_back(contact2.get());
  contact_manager_->SetContacts(contacts);
  contact_manager_->NotifyObserversAboutUpdatedContacts();

  StartQuery("b");
  EXPECT_EQ(
      contacts::test::VarContactsToString(1, contact1.get()),
      contacts::test::ContactsToString(GetMatchedContacts()));
  EXPECT_EQ("0,2,1,0", GetMatchClassifications());

  StartQuery("bob");
  EXPECT_EQ(
      contacts::test::VarContactsToString(1, contact1.get()),
      contacts::test::ContactsToString(GetMatchedContacts()));
  EXPECT_EQ("0,2,3,0", GetMatchClassifications());

  StartQuery("bob smith");
  EXPECT_EQ(
      contacts::test::VarContactsToString(1, contact1.get()),
      contacts::test::ContactsToString(GetMatchedContacts()));
  EXPECT_EQ("0,2", GetMatchClassifications());

  StartQuery("sm");
  EXPECT_EQ(
      contacts::test::VarContactsToString(2, contact1.get(), contact2.get()),
      contacts::test::ContactsToString(GetMatchedContacts()));
  EXPECT_EQ("0,0,4,2,6,0;" "0,0,9,2,11,0", GetMatchClassifications());

  StartQuery("smith");
  EXPECT_EQ(
      contacts::test::VarContactsToString(2, contact1.get(), contact2.get()),
      contacts::test::ContactsToString(GetMatchedContacts()));
  EXPECT_EQ("0,0,4,2;" "0,0,9,2", GetMatchClassifications());

  StartQuery("smIth BOb");
  EXPECT_EQ(
      contacts::test::VarContactsToString(1, contact1.get()),
      contacts::test::ContactsToString(GetMatchedContacts()));
  EXPECT_EQ("0,2,3,0,4,2", GetMatchClassifications());

  StartQuery("bobo");
  EXPECT_EQ("", contacts::test::ContactsToString(GetMatchedContacts()));
  EXPECT_EQ("", GetMatchClassifications());

  StartQuery("mith");
  EXPECT_EQ("", contacts::test::ContactsToString(GetMatchedContacts()));
  EXPECT_EQ("", GetMatchClassifications());

  StartQuery("dr");
  EXPECT_EQ(
      contacts::test::VarContactsToString(1, contact2.get()),
      contacts::test::ContactsToString(GetMatchedContacts()));
  EXPECT_EQ("0,2,2,0", GetMatchClassifications());

  StartQuery("dr. j");
  EXPECT_EQ(
      contacts::test::VarContactsToString(1, contact2.get()),
      contacts::test::ContactsToString(GetMatchedContacts()));
  EXPECT_EQ("0,2,5,0", GetMatchClassifications());

  StartQuery("jane");
  EXPECT_EQ(
      contacts::test::VarContactsToString(1, contact2.get()),
      contacts::test::ContactsToString(GetMatchedContacts()));
  EXPECT_EQ("0,0,4,2,8,0", GetMatchClassifications());
}

TEST_F(ContactProviderTest, Collation) {
  scoped_ptr<contacts::Contact> contact(new contacts::Contact);
  InitContact("1", "Bj\xC3\xB6rn Adelsv\xC3\xA4rd",
              "Bj\xC3\xB6rn", "Adelsv\xC3\xA4rd",
              contact.get());

  contacts::ContactPointers contacts;
  contacts.push_back(contact.get());
  contact_manager_->SetContacts(contacts);
  contact_manager_->NotifyObserversAboutUpdatedContacts();

  StartQuery("bjorn");
  EXPECT_EQ(
      contacts::test::VarContactsToString(1, contact.get()),
      contacts::test::ContactsToString(GetMatchedContacts()));
  EXPECT_EQ("0,2,5,0", GetMatchClassifications());

  StartQuery("adelsvard");
  EXPECT_EQ(
      contacts::test::VarContactsToString(1, contact.get()),
      contacts::test::ContactsToString(GetMatchedContacts()));
  EXPECT_EQ("0,0,6,2", GetMatchClassifications());
}

TEST_F(ContactProviderTest, Relevance) {
  // Create more contacts than the maximum number of results that an
  // AutocompleteProvider should return.  Give them all the same family name and
  // ascending affinities from 0.0 to 1.0.
  const size_t kNumContacts = AutocompleteProvider::kMaxMatches + 1;
  const std::string kFamilyName = "Jones";

  ScopedVector<contacts::Contact> contacts;
  contacts::ContactPointers contact_pointers;
  for (size_t i = 0; i < kNumContacts; ++i) {
    contacts::Contact* contact = new contacts::Contact;
    std::string id_string = base::IntToString(static_cast<int>(i));
    InitContact(id_string, id_string, kFamilyName,
                id_string + " " + kFamilyName, contact);
    contact->set_affinity(static_cast<float>(i) / kNumContacts);
    contacts.push_back(contact);
    contact_pointers.push_back(contact);
  }

  contact_manager_->SetContacts(contact_pointers);
  contact_manager_->NotifyObserversAboutUpdatedContacts();

  // Do a search for the family name and check that the total number of results
  // is limited as expected and that the results are ordered by descending
  // affinity.
  StartQuery(kFamilyName);
  const ACMatches& matches = contact_provider_->matches();
  ASSERT_EQ(AutocompleteProvider::kMaxMatches, matches.size());

  int previous_relevance = 0;
  for (size_t i = 0; i < matches.size(); ++i) {
    const contacts::Contact& exp_contact =
        *(contacts[kNumContacts - 1 - i]);
    std::string match_id = GetContactIdFromMatch(matches[i]);
    EXPECT_EQ(exp_contact.contact_id(), match_id)
        << "Expected contact ID " << exp_contact.contact_id()
        << " for match " << i << " but got " << match_id << " instead";
    if (i > 0) {
      EXPECT_LE(matches[i].relevance, previous_relevance)
          << "Match " << i << " has greater relevance than previous match";
    }
    previous_relevance = matches[i].relevance;
  }
}
