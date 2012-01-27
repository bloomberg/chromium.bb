// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/about_handler.h"

#include "base/logging.h"
#include "base/process_util.h"
#include "base/threading/platform_thread.h"
#include "chrome/common/about_handler.h"
#include "content/public/common/url_constants.h"
#include "googleurl/src/gurl.h"

typedef void (*AboutHandlerFuncPtr)();

// This needs to match up with chrome_about_handler::about_urls in
// chrome/common/about_handler.cc.
static const AboutHandlerFuncPtr about_urls_handlers[] = {
    AboutHandler::AboutCrash,
    AboutHandler::AboutKill,
    AboutHandler::AboutHang,
    AboutHandler::AboutShortHang,
    NULL,
};

// static
bool AboutHandler::MaybeHandle(const GURL& url) {
  if (!url.SchemeIs(chrome::kChromeUIScheme))
    return false;

  int about_urls_handler_index = 0;
  const char* const* url_handler = chrome_about_handler::about_urls;
  while (*url_handler) {
    if (GURL(*url_handler) == url) {
      about_urls_handlers[about_urls_handler_index]();
      return true;  // theoretically :]
    }
    url_handler++;
    about_urls_handler_index++;
  }
  return false;
}

// static
void AboutHandler::AboutCrash() {
  // NOTE(shess): Crash directly rather than using NOTREACHED() so
  // that the signature is easier to triage in crash reports.
  volatile int* zero = NULL;
  *zero = 0;

  // Just in case the compiler decides the above is undefined and
  // optimizes it away.
  NOTREACHED();
}

// static
void AboutHandler::AboutKill() {
  base::KillProcess(base::GetCurrentProcessHandle(), 1, false);
}

// static
void AboutHandler::AboutHang() {
  for (;;) {
    base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(1));
  }
}

// static
void AboutHandler::AboutShortHang() {
  base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(20));
}

// static
size_t AboutHandler::AboutURLHandlerSize() {
  return arraysize(about_urls_handlers);
}
