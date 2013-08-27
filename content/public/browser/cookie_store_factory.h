// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_COOKIE_STORE_FACTORY_H_
#define CONTENT_PUBLIC_BROWSER_COOKIE_STORE_FACTORY_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "webkit/browser/quota/special_storage_policy.h"

namespace base {
class FilePath;
}

namespace net {
class CookieMonsterDelegate;
class CookieStore;
}

namespace content {

struct CONTENT_EXPORT CookieStoreConfig {
  // Specifies how session cookies are persisted in the backing data store.
  enum SessionCookieMode {
    EPHEMERAL_SESSION_COOKIES,
    PERSISTANT_SESSION_COOKIES,
    RESTORED_SESSION_COOKIES
  };

  // If |path| is empty, then this specifies an in-memory cookie store.
  // With in-memory cookie stores, |session_cookie_mode| must be
  // EPHEMERAL_SESSION_COOKIES.
  CookieStoreConfig();
  CookieStoreConfig(const base::FilePath& path,
                    SessionCookieMode session_cookie_mode,
                    quota::SpecialStoragePolicy* storage_policy,
                    net::CookieMonsterDelegate* cookie_delegate);
  ~CookieStoreConfig();

  base::FilePath path;
  SessionCookieMode session_cookie_mode;
  scoped_refptr<quota::SpecialStoragePolicy> storage_policy;
  scoped_refptr<net::CookieMonsterDelegate> cookie_delegate;
};

CONTENT_EXPORT net::CookieStore* CreateCookieStore(
    const CookieStoreConfig& config);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_COOKIE_STORE_FACTORY_H_
