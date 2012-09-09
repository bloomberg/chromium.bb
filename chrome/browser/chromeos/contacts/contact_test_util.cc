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
#include "chrome/browser/chromeos/contacts/contact_map.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/size.h"

namespace contacts {
namespace test {

namespace {

// Invokes |stringify_callback| on each item in |items| and prepends |prefix|,
// and then sorts the resulting strings and joins them using |join_char|.
template<class T>
std::string StringifyField(
    const ::google::protobuf::RepeatedPtrField<T>& items,
    base::Callback<std::string(const T&)> stringify_callback,
    const std::string& prefix,
    char join_char) {
  std::vector<std::string> strings;
  for (int i = 0; i < items.size(); ++i)
    strings.push_back(prefix + stringify_callback.Run(items.Get(i)));
  std::sort(strings.begin(), strings.end());
  return JoinString(strings, join_char);
}

std::string EmailAddressToString(const Contact_EmailAddress& email) {
  return email.address() + "," +
         base::IntToString(email.type().relation()) + "," +
         email.type().label() + "," +
         base::IntToString(email.primary());
}

std::string PhoneNumberToString(const Contact_PhoneNumber& phone) {
  return phone.number() + "," +
         base::IntToString(phone.type().relation()) + "," +
         phone.type().label() + "," +
         base::IntToString(phone.primary());
}

std::string PostalAddressToString(const Contact_PostalAddress& postal) {
  return postal.address() + "," +
         base::IntToString(postal.type().relation()) + "," +
         postal.type().label() + "," +
         base::IntToString(postal.primary());
}

std::string InstantMessagingAddressToString(
    const Contact_InstantMessagingAddress& im) {
  return im.address() + "," +
         base::IntToString(im.protocol()) + "," +
         base::IntToString(im.type().relation()) + "," +
         im.type().label() + "," +
         base::IntToString(im.primary());
}

}  // namespace

std::string ContactToString(const Contact& contact) {
  std::string result =
      contact.contact_id() + "," +
      base::Int64ToString(contact.update_time()) + "," +
      base::IntToString(contact.deleted()) + "," +
      contact.full_name() + "," +
      contact.given_name() + "," +
      contact.additional_name() + "," +
      contact.family_name() + "," +
      contact.name_prefix() + "," +
      contact.name_suffix();

  SkBitmap bitmap;
  if (contact.has_raw_untrusted_photo()) {
    // Testing code just uses PNG for now.  If that changes, use ImageDecoder
    // here instead.
    CHECK(gfx::PNGCodec::Decode(reinterpret_cast<const unsigned char*>(
                                    contact.raw_untrusted_photo().data()),
                                contact.raw_untrusted_photo().size(),
                                &bitmap));
  }
  result += "," + base::IntToString(bitmap.width()) + "x" +
            base::IntToString(bitmap.height());

  result += " " + StringifyField(contact.email_addresses(),
                                 base::Bind(EmailAddressToString),
                                 "email=", ' ');
  result += " " + StringifyField(contact.phone_numbers(),
                                 base::Bind(PhoneNumberToString),
                                 "phone=", ' ');
  result += " " + StringifyField(contact.postal_addresses(),
                                 base::Bind(PostalAddressToString),
                                 "postal=", ' ');
  result += " " + StringifyField(contact.instant_messaging_addresses(),
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

std::string ContactMapToString(const ContactMap& contact_map) {
  ContactPointers contacts;
  for (ContactMap::const_iterator it = contact_map.begin();
       it != contact_map.end(); ++it) {
    contacts.push_back(it->second);
  }
  return ContactsToString(contacts);
}

void CopyContacts(const ContactPointers& source,
                  ScopedVector<Contact>* dest) {
  DCHECK(dest);
  dest->clear();
  for (size_t i = 0; i < source.size(); ++i)
    dest->push_back(new Contact(*source[i]));
}

void CopyContacts(const ScopedVector<Contact>& source,
                  ScopedVector<Contact>* dest) {
  ContactPointers pointers;
  for (size_t i = 0; i < source.size(); ++i)
    pointers.push_back(source[i]);
  CopyContacts(pointers, dest);
}

void InitContact(const std::string& contact_id,
                 const std::string& name_suffix,
                 bool deleted,
                 Contact* contact) {
  DCHECK(contact);
  contact->Clear();
  contact->set_contact_id(contact_id);
  contact->set_update_time(base::Time::Now().ToInternalValue());
  contact->set_deleted(deleted);
  contact->set_full_name("full_name_" + name_suffix);
  contact->set_given_name("given_name_" + name_suffix);
  contact->set_additional_name("additional_name_" + name_suffix);
  contact->set_family_name("family_name_" + name_suffix);
  contact->set_name_prefix("name_prefix_" + name_suffix);
  contact->set_name_suffix("name_suffix_" + name_suffix);
}

void AddEmailAddress(const std::string& address,
                     Contact_AddressType_Relation relation,
                     const std::string& label,
                     bool primary,
                     Contact* contact) {
  DCHECK(contact);
  Contact::EmailAddress* email = contact->add_email_addresses();
  email->set_address(address);
  email->mutable_type()->set_relation(relation);
  email->mutable_type()->set_label(label);
  email->set_primary(primary);
}

void AddPhoneNumber(const std::string& number,
                    Contact_AddressType_Relation relation,
                    const std::string& label,
                    bool primary,
                    Contact* contact) {
  DCHECK(contact);
  Contact::PhoneNumber* phone = contact->add_phone_numbers();
  phone->set_number(number);
  phone->mutable_type()->set_relation(relation);
  phone->mutable_type()->set_label(label);
  phone->set_primary(primary);
}

void AddPostalAddress(const std::string& address,
                      Contact_AddressType_Relation relation,
                      const std::string& label,
                      bool primary,
                      Contact* contact) {
  DCHECK(contact);
  Contact::PostalAddress* postal = contact->add_postal_addresses();
  postal->set_address(address);
  postal->mutable_type()->set_relation(relation);
  postal->mutable_type()->set_label(label);
  postal->set_primary(primary);
}

void AddInstantMessagingAddress(
    const std::string& address,
    Contact_InstantMessagingAddress_Protocol protocol,
    Contact_AddressType_Relation relation,
    const std::string& label,
    bool primary,
    Contact* contact) {
  DCHECK(contact);
  Contact::InstantMessagingAddress* im =
      contact->add_instant_messaging_addresses();
  im->set_address(address);
  im->set_protocol(protocol);
  im->mutable_type()->set_relation(relation);
  im->mutable_type()->set_label(label);
  im->set_primary(primary);
}

void SetPhoto(const gfx::Size& size, Contact* contact) {
  DCHECK(contact);
  if (size.IsEmpty()) {
    contact->clear_raw_untrusted_photo();
    return;
  }

  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, size.width(), size.height());
  bitmap.allocPixels();
  SkCanvas canvas(bitmap);
  canvas.clear(SK_ColorBLACK);

  std::vector<unsigned char> png_photo;
  CHECK(gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &png_photo));
  contact->set_raw_untrusted_photo(&png_photo[0], png_photo.size());
}

}  // namespace test
}  // namespace contacts
