// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/in_process_webkit/webkit_context.h"

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/browser/profiles/profile.h"
#include "content/browser/browser_thread.h"

WebKitContext::WebKitContext(Profile* profile, bool clear_local_state_on_exit)
    : data_path_(profile->IsOffTheRecord() ? FilePath() : profile->GetPath()),
      is_incognito_(profile->IsOffTheRecord()),
      clear_local_state_on_exit_(clear_local_state_on_exit),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          dom_storage_context_(new DOMStorageContext(
              this, profile->GetExtensionSpecialStoragePolicy()))),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          indexed_db_context_(new IndexedDBContext(this))) {
}

WebKitContext::~WebKitContext() {
  // If the WebKit thread was ever spun up, delete the object there.  The task
  // will just get deleted if the WebKit thread isn't created (which only
  // happens during testing).
  dom_storage_context_->set_clear_local_state_on_exit_(
      clear_local_state_on_exit_);
  DOMStorageContext* dom_storage_context = dom_storage_context_.release();
  if (!BrowserThread::DeleteSoon(
          BrowserThread::WEBKIT, FROM_HERE, dom_storage_context)) {
    // The WebKit thread wasn't created, and the task got deleted without
    // freeing the DOMStorageContext, so delete it manually.
    delete dom_storage_context;
  }

  indexed_db_context_->set_clear_local_state_on_exit(
      clear_local_state_on_exit_);
  IndexedDBContext* indexed_db_context = indexed_db_context_.release();
  if (!BrowserThread::DeleteSoon(
          BrowserThread::WEBKIT, FROM_HERE, indexed_db_context)) {
    delete indexed_db_context;
  }
}

void WebKitContext::PurgeMemory() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::WEBKIT)) {
    BrowserThread::PostTask(
        BrowserThread::WEBKIT, FROM_HERE,
        NewRunnableMethod(this, &WebKitContext::PurgeMemory));
    return;
  }

  dom_storage_context_->PurgeMemory();
}

void WebKitContext::DeleteDataModifiedSince(const base::Time& cutoff) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::WEBKIT)) {
    BrowserThread::PostTask(
        BrowserThread::WEBKIT, FROM_HERE,
        NewRunnableMethod(this, &WebKitContext::DeleteDataModifiedSince,
                          cutoff));
    return;
  }

  dom_storage_context_->DeleteDataModifiedSince(cutoff);
}


void WebKitContext::DeleteSessionStorageNamespace(
    int64 session_storage_namespace_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::WEBKIT)) {
    BrowserThread::PostTask(
        BrowserThread::WEBKIT, FROM_HERE,
        NewRunnableMethod(this, &WebKitContext::DeleteSessionStorageNamespace,
                          session_storage_namespace_id));
    return;
  }

  dom_storage_context_->DeleteSessionStorageNamespace(
      session_storage_namespace_id);
}
