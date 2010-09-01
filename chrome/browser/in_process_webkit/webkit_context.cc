// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/in_process_webkit/webkit_context.h"

#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/profile.h"

WebKitContext::WebKitContext(Profile* profile)
    : data_path_(profile->IsOffTheRecord() ? FilePath() : profile->GetPath()),
      is_incognito_(profile->IsOffTheRecord()),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          dom_storage_context_(new DOMStorageContext(this))),
      indexed_db_context_(new IndexedDBContext()) {
}

WebKitContext::~WebKitContext() {
  // If the WebKit thread was ever spun up, delete the object there.  The task
  // will just get deleted if the WebKit thread isn't created (which only
  // happens during testing).
  DOMStorageContext* dom_storage_context = dom_storage_context_.release();
  if (!ChromeThread::DeleteSoon(
          ChromeThread::WEBKIT, FROM_HERE, dom_storage_context)) {
    // The WebKit thread wasn't created, and the task got deleted without
    // freeing the DOMStorageContext, so delete it manually.
    delete dom_storage_context;
  }

  IndexedDBContext* indexed_db_context = indexed_db_context_.release();
  if (!ChromeThread::DeleteSoon(
          ChromeThread::WEBKIT, FROM_HERE, indexed_db_context)) {
    delete indexed_db_context;
  }
}

void WebKitContext::PurgeMemory() {
  if (!ChromeThread::CurrentlyOn(ChromeThread::WEBKIT)) {
    bool result = ChromeThread::PostTask(
        ChromeThread::WEBKIT, FROM_HERE,
        NewRunnableMethod(this, &WebKitContext::PurgeMemory));
    DCHECK(result);
    return;
  }

  dom_storage_context_->PurgeMemory();
}

void WebKitContext::DeleteDataModifiedSince(
    const base::Time& cutoff,
    const char* url_scheme_to_be_skipped,
    const std::vector<string16>& protected_origins) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::WEBKIT)) {
    bool result = ChromeThread::PostTask(
        ChromeThread::WEBKIT, FROM_HERE,
        NewRunnableMethod(this, &WebKitContext::DeleteDataModifiedSince,
                          cutoff, url_scheme_to_be_skipped, protected_origins));
    DCHECK(result);
    return;
  }

  dom_storage_context_->DeleteDataModifiedSince(
      cutoff, url_scheme_to_be_skipped, protected_origins);
}


void WebKitContext::DeleteSessionStorageNamespace(
    int64 session_storage_namespace_id) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::WEBKIT)) {
    ChromeThread::PostTask(
        ChromeThread::WEBKIT, FROM_HERE,
        NewRunnableMethod(this, &WebKitContext::DeleteSessionStorageNamespace,
                          session_storage_namespace_id));
    return;
  }

  dom_storage_context_->DeleteSessionStorageNamespace(
      session_storage_namespace_id);
}
