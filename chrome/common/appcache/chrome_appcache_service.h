// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_APPCACHE_CHROME_APPCACHE_SERVICE_H_
#define CHROME_COMMON_APPCACHE_CHROME_APPCACHE_SERVICE_H_

#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/chrome_constants.h"
#include "webkit/appcache/appcache_service.h"

// An AppCacheService subclass used by the chrome. There is an instance
// associated with each Profile. This derivation adds refcounting semantics
// since a profile has multiple URLRequestContexts which refer to the same
// object, and those URLRequestContexts are refcounted independently of the
// owning profile.
//
// All methods, including the dtor, are expected to be called on the IO thread.
class ChromeAppCacheService
    : public base::RefCounted<ChromeAppCacheService>,
      public appcache::AppCacheService {
 public:

  ChromeAppCacheService(const FilePath& data_directory,
                        bool is_incognito) {
    Initialize(is_incognito ? FilePath()
                            : data_directory.Append(chrome::kAppCacheDirname));
  }
 private:
  friend class base::RefCounted<ChromeAppCacheService>;

  virtual ~ChromeAppCacheService() {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  }
};

#endif  // CHROME_COMMON_APPCACHE_CHROME_APPCACHE_SERVICE_H_
