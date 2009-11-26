// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/in_process_webkit/webkit_context.h"

#include "chrome/browser/chrome_thread.h"

WebKitContext::WebKitContext(const FilePath& data_path, bool is_incognito)
    : data_path_(data_path),
      is_incognito_(is_incognito),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          dom_storage_context_(new DOMStorageContext(this))) {
}

WebKitContext::~WebKitContext() {
  // If the WebKit thread was ever spun up, delete the object there.  The task
  // will just get deleted if the WebKit thread isn't created.
  DOMStorageContext* dom_storage_context = dom_storage_context_.release();
  if (!ChromeThread::DeleteSoon(
          ChromeThread::WEBKIT, FROM_HERE, dom_storage_context)) {
    // The WebKit thread wasn't created, and the task got deleted without
    // freeing the DOMStorageContext, so delete it manually.
    delete dom_storage_context;
  }
}

void WebKitContext::PurgeMemory() {
  // DOMStorageContext::PurgeMemory() should only be called on the WebKit
  // thread.
  //
  // Note that if there is no WebKit thread, then there's nothing in
  // LocalStorage and it's OK to no-op here.
  if (ChromeThread::CurrentlyOn(ChromeThread::WEBKIT)) {
    dom_storage_context_->PurgeMemory();
  } else {
    // Since we're not on the WebKit thread, proxy the call over to it.  We
    // can't post a task to call DOMStorageContext::PurgeMemory() directly
    // because that class is not refcounted.
    ChromeThread::PostTask(
        ChromeThread::WEBKIT, FROM_HERE,
        NewRunnableMethod(this, &WebKitContext::PurgeMemory));
  }
}

void WebKitContext::DeleteDataModifiedSince(const base::Time& cutoff) {
  // DOMStorageContext::DeleteDataModifiedSince() should only be called on the
  // WebKit thread.
  if (ChromeThread::CurrentlyOn(ChromeThread::WEBKIT)) {
    dom_storage_context_->DeleteDataModifiedSince(cutoff);
  } else {
    bool result = ChromeThread::PostTask(
        ChromeThread::WEBKIT, FROM_HERE,
        NewRunnableMethod(this, &WebKitContext::DeleteDataModifiedSince,
                          cutoff));
    DCHECK(result);
  }
}
