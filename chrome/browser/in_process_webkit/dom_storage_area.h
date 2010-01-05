// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CHROME_BROWSER_IN_PROCESS_WEBKIT_DOM_STORAGE_AREA_H_
#define CHROME_BROWSER_IN_PROCESS_WEBKIT_DOM_STORAGE_AREA_H_

#include "base/hash_tables.h"
#include "base/nullable_string16.h"
#include "base/scoped_ptr.h"

class DOMStorageNamespace;

namespace WebKit {
class WebStorageArea;
}

// Only use on the WebKit thread.  DOMStorageNamespace manages our registration
// with DOMStorageContext.
class DOMStorageArea {
 public:
  DOMStorageArea(const string16& origin, int64 id, DOMStorageNamespace* owner);
  ~DOMStorageArea();

  unsigned Length();
  NullableString16 Key(unsigned index);
  NullableString16 GetItem(const string16& key);
  void SetItem(const string16& key, const string16& value,
               bool* quota_xception);
  void RemoveItem(const string16& key);
  void Clear();
  void PurgeMemory();

  int64 id() const { return id_; }

 private:
  // Creates the underlying WebStorageArea on demand.
  void CreateWebStorageAreaIfNecessary();

  // The origin this storage area represents.
  string16 origin_;

  // The storage area we wrap.
  scoped_ptr<WebKit::WebStorageArea> storage_area_;

  // Our storage area id.  Unique to our parent WebKitContext.
  int64 id_;

  // The DOMStorageNamespace that owns us.
  DOMStorageNamespace* owner_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DOMStorageArea);
};

#if defined(COMPILER_GCC)
namespace __gnu_cxx {

template<>
struct hash<DOMStorageArea*> {
  std::size_t operator()(DOMStorageArea* const& p) const {
    return reinterpret_cast<std::size_t>(p);
  }
};

}  // namespace __gnu_cxx
#endif

#endif  // CHROME_BROWSER_IN_PROCESS_WEBKIT_DOM_STORAGE_AREA_H_
