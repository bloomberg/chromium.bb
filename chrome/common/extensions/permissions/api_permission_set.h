// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_PERMISSIONS_API_PERMISSION_SET_H_
#define CHROME_COMMON_EXTENSIONS_PERMISSIONS_API_PERMISSION_SET_H_

#include <iterator>
#include <map>

#include "base/memory/linked_ptr.h"
#include "chrome/common/extensions/permissions/api_permission.h"

namespace base {
class ListValue;
}  // namespace base

namespace extensions {

class Extension;

typedef std::map<APIPermission::ID,
  linked_ptr<APIPermission> > APIPermissionMap;

class APIPermissionSet {
 public:
  class const_iterator :
    public std::iterator<std::input_iterator_tag, const APIPermission*> {
   public:
    const_iterator(const APIPermissionMap::const_iterator& it);
    const_iterator(const const_iterator& ids_it);

    const_iterator& operator++() {
      ++it_;
      return *this;
    }

    const_iterator operator++(int) {
      const_iterator tmp(it_++);
      return tmp;
    }

    bool operator==(const const_iterator& rhs) const {
      return it_ == rhs.it_;
    }

    bool operator!=(const const_iterator& rhs) const {
      return it_ != rhs.it_;
    }

    const APIPermission* operator*() const {
      return it_->second.get();
    }

    const APIPermission* operator->() const {
      return it_->second.get();
    }

   private:
    APIPermissionMap::const_iterator it_;
  };

  APIPermissionSet();

  APIPermissionSet(const APIPermissionSet& set);

  ~APIPermissionSet();

  const_iterator begin() const {
    return const_iterator(map().begin());
  }

  const_iterator end() const {
    return map().end();
  }

  const_iterator find(APIPermission::ID id) const {
    return map().find(id);
  }

  const APIPermissionMap& map() const {
    return map_;
  }

  APIPermissionMap& map() {
    return map_;
  }

  void clear() {
    map_.clear();
  }

  size_t count(APIPermission::ID id) const {
    return map().count(id);
  }

  bool empty() const {
    return map().empty();
  }

  size_t erase(APIPermission::ID id) {
    return map().erase(id);
  }

  size_t size() const {
    return map().size();
  }

  APIPermissionSet& operator=(const APIPermissionSet& rhs);

  bool operator==(const APIPermissionSet& rhs) const;

  bool operator!=(const APIPermissionSet& rhs) const {
    return !operator==(rhs);
  }

  void insert(APIPermission::ID id);

  // Insert |permission| into the APIPermissionSet. The APIPermissionSet will
  // take the ownership of |permission|,
  void insert(APIPermission* permission);

  bool Contains(const APIPermissionSet& rhs) const;

  static void Difference(
      const APIPermissionSet& set1,
      const APIPermissionSet& set2,
      APIPermissionSet* set3);

  static void Intersection(
      const APIPermissionSet& set1,
      const APIPermissionSet& set2,
      APIPermissionSet* set3);

  static void Union(
      const APIPermissionSet& set1,
      const APIPermissionSet& set2,
      APIPermissionSet* set3);

  // Parses permissions from |permissions_data| and adds the parsed permissions
  // to |api_permissions|. If |unhandled_permissions| is not NULL the names of
  // all permissions that couldn't be parsed will be added to this vector.
  // If |error| is NULL, parsing will continue with the next permission if
  // invalid data is detected. If |error| is not NULL, it will be set to an
  // error message and false is returned when an invalid permission is found.
  static bool ParseFromJSON(
      const base::ListValue* permissions_data,
      APIPermissionSet* api_permissions,
      string16* error,
      std::vector<std::string>* unhandled_permissions);

 private:
  APIPermissionMap map_;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_PERMISSIONS_API_PERMISSION_SET_H_
