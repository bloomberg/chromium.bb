// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/contacts/contact_test_util.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/time.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/size.h"

namespace contacts {
namespace test {

namespace {

// Invokes |stringify_callback| on each item in |items| and prepends |prefix|,
// and then sorts the resulting strings and joins them using |join_char|.
template<class T>
std::string StringifyField(
    const std::vector<T>& items,
    base::Callback<std::string(const T&)> stringify_callback,
    const std::string& prefix,
    char join_char) {
  std::vector<std::string> strings;
  for (size_t i = 0; i < items.size(); ++i)
    strings.push_back(prefix + stringify_callback.Run(items[i]));
  std::sort(strings.begin(), strings.end());
  return JoinString(strings, join_char);
}

std::string EmailAddressToString(const Contact::EmailAddress& email) {
  return email.address + "," +
         base::IntToString(email.type.relation) + "," +
         email.type.label + "," +
         base::IntToString(email.primary);
}

std::string PhoneNumberToString(const Contact::PhoneNumber& phone) {
  return phone.number + "," +
         base::IntToString(phone.type.relation) + "," +
         phone.type.label + "," +
         base::IntToString(phone.primary);
}

std::string PostalAddressToString(const Contact::PostalAddress& postal) {
  return postal.address + "," +
         base::IntToString(postal.type.relation) + "," +
         postal.type.label + "," +
         base::IntToString(postal.primary);
}

std::string InstantMessagingAddressToString(
    const Contact::InstantMessagingAddress& im) {
  return im.address + "," +
         base::IntToString(im.protocol) + "," +
         base::IntToString(im.type.relation) + "," +
         im.type.label + "," +
         base::IntToString(im.primary);
}

}  // namespace

std::string ContactToString(const Contact& contact) {
  std::string result =
      contact.provider_id + "," +
      base::Int64ToString(contact.update_time.ToInternalValue()) + "," +
      base::IntToString(contact.deleted) + "," +
      contact.full_name + "," +
      contact.given_name + "," +
      contact.additional_name + "," +
      contact.family_name + "," +
      contact.name_prefix + "," +
      contact.name_suffix + "," +
      base::IntToString(contact.photo.width()) + "x" +
      base::IntToString(contact.photo.height());

  result += " " + StringifyField(contact.email_addresses,
                                 base::Bind(EmailAddressToString),
                                 "email=", ' ');
  result += " " + StringifyField(contact.phone_numbers,
                                 base::Bind(PhoneNumberToString),
                                 "phone=", ' ');
  result += " " + StringifyField(contact.postal_addresses,
                                 base::Bind(PostalAddressToString),
                                 "postal=", ' ');
  result += " " + StringifyField(contact.instant_messaging_addresses,
                                 base::Bind(InstantMessagingAddressToString),
                                 "im=", ' ');

  return result;
}

std::string ContactsToString(const ContactPointers& contacts) {
  std::vector<std::string> contact_strings;
  for (size_t i = 0; i < contacts.size(); ++i)
    contact_strings.push_back(ContactToString(*contacts[i]));
  std::sort(contact_strings.begin(), contact_strings.end());
  return JoinString(contact_strings, '\n');
}

std::string ContactsToString(const ScopedVector<Contact>& contacts) {
  ContactPointers pointers;
  for (size_t i = 0; i < contacts.size(); ++i)
    pointers.push_back(contacts[i]);
  return ContactsToString(pointers);
}

std::string VarContactsToString(int num_contacts, ...) {
  ContactPointers contacts;
  va_list list;
  va_start(list, num_contacts);
  for (int i = 0; i < num_contacts; ++i)
    contacts.push_back(va_arg(list, const Contact*));
  va_end(list);
  return ContactsToString(contacts);
}

void CopyContact(const Contact& source, Contact* dest) {
  dest->provider_id = source.provider_id;
  dest->update_time = source.update_time;
  dest->deleted = source.deleted;
  dest->full_name = source.full_name;
  dest->given_name = source.given_name;
  dest->additional_name = source.additional_name;
  dest->family_name = source.family_name;
  dest->name_prefix = source.name_prefix;
  dest->name_suffix = source.name_suffix;
  dest->photo = source.photo;
  dest->email_addresses = source.email_addresses;
  dest->phone_numbers = source.phone_numbers;
  dest->postal_addresses = source.postal_addresses;
  dest->instant_messaging_addresses = source.instant_messaging_addresses;
}

void CopyContacts(const ContactPointers& source,
                  ScopedVector<Contact>* dest) {
  DCHECK(dest);
  dest->clear();
  for (size_t i = 0; i < source.size(); ++i) {
    Contact* contact = new Contact;
    CopyContact(*source[i], contact);
    dest->push_back(contact);
  }
}

void CopyContacts(const ScopedVector<Contact>& source,
                  ScopedVector<Contact>* dest) {
  ContactPointers pointers;
  for (size_t i = 0; i < source.size(); ++i)
    pointers.push_back(source[i]);
  CopyContacts(pointers, dest);
}

void InitContact(const std::string& provider_id,
                 const std::string& name_suffix,
                 bool deleted,
                 Contact* contact) {
  DCHECK(contact);
  contact->provider_id = provider_id;
  contact->update_time = base::Time::Now();
  contact->deleted = deleted;
  contact->full_name = "full_name_" + name_suffix;
  contact->given_name = "given_name_" + name_suffix;
  contact->additional_name = "additional_name_" + name_suffix;
  contact->family_name = "family_name_" + name_suffix;
  contact->name_prefix = "name_prefix_" + name_suffix;
  contact->name_suffix = "name_suffix_" + name_suffix;
  contact->photo = SkBitmap();
  contact->email_addresses.clear();
  contact->phone_numbers.clear();
  contact->postal_addresses.clear();
  contact->instant_messaging_addresses.clear();
}

void AddEmailAddress(const std::string& address,
                     Contact::AddressType::Relation relation,
                     const std::string& label,
                     bool primary,
                     Contact* contact) {
  DCHECK(contact);
  Contact::EmailAddress email;
  email.address = address;
  email.type.relation = relation;
  email.type.label = label;
  email.primary = primary;
  contact->email_addresses.push_back(email);
}

void AddPhoneNumber(const std::string& number,
                    Contact::AddressType::Relation relation,
                    const std::string& label,
                    bool primary,
                    Contact* contact) {
  DCHECK(contact);
  Contact::PhoneNumber phone;
  phone.number = number;
  phone.type.relation = relation;
  phone.type.label = label;
  phone.primary = primary;
  contact->phone_numbers.push_back(phone);
}

void AddPostalAddress(const std::string& address,
                      Contact::AddressType::Relation relation,
                      const std::string& label,
                      bool primary,
                      Contact* contact) {
  DCHECK(contact);
  Contact::PostalAddress postal;
  postal.address = address;
  postal.type.relation = relation;
  postal.type.label = label;
  postal.primary = primary;
  contact->postal_addresses.push_back(postal);
}

void AddInstantMessagingAddress(
    const std::string& address,
    Contact::InstantMessagingAddress::Protocol protocol,
    Contact::AddressType::Relation relation,
    const std::string& label,
    bool primary,
    Contact* contact) {
  DCHECK(contact);
  Contact::InstantMessagingAddress im;
  im.address = address;
  im.protocol = protocol;
  im.type.relation = relation;
  im.type.label = label;
  im.primary = primary;
  contact->instant_messaging_addresses.push_back(im);
}

void SetPhoto(const gfx::Size& size, Contact* contact) {
  DCHECK(contact);
  contact->photo.setConfig(
      SkBitmap::kARGB_8888_Config, size.width(), size.height());
  contact->photo.allocPixels();
}

}  // namespace test
}  // namespace contacts
