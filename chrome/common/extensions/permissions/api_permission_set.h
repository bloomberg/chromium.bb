// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_PERMISSIONS_API_PERMISSION_SET_H_
#define CHROME_COMMON_EXTENSIONS_PERMISSIONS_API_PERMISSION_SET_H_

#include <iterator>
#include <map>

#include "chrome/common/extensions/permissions/api_permission.h"

namespace extensions {

typedef std::map<APIPermission::ID,
  scoped_refptr<APIPermissionDetail> > APIPermissionMap;

class APIPermissionSet {
 public:
  class const_iterator :
    public std::iterator<
      std::input_iterator_tag,
      scoped_refptr<APIPermissionDetail> > {
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

    const scoped_refptr<APIPermissionDetail>& operator*() const {
      return it_->second;
    }

    const scoped_refptr<APIPermissionDetail>& operator->() const {
      return it_->second;
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
  void insert(const scoped_refptr<APIPermissionDetail>& detail);

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

 private:
  APIPermissionMap map_;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_PERMISSIONS_API_PERMISSION_SET_H_
