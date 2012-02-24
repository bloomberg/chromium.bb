// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/in_process_webkit/webkit_context.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "content/browser/in_process_webkit/dom_storage_context_impl.h"
#include "content/browser/in_process_webkit/indexed_db_context_impl.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

WebKitContext::WebKitContext(
    bool is_incognito, const FilePath& data_path,
    quota::SpecialStoragePolicy* special_storage_policy,
    quota::QuotaManagerProxy* quota_manager_proxy,
    base::MessageLoopProxy* webkit_thread_loop)
    : data_path_(is_incognito ? FilePath() : data_path),
      is_incognito_(is_incognito),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          dom_storage_context_(new DOMStorageContextImpl(
              this, special_storage_policy))),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          indexed_db_context_(new IndexedDBContextImpl(
              this, special_storage_policy, quota_manager_proxy,
              webkit_thread_loop))) {
}

WebKitContext::~WebKitContext() {
  // If the WebKit thread was ever spun up, delete the object there.  The task
  // will just get deleted if the WebKit thread isn't created (which only
  // happens during testing).
  DOMStorageContextImpl* dom_storage_context = dom_storage_context_.release();
  if (!BrowserThread::ReleaseSoon(
          BrowserThread::WEBKIT_DEPRECATED, FROM_HERE, dom_storage_context)) {
    // The WebKit thread wasn't created, and the task got deleted without
    // freeing the DOMStorageContext, so delete it manually.
    dom_storage_context->Release();
  }
}
