// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_THREAD_IMPL_H_
#define IOS_WEB_WEB_THREAD_IMPL_H_

#include "content/public/browser/browser_thread.h"
#include "ios/web/public/web_thread.h"

namespace web {

// Converts web::WebThread::ID to content::BrowserThread::ID.
content::BrowserThread::ID BrowserThreadIDFromWebThreadID(
    WebThread::ID identifier);

}  // namespace web

#endif  // IOS_WEB_WEB_THREAD_IMPL_H_
