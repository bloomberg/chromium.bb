// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/api_permission_set.h"

#include "base/logging.h"
#include "chrome/common/extensions/permissions/permissions_info.h"

namespace extensions {

APIPermissionSet::APIPermissionSet() {
}

APIPermissionSet::APIPermissionSet(const APIPermissionSet& set) {
  this->operator=(set);
}

APIPermissionSet::~APIPermissionSet() {
}

APIPermissionSet::const_iterator::const_iterator(
    const APIPermissionMap::const_iterator& it)
  : it_(it) {
}

APIPermissionSet::const_iterator::const_iterator(
    const const_iterator& ids_it)
  : it_(ids_it.it_) {
}

APIPermissionSet& APIPermissionSet::operator=(const APIPermissionSet& rhs) {
  const_iterator it = rhs.begin();
  const const_iterator end = rhs.end();
  while (it != end) {
    insert(it->Clone());
    ++it;
  }
  return *this;
}

bool APIPermissionSet::operator==(const APIPermissionSet& rhs) const {
  const_iterator it = begin();
  const_iterator rhs_it = rhs.begin();
  const_iterator it_end = end();
  const_iterator rhs_it_end = rhs.end();

  while (it != it_end && rhs_it != rhs_it_end) {
    if (!it->Equal(*rhs_it))
      return false;
    ++it;
    ++rhs_it;
  }
  return it == it_end && rhs_it == rhs_it_end;
}

void APIPermissionSet::insert(APIPermission::ID id) {
  const APIPermissionInfo* permission_info =
    PermissionsInfo::GetInstance()->GetByID(id);
  insert(permission_info->CreateAPIPermission());
}

void APIPermissionSet::insert(
    APIPermission* permission) {
  map_[permission->id()].reset(permission);
}

bool APIPermissionSet::Contains(const APIPermissionSet& rhs) const {
  APIPermissionSet::const_iterator it1 = begin();
  APIPermissionSet::const_iterator it2 = rhs.begin();
  APIPermissionSet::const_iterator end1 = end();
  APIPermissionSet::const_iterator end2 = rhs.end();

  while (it1 != end1 && it2 != end2) {
    if (it1->id() > it2->id()) {
      return false;
    } else if (it1->id() < it2->id()) {
      ++it1;
    } else {
        if (!it1->Contains(*it2))
          return false;
      ++it1;
      ++it2;
    }
  }

  return it2 == end2;
}

void APIPermissionSet::Difference(
    const APIPermissionSet& set1,
    const APIPermissionSet& set2,
    APIPermissionSet* set3) {
  CHECK(set3);
  set3->clear();

  APIPermissionSet::const_iterator it1 = set1.begin();
  APIPermissionSet::const_iterator it2 = set2.begin();
  const APIPermissionSet::const_iterator end1 = set1.end();
  const APIPermissionSet::const_iterator end2 = set2.end();

  while (it1 != end1 && it2 != end2) {
    if (it1->id() < it2->id()) {
      set3->insert(it1->Clone());
      ++it1;
    } else if (it1->id() > it2->id()) {
      ++it2;
    } else {
      APIPermission* p = it1->Diff(*it2);
      if (p)
        set3->insert(p);
      ++it1;
      ++it2;
    }
  }

  while (it1 != end1) {
    set3->insert(it1->Clone());
    ++it1;
  }
}

void APIPermissionSet::Intersection(
    const APIPermissionSet& set1,
    const APIPermissionSet& set2,
    APIPermissionSet* set3) {
  DCHECK(set3);
  set3->clear();

  APIPermissionSet::const_iterator it1 = set1.begin();
  APIPermissionSet::const_iterator it2 = set2.begin();
  const APIPermissionSet::const_iterator end1 = set1.end();
  const APIPermissionSet::const_iterator end2 = set2.end();

  while (it1 != end1 && it2 != end2) {
    if (it1->id() < it2->id()) {
      ++it1;
    } else if (it1->id() > it2->id()) {
      ++it2;
    } else {
      APIPermission* p = it1->Intersect(*it2);
      if (p)
        set3->insert(p);
      ++it1;
      ++it2;
    }
  }
}

void APIPermissionSet::Union(
    const APIPermissionSet& set1,
    const APIPermissionSet& set2,
    APIPermissionSet* set3) {
  DCHECK(set3);
  set3->clear();

  APIPermissionSet::const_iterator it1 = set1.begin();
  APIPermissionSet::const_iterator it2 = set2.begin();
  const APIPermissionSet::const_iterator end1 = set1.end();
  const APIPermissionSet::const_iterator end2 = set2.end();

  while (true) {
    if (it1 == end1) {
      while (it2 != end2) {
        set3->insert(it2->Clone());
        ++it2;
      }
      break;
    }
    if (it2 == end2) {
      while (it1 != end1) {
        set3->insert(it1->Clone());
        ++it1;
      }
      break;
    }
    if (it1->id() < it2->id()) {
      set3->insert(it1->Clone());
      ++it1;
    } else if (it1->id() > it2->id()) {
      set3->insert(it2->Clone());
      ++it2;
    } else {
      set3->insert(it1->Union(*it2));
      ++it1;
      ++it2;
    }
  }
}

}  // namespace extensions
