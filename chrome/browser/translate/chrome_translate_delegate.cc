// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/chrome_translate_delegate.h"

#include "base/memory/singleton.h"
#include "chrome/browser/browser_process.h"

ChromeTranslateDelegate* ChromeTranslateDelegate::GetInstance() {
  return Singleton<ChromeTranslateDelegate>::get();
}

ChromeTranslateDelegate::ChromeTranslateDelegate() {}

ChromeTranslateDelegate::~ChromeTranslateDelegate() {}

// TranslateDelegate methods.

net::URLRequestContextGetter* ChromeTranslateDelegate::GetURLRequestContext() {
  return g_browser_process->system_request_context();
}
