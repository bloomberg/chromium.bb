// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_AURA_REFCOUNTED_ID_MAP_H_
#define CONTENT_BROWSER_AURA_REFCOUNTED_ID_MAP_H_

#include "base/callback.h"
#include "base/id_map.h"
#include "base/memory/ref_counted.h"

namespace content {

// A refcounted-threadsafe wrapper around IDMap.
template <typename T>
class RefCountedIDMap : public base::RefCountedThreadSafe<RefCountedIDMap<T> > {
 public:
  RefCountedIDMap();

  void AddWithID(T* element, int id) { map_.AddWithID(element, id); }
  void Remove(int id) { map_.Remove(id); }
  T* Lookup(int id) const { return map_.Lookup(id); }

 private:
  friend class base::RefCountedThreadSafe<RefCountedIDMap>;
  ~RefCountedIDMap();

  IDMap<T> map_;

  DISALLOW_COPY_AND_ASSIGN(RefCountedIDMap);
};

template<typename T> inline RefCountedIDMap<T>::RefCountedIDMap() {}

template<typename T> inline RefCountedIDMap<T>::~RefCountedIDMap() {}

}  // namespace content

#endif  // CONTENT_BROWSER_AURA_REFCOUNTED_ID_MAP_H_
