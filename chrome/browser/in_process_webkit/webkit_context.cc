// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/in_process_webkit/webkit_context.h"

#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/in_process_webkit/dom_storage_context.h"

WebKitContext::WebKitContext(const FilePath& data_path, bool is_incognito)
    : data_path_(data_path),
      is_incognito_(is_incognito) {
}

WebKitContext::~WebKitContext() {
  // If a dom storage context was ever created, we need to destroy it on the
  // WebKit thread.  Luckily we're guaranteed that the WebKit thread is still
  // alive since the ResourceDispatcherHost (which owns the WebKit thread) goes
  // away after all the ResourceMessageFilters and the profiles (i.e. all the
  // objects with a reference to us).
  if (dom_storage_context_.get()) {
    MessageLoop* loop = ChromeThread::GetMessageLoop(ChromeThread::WEBKIT);
    loop->DeleteSoon(FROM_HERE, dom_storage_context_.release());
  }
}

DOMStorageContext* WebKitContext::GetDOMStorageContext() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  if (!dom_storage_context_.get())
    dom_storage_context_.reset(new DOMStorageContext(this));
  return dom_storage_context_.get();
}
