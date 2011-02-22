// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CHROME_BLOB_STORAGE_CONTEXT_H_
#define CONTENT_BROWSER_CHROME_BLOB_STORAGE_CONTEXT_H_
#pragma once

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/browser_thread.h"

class GURL;

namespace webkit_blob {
class BlobStorageController;
}

// A context class that keeps track of BlobStorageController used by the chrome.
// There is an instance associated with each Profile. There could be multiple
// URLRequestContexts in the same profile that refers to the same instance.
//
// All methods, except the ctor, are expected to be called on
// the IO thread (unless specifically called out in doc comments).
class ChromeBlobStorageContext
    : public base::RefCountedThreadSafe<ChromeBlobStorageContext,
                                        BrowserThread::DeleteOnIOThread> {
 public:
  ChromeBlobStorageContext();

  void InitializeOnIOThread();

  webkit_blob::BlobStorageController* controller() const {
    return controller_.get();
  }

 private:
  friend class BrowserThread;
  friend class DeleteTask<ChromeBlobStorageContext>;

  virtual ~ChromeBlobStorageContext();

  scoped_ptr<webkit_blob::BlobStorageController> controller_;
};

#endif  // CONTENT_BROWSER_CHROME_BLOB_STORAGE_CONTEXT_H_
