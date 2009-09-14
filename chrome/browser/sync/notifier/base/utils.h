// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utility functions.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_BASE_UTILS_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_BASE_UTILS_H_

#include <map>
#include <string>

#include "chrome/browser/sync/notifier/base/static_assert.h"

// Return error if the first argument evaluates to false.
#define RET_IF_FALSE(x) do { if (!(x)) return false; } while (false)

// Protocol constants.
const char kHttpProto[] = "http://";
const char kHttpsProto[] = "https://";

// Initialize a POD to zero. Using this function requires discipline. Don't
// use for types that have a v-table or virtual bases.
template <typename T>
inline void SetZero(T& p) {
  // Guard against the easy mistake of
  //    foo(int *p) { SetZero(p); } instead of
  //                  SetZero(*p);
  // which it should be.
  STATIC_ASSERT(sizeof(T) != sizeof(void*));

  // A POD (plain old data) object has one of these data types:
  // a fundamental type, union, struct, array,
  // or class--with no constructor. PODs don't have virtual functions or
  // virtual bases.

  // Test to see if the type has constructors.
  union CtorTest {
    T t;
    int i;
  };

  // TODO(sync) There might be a way to test if the type has virtuals.
  // For now, if we zero a type with virtuals by mistake, it is going to crash
  // predictable at run-time when the virtuals are called.
  memset(&p, 0, sizeof(T));
}

// Used to delete each element in a vector<T*>/deque<T*> (and then empty the
// sequence).
template <class T>
void CleanupSequence(T* items) {
  for (typename T::iterator it(items->begin()); it != items->end(); ++it) {
    delete (*it);
  }
  items->clear();
}

// Typically used to clean up values used in a hash_map that had Type* as
// values.
//
// WARNING: This function assumes that T::clear will not access the values
// (or the keys if they are the same as the values).  This is true for
// hash_map.
template <class T>
void CleanupMap(T* items) {
  // This is likely slower than a for loop, but we have to do it this way. In
  // some of the maps we use, deleting it->second causes it->first to be
  // deleted as well, and that seems to send the iterator in a tizzy.
  typename T::iterator it = items->begin();
  while (it != items->end()) {
    items->erase(it->first);
    delete it->second;
    it = items->begin();
  }
}

// Get the value of an element in the map with the specified name.
template <class T>
void GetMapElement(const std::map<const std::string, const T>& m,
                   const char* name,
                   T* value) {
  typename std::map<const std::string, const T>::const_iterator iter(
      m.find(name));
  if (iter != m.end()) {
    *value = iter->second;
  }
}

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_BASE_UTILS_H_
