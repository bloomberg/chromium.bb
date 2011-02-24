// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_IN_PROCESS_WEBKIT_DOM_STORAGE_AREA_H_
#define CONTENT_BROWSER_IN_PROCESS_WEBKIT_DOM_STORAGE_AREA_H_
#pragma once

#include "base/hash_tables.h"
#include "base/nullable_string16.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/common/dom_storage_common.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageArea.h"

class DOMStorageNamespace;
class HostContentSettingsMap;

// Only use on the WebKit thread.  DOMStorageNamespace manages our registration
// with DOMStorageContext.
class DOMStorageArea {
 public:
  DOMStorageArea(const string16& origin,
                 int64 id,
                 DOMStorageNamespace* owner,
                 HostContentSettingsMap* host_content_settings_map);
  ~DOMStorageArea();

  unsigned Length();
  NullableString16 Key(unsigned index);
  NullableString16 GetItem(const string16& key);
  NullableString16 SetItem(
      const string16& key, const string16& value,
      WebKit::WebStorageArea::Result* result);
  NullableString16 RemoveItem(const string16& key);
  bool Clear();
  void PurgeMemory();

  int64 id() const { return id_; }

  DOMStorageNamespace* owner() const { return owner_; }

 private:
  // Creates the underlying WebStorageArea on demand.
  void CreateWebStorageAreaIfNecessary();

  // Used to see if setItem has permission to do its thing.
  bool CheckContentSetting(const string16& key, const string16& value);

  // The origin this storage area represents.
  string16 origin_;
  GURL origin_url_;

  // The storage area we wrap.
  scoped_ptr<WebKit::WebStorageArea> storage_area_;

  // Our storage area id.  Unique to our parent WebKitContext.
  int64 id_;

  // The DOMStorageNamespace that owns us.
  DOMStorageNamespace* owner_;

  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;

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

#endif  // CONTENT_BROWSER_IN_PROCESS_WEBKIT_DOM_STORAGE_AREA_H_
