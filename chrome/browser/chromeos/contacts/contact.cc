// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/contacts/contact.h"

namespace contacts {

Contact::AddressType::AddressType() : relation(RELATION_OTHER) {}

Contact::EmailAddress::EmailAddress() : primary(false) {}

Contact::PhoneNumber::PhoneNumber() : primary(false) {}

Contact::PostalAddress::PostalAddress() : primary(false) {}

Contact::InstantMessagingAddress::InstantMessagingAddress()
    : protocol(PROTOCOL_OTHER),
      primary(false) {}

Contact::Contact() : deleted(false) {}

Contact::~Contact() {}

}  // namespace contacts
