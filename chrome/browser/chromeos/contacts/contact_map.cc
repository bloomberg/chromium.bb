// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/contacts/contact_map.h"

#include "chrome/browser/chromeos/contacts/contact.pb.h"

namespace contacts {

ContactMap::ContactMap() : contacts_deleter_(&contacts_) {}

ContactMap::~ContactMap() {}

const Contact* ContactMap::Find(const std::string& contact_id) const {
  Map::const_iterator it = contacts_.find(contact_id);
  return (it != contacts_.end()) ? it->second : NULL;
}

void ContactMap::Erase(const std::string& contact_id) {
  Map::iterator it = contacts_.find(contact_id);
  if (it == contacts_.end())
    return;

  delete it->second;
  contacts_.erase(it);
}

void ContactMap::Clear() {
  STLDeleteValues(&contacts_);
}

void ContactMap::Merge(scoped_ptr<ScopedVector<Contact> > updated_contacts,
                       DeletedContactPolicy policy) {
  for (ScopedVector<Contact>::iterator it = updated_contacts->begin();
       it != updated_contacts->end(); ++it) {
    Contact* contact = *it;
    Map::iterator map_it = contacts_.find(contact->contact_id());

    if (contact->deleted() && policy == DROP_DELETED_CONTACTS) {
      // Also delete the previous version of the contact, if any.
      if (map_it != contacts_.end()) {
        delete map_it->second;
        contacts_.erase(map_it);
      }
      delete contact;
    } else {
      if (map_it != contacts_.end()) {
        delete map_it->second;
        map_it->second = contact;
      } else {
        contacts_[contact->contact_id()] = contact;
      }
    }
  }

  // Make sure that the Contact objects that we just saved to the map won't be
  // destroyed when |updated_contacts| is destroyed.
  updated_contacts->weak_clear();
}

}  // namespace contacts
