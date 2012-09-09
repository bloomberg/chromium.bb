// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CONTACTS_CONTACT_MAP_H_
#define CHROME_BROWSER_CHROMEOS_CONTACTS_CONTACT_MAP_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/stl_util.h"
#include "base/time.h"

namespace contacts {

class Contact;

// Stores Contact objects indexed by their IDs.
class ContactMap {
 public:
  typedef std::map<std::string, Contact*> Map;
  typedef Map::const_iterator const_iterator;

  // What should Merge() do when passed a deleted contact?
  enum DeletedContactPolicy {
    // The deleted contact will be inserted into the map.
    KEEP_DELETED_CONTACTS,

    // The deleted contact will not be deleted from the map, and if there is a
    // previous version of the now-deleted contact already in the map, it will
    // also be removed.
    DROP_DELETED_CONTACTS,
  };

  ContactMap();
  ~ContactMap();

  bool empty() const { return contacts_.empty(); }
  size_t size() const { return contacts_.size(); }
  const_iterator begin() const { return contacts_.begin(); }
  const_iterator end() const { return contacts_.end(); }

  // Returns the contact with ID |contact_id|.  NULL is returned if the contact
  // isn't present.
  const Contact* Find(const std::string& contact_id) const;

  // Deletes all contacts.
  void Clear();

  // Merges |updated_contacts| into |contacts_|.
  void Merge(scoped_ptr<ScopedVector<Contact> > updated_contacts,
             DeletedContactPolicy policy);

  // Returns the maximum |update_time| value stored within a contact in
  // |contacts_|.
  base::Time GetMaxUpdateTime() const;

 private:
  Map contacts_;

  // Deletes values in |contacts_|.
  STLValueDeleter<Map> contacts_deleter_;

  DISALLOW_COPY_AND_ASSIGN(ContactMap);
};

}  // namespace contacts

#endif  // CHROME_BROWSER_CHROMEOS_CONTACTS_CONTACT_MAP_H_
