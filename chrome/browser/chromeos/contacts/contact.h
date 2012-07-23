// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CONTACTS_CONTACT_H_
#define CHROME_BROWSER_CHROMEOS_CONTACTS_CONTACT_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/time.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace contacts {

// Struct representing a contact, roughly based on the GData Contact kind:
// https://developers.google.com/gdata/docs/2.0/elements#gdContactKind
// All strings are UTF-8.
struct Contact {
  // Describes an address-like field's type.
  struct AddressType {
    enum Relation {
      RELATION_HOME   = 0,
      RELATION_WORK   = 1,
      RELATION_MOBILE = 2,
      RELATION_OTHER  = 3,
    };

    AddressType();
    Relation relation;
    std::string label;
  };

  struct EmailAddress {
    EmailAddress();
    std::string address;
    AddressType type;
    bool primary;
  };

  struct PhoneNumber {
    PhoneNumber();
    std::string number;
    AddressType type;
    bool primary;
  };

  struct PostalAddress {
    PostalAddress();
    std::string address;
    AddressType type;
    bool primary;
  };

  struct InstantMessagingAddress {
    // Taken from https://developers.google.com/gdata/docs/2.0/elements#gdIm.
    enum Protocol {
      PROTOCOL_AIM         = 0,
      PROTOCOL_MSN         = 1,
      PROTOCOL_YAHOO       = 2,
      PROTOCOL_SKYPE       = 3,
      PROTOCOL_QQ          = 4,
      PROTOCOL_GOOGLE_TALK = 5,
      PROTOCOL_ICQ         = 6,
      PROTOCOL_JABBER      = 7,
      PROTOCOL_OTHER       = 8,
    };

    InstantMessagingAddress();
    std::string address;
    Protocol protocol;
    AddressType type;
    bool primary;
  };

  Contact();
  ~Contact();

  int64 serialized_update_time() const {
    return update_time.ToInternalValue();
  }
  void set_serialized_update_time(int64 serialized) {
    update_time = base::Time::FromInternalValue(serialized);
  }

  // NOTE: Any changes to the below fields must be reflected in
  // contact_test_util.cc's CopyContact() function.

  // Provider-assigned unique identifier.
  std::string provider_id;

  // Last time at which this contact was updated.
  base::Time update_time;

  // Has the contact been deleted?
  bool deleted;

  // Taken from https://developers.google.com/gdata/docs/2.0/elements#gdName.
  std::string full_name;
  std::string given_name;
  std::string additional_name;
  std::string family_name;
  std::string name_prefix;
  std::string name_suffix;

  SkBitmap photo;

  std::vector<EmailAddress> email_addresses;
  std::vector<PhoneNumber> phone_numbers;
  std::vector<PostalAddress> postal_addresses;
  std::vector<InstantMessagingAddress> instant_messaging_addresses;

  DISALLOW_COPY_AND_ASSIGN(Contact);
};

typedef std::vector<const Contact*> ContactPointers;

}  // namespace contacts

#endif  // CHROME_BROWSER_CHROMEOS_CONTACTS_CONTACT_H_
