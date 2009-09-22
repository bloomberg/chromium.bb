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
// All methods, including the dtor, are expected to be called on the IO thread
// except for the ctor and the init method which are expected to be called on
// the UI thread.
class ChromeAppCacheService
    : public base::RefCountedThreadSafe<ChromeAppCacheService>,
      public appcache::AppCacheService {
 public:

  explicit ChromeAppCacheService()
      : is_initialized_(false), was_initialized_with_io_thread_(false) {
  }

  bool is_initialized() const { return is_initialized_; }

  void InitializeOnUIThread(const FilePath& data_directory,
                            bool is_incognito) {
    DCHECK(!is_initialized_);
    is_initialized_ = true;

    // The I/O thread may be NULL during testing.
    base::Thread* io_thread = g_browser_process->io_thread();
    if (io_thread) {
      was_initialized_with_io_thread_ = true;
      io_thread->message_loop()->PostTask(FROM_HERE,
          NewRunnableMethod(this, &ChromeAppCacheService::InitializeOnIOThread,
              data_directory, is_incognito));
    }
  }

 private:
  friend class base::RefCountedThreadSafe<ChromeAppCacheService>;

  virtual ~ChromeAppCacheService() {
    DCHECK(!was_initialized_with_io_thread_ ||
           ChromeThread::CurrentlyOn(ChromeThread::IO));
  }

  void InitializeOnIOThread(const FilePath& data_directory,
                            bool is_incognito) {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
    Initialize(is_incognito ? FilePath()
                            : data_directory.Append(chrome::kAppCacheDirname));
  }

  bool is_initialized_;
  bool was_initialized_with_io_thread_;
};

#endif  // CHROME_COMMON_APPCACHE_CHROME_APPCACHE_SERVICE_H_
