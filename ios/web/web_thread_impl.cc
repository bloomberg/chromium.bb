// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/web_thread_impl.h"

#include "base/logging.h"

namespace web {

content::BrowserThread::ID BrowserThreadIDFromWebThreadID(
    WebThread::ID identifier) {
  switch (identifier) {
    case WebThread::UI:
      return content::BrowserThread::UI;
    case WebThread::DB:
      return content::BrowserThread::DB;
    case WebThread::FILE:
      return content::BrowserThread::FILE;
    case WebThread::FILE_USER_BLOCKING:
      return content::BrowserThread::FILE_USER_BLOCKING;
    case WebThread::CACHE:
      return content::BrowserThread::CACHE;
    case WebThread::IO:
      return content::BrowserThread::IO;
    default:
      NOTREACHED() << "Unknown web::WebThread::ID: " << identifier;
      return content::BrowserThread::UI;
  }
}

}  // namespace web
