// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/contacts/contact_map.h"

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/time.h"
#include "chrome/browser/chromeos/contacts/contact.pb.h"
#include "chrome/browser/chromeos/contacts/contact_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contacts {
namespace test {

TEST(ContactMapTest, Merge) {
  ContactMap map;
  EXPECT_TRUE(map.empty());
  EXPECT_EQ(0U, map.size());
  EXPECT_EQ(base::Time(), map.GetMaxUpdateTime());

  // Create a contact.
  const std::string kContactId1 = "contact_id_1";
  scoped_ptr<Contact> contact1(new Contact);
  InitContact(kContactId1, "1", false, contact1.get());
  const base::Time kUpdateTime1 = base::Time::FromInternalValue(100);
  contact1->set_update_time(kUpdateTime1.ToInternalValue());

  // Merge it into the map and check that it's stored.
  scoped_ptr<ScopedVector<Contact> > contacts_to_merge(
      new ScopedVector<Contact>);
  contacts_to_merge->push_back(new Contact(*contact1));
  map.Merge(contacts_to_merge.Pass(), ContactMap::KEEP_DELETED_CONTACTS);
  EXPECT_FALSE(map.empty());
  EXPECT_EQ(1U, map.size());
  ASSERT_TRUE(map.Find(kContactId1));
  EXPECT_EQ(ContactMapToString(map), VarContactsToString(1, contact1.get()));
  EXPECT_EQ(kUpdateTime1, map.GetMaxUpdateTime());

  // Create a second, deleted contact with a newer update time.
  const std::string kContactId2 = "contact_id_2";
  scoped_ptr<Contact> contact2(new Contact);
  InitContact(kContactId2, "2", true, contact2.get());
  const base::Time kUpdateTime2 =
      kUpdateTime1 + base::TimeDelta::FromSeconds(10);
  contact2->set_update_time(kUpdateTime2.ToInternalValue());

  // Merge it into the map.  Since we request keeping deleted contacts, the
  // contact should be saved.
  contacts_to_merge.reset(new ScopedVector<Contact>);
  contacts_to_merge->push_back(new Contact(*contact2));
  map.Merge(contacts_to_merge.Pass(), ContactMap::KEEP_DELETED_CONTACTS);
  EXPECT_EQ(2U, map.size());
  ASSERT_TRUE(map.Find(kContactId2));
  EXPECT_EQ(ContactMapToString(map),
            VarContactsToString(2, contact1.get(), contact2.get()));
  EXPECT_EQ(kUpdateTime2, map.GetMaxUpdateTime());

  // Update the first contact's update time and merge it into the map.
  const base::Time kNewUpdateTime1 =
      kUpdateTime2 + base::TimeDelta::FromSeconds(20);
  contact1->set_update_time(kNewUpdateTime1.ToInternalValue());
  contacts_to_merge.reset(new ScopedVector<Contact>);
  contacts_to_merge->push_back(new Contact(*contact1));
  map.Merge(contacts_to_merge.Pass(), ContactMap::KEEP_DELETED_CONTACTS);
  EXPECT_EQ(ContactMapToString(map),
            VarContactsToString(2, contact1.get(), contact2.get()));
  EXPECT_EQ(kNewUpdateTime1, map.GetMaxUpdateTime());

  // Create another deleted contact.
  const std::string kContactId3 = "contact_id_3";
  scoped_ptr<Contact> contact3(new Contact);
  InitContact(kContactId3, "3", true, contact3.get());
  const base::Time kUpdateTime3 =
      kNewUpdateTime1 + base::TimeDelta::FromSeconds(40);
  contact3->set_update_time(kUpdateTime3.ToInternalValue());

  // Merge it into the map with DROP_DELETED_CONTACTS.  The contact shouldn't be
  // saved.
  contacts_to_merge.reset(new ScopedVector<Contact>);
  contacts_to_merge->push_back(new Contact(*contact3));
  map.Merge(contacts_to_merge.Pass(), ContactMap::DROP_DELETED_CONTACTS);
  EXPECT_EQ(ContactMapToString(map),
            VarContactsToString(2, contact1.get(), contact2.get()));
  EXPECT_EQ(kNewUpdateTime1, map.GetMaxUpdateTime());

  // Mark the first contact as being deleted and merge it with
  // DROP_DELETED_CONTACTS.  The previous version of the contact should also be
  // removed.
  contact1->set_deleted(true);
  contacts_to_merge.reset(new ScopedVector<Contact>);
  contacts_to_merge->push_back(new Contact(*contact1));
  map.Merge(contacts_to_merge.Pass(), ContactMap::DROP_DELETED_CONTACTS);
  EXPECT_EQ(ContactMapToString(map), VarContactsToString(1, contact2.get()));
  EXPECT_EQ(kUpdateTime2, map.GetMaxUpdateTime());

  map.Clear();
  EXPECT_TRUE(map.empty());
  EXPECT_EQ(0U, map.size());
  EXPECT_EQ(base::Time(), map.GetMaxUpdateTime());
}

}  // namespace test
}  // namespace contacts
