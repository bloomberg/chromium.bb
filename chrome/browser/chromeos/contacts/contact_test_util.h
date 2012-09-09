// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CONTACTS_CONTACT_TEST_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_CONTACTS_CONTACT_TEST_UTIL_H_

#include <string>

#include "base/memory/scoped_vector.h"
#include "chrome/browser/chromeos/contacts/contact.pb.h"
#include "ui/gfx/size.h"

namespace contacts {

typedef std::vector<const Contact*> ContactPointers;
class ContactMap;

namespace test {

// Returns a string containing the information stored in |contact|.  The same
// string will be returned for functionally-equivalent contacts (e.g. ones
// containing the same email addresses but in a different order).
std::string ContactToString(const Contact& contact);

// Runs ContactToString() on each entry in |contacts| and returns the results
// joined by newlines (in a consistent order).
std::string ContactsToString(const ContactPointers& contacts);
std::string ContactsToString(const ScopedVector<Contact>& contacts);

// Convenience wrapper for ContactsToString().  Takes |num_contacts|
// const Contact* arguments.
std::string VarContactsToString(int num_contacts, ...);

// Like ContactsToStrings(), but takes a ContactMap as input.
std::string ContactMapToString(const ContactMap& contact_map);

// Saves copies of all contacts in |source| to |dest|.
void CopyContacts(const ContactPointers& source,
                  ScopedVector<Contact>* dest);
void CopyContacts(const ScopedVector<Contact>& source,
                  ScopedVector<Contact>* dest);

// Initializes |contact| with the passed-in data.  The photo and all address
// fields are cleared.  |contact_id| corresponds to Contact::contact_id,
// |deleted| to Contact::deleted, and a unique string should be passed to
// |name_suffix| to make the name-related fields be distinct from those in other
// contacts.
void InitContact(const std::string& contact_id,
                 const std::string& name_suffix,
                 bool deleted,
                 Contact* contact);

// Adds an email address to |contact|.
void AddEmailAddress(const std::string& address,
                     Contact_AddressType_Relation relation,
                     const std::string& label,
                     bool primary,
                     Contact* contact);

// Adds a phone number to |contact|.
void AddPhoneNumber(const std::string& number,
                    Contact_AddressType_Relation relation,
                    const std::string& label,
                    bool primary,
                    Contact* contact);

// Adds a postal address to |contact|.
void AddPostalAddress(const std::string& address,
                      Contact_AddressType_Relation relation,
                      const std::string& label,
                      bool primary,
                      Contact* contact);

// Adds an IM address to |contact|.
void AddInstantMessagingAddress(
    const std::string& address,
    Contact_InstantMessagingAddress_Protocol protocol,
    Contact_AddressType_Relation relation,
    const std::string& label,
    bool primary,
    Contact* contact);

// Initializes |contact|'s photo to a bitmap of the given size.
// ContactToString() includes the photo's dimensions in its output, so tests can
// call this method to set the photo to a given size and then check that the
// size matches later (e.g. after loading the contact from a server or from
// disk) to confirm that the photo was loaded correctly.
void SetPhoto(const gfx::Size& size, Contact* contact);

}  // namespace test
}  // namespace contacts

#endif  // CHROME_BROWSER_CHROMEOS_CONTACTS_CONTACT_TEST_UTIL_H_
