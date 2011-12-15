// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_ENUM_MAPPER_H_
#define CHROME_BROWSER_CHROMEOS_CROS_ENUM_MAPPER_H_
#pragma once

#include <map>
#include <string>

#include "base/basictypes.h"

namespace chromeos {

// This turns an array of string-to-enum-value mappings into a class
// that can cache the mapping and do quick lookups using an actual map
// class.  Usage is something like:
//
// const char kKey1[] = "key1";
// const char kKey2[] = "key2";
//
// enum EnumFoo {
//   UNKNOWN = 0,
//   FOO = 1,
//   BAR = 2,
// };
//
// const EnumMapper<EnumFoo>::Pair index_table[] = {
//   { kKey1, FOO },
//   { kKey2, BAR },
// };
//
// EnumMapper<EnumFoo> mapper(index_table, arraysize(index_table), UNKNOWN);
// EnumFoo value = mapper.Get(kKey1);  // Returns FOO.
// EnumFoo value = mapper.Get('boo');  // Returns UNKNOWN.
template <typename EnumType>
class EnumMapper {
 public:
  struct Pair {
    const char* key;
    const EnumType value;
  };

  EnumMapper(const Pair* list, size_t num_entries, EnumType unknown)
      : unknown_value_(unknown) {
    for (size_t i = 0; i < num_entries; ++i, ++list) {
      enum_map_[list->key] = list->value;
      inverse_enum_map_[list->value] = list->key;
    }
  }

  EnumType Get(const std::string& type) const {
    EnumMapConstIter iter = enum_map_.find(type);
    if (iter != enum_map_.end())
      return iter->second;
    return unknown_value_;
  }

  std::string GetKey(EnumType type) const {
    InverseEnumMapConstIter iter = inverse_enum_map_.find(type);
    if (iter != inverse_enum_map_.end())
      return iter->second;
    return std::string();
  }

 private:
  typedef typename std::map<std::string, EnumType> EnumMap;
  typedef typename std::map<EnumType, std::string> InverseEnumMap;
  typedef typename EnumMap::const_iterator EnumMapConstIter;
  typedef typename InverseEnumMap::const_iterator InverseEnumMapConstIter;
  EnumMap enum_map_;
  InverseEnumMap inverse_enum_map_;
  EnumType unknown_value_;
  DISALLOW_COPY_AND_ASSIGN(EnumMapper);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_ENUM_MAPPER_H_
