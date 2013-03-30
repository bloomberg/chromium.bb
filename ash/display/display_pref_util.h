// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_PREF_UTIL_H
#define ASH_DISPLAY_DISPLAY_PREF_UTIL_H

#include <map>
#include <string>

#include "base/string_piece.h"

namespace ash {

// Utility templates to create enum to string map and
// a function to find an enum value from a string.
template<typename T>
std::map<T, std::string>* CreateToStringMap(T k1, const std::string& v1,
                                            T k2, const std::string& v2,
                                            T k3, const std::string& v3,
                                            T k4, const std::string& v4) {
  std::map<T, std::string>* map = new std::map<T, std::string>();
  (*map)[k1] = v1;
  (*map)[k2] = v2;
  (*map)[k3] = v3;
  (*map)[k4] = v4;
  return map;
}

template<typename T>
bool ReverseFind(const std::map<T, std::string>* map,
                 const base::StringPiece& value,
                 T* key) {
  typename std::map<T, std::string>::const_iterator iter = map->begin();
  for (;
       iter != map->end();
       ++iter) {
    if (iter->second == value) {
      *key = iter->first;
      return true;
    }
  }
  return false;
}

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_PREF_UTIL_H
