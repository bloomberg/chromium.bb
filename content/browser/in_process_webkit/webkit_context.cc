// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/in_process_webkit/webkit_context.h"

#include "base/command_line.h"
#include "content/browser/browser_thread.h"

WebKitContext::WebKitContext(
    bool is_incognito, const FilePath& data_path,
    quota::SpecialStoragePolicy* special_storage_policy,
    bool clear_local_state_on_exit,
    quota::QuotaManagerProxy* quota_manager_proxy,
    base::MessageLoopProxy* webkit_thread_loop)
    : data_path_(is_incognito ? FilePath() : data_path),
      is_incognito_(is_incognito),
      clear_local_state_on_exit_(clear_local_state_on_exit),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          dom_storage_context_(new DOMStorageContext(
              this, special_storage_policy))),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          indexed_db_context_(new IndexedDBContext(
              this, special_storage_policy, quota_manager_proxy,
              webkit_thread_loop))) {
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

void WebKitContext::DeleteSessionOnlyData() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::WEBKIT)) {
    BrowserThread::PostTask(
        BrowserThread::WEBKIT, FROM_HERE,
        NewRunnableMethod(this, &WebKitContext::DeleteSessionOnlyData));
    return;
  }

  dom_storage_context_->DeleteSessionOnlyData();
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
