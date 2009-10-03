// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/in_process_webkit/webkit_context.h"

#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/in_process_webkit/dom_storage_context.h"

WebKitContext::WebKitContext(const FilePath& data_path, bool is_incognito)
    : data_path_(data_path),
      is_incognito_(is_incognito),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          dom_storage_context_(new DOMStorageContext(this))) {
}

WebKitContext::~WebKitContext() {
  // If the WebKit thread was ever spun up, delete the object there.  If we're
  // on the IO thread, this is safe because the WebKit thread goes away after
  // the IO.  If we're on the UI thread, we're safe because the UI thread kills
  // the WebKit thread.
  MessageLoop* webkit_loop = ChromeThread::GetMessageLoop(ChromeThread::WEBKIT);
  if (webkit_loop)
    webkit_loop->DeleteSoon(FROM_HERE, dom_storage_context_.release());
}
