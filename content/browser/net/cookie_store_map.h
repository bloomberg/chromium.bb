// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NET_COOKIE_STORE_MAP_H_
#define CONTENT_BROWSER_NET_COOKIE_STORE_MAP_H_

#include <map>
#include <string>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "content/common/content_export.h"

class GURL;

namespace net {
class CookieStore;
}  // namespace net

namespace content {

// CookieStoreMap allows associating different CookieStore objects with
// different schemes. It is mainly a convenience class.
class CookieStoreMap {
 public:
  CookieStoreMap();
  virtual ~CookieStoreMap();

  // Returns the CookieStore associated with |scheme|.
  CONTENT_EXPORT net::CookieStore* GetForScheme(
      const std::string& scheme) const;

  // Associates a |cookie_store| with the given |scheme|. Should only be called
  // once for any given |scheme|. |cookie_store| must be non-NULL. The
  // CookieStoreMap will retain the |cookie_store| object.
  void SetForScheme(const std::string& scheme, net::CookieStore* cookie_store);

  // Clears cookies matching the specified parameters from all CookieStores
  // contained in this map.  Calls |done| when all CookieStores have been
  // cleared. |done| is guaranteed to be run on the calling thread.
  void DeleteCookies(const GURL& origin,
                     const base::Time begin,
                     const base::Time end,
                     const base::Closure& done);

  // Makes a clone of the map. Useful if the map needs to be copied to another
  // thread.
  CookieStoreMap* Clone() const;

 private:
  typedef std::map<std::string, scoped_refptr<net::CookieStore> > MapType;
  MapType scheme_map_;

  DISALLOW_COPY_AND_ASSIGN(CookieStoreMap);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NET_COOKIE_STORE_MAP_H_
