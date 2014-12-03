// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/web_thread.h"

#include "base/logging.h"
#include "content/public/browser/browser_thread.h"

namespace web {
namespace {

content::BrowserThread::ID BrowserThreadIDFromWebThreadID(
    WebThread::ID identifier) {
  switch (identifier) {
    case WebThread::UI:
      return content::BrowserThread::UI;
    default:
      NOTREACHED() << "Unknown web::WebThread::ID: " << identifier;
  }
  return content::BrowserThread::UI;
}

}  // namespace

// static
bool WebThread::PostTask(ID identifier,
                         const tracked_objects::Location& from_here,
                         const base::Closure& task) {
  return content::BrowserThread::PostTask(
      BrowserThreadIDFromWebThreadID(identifier), from_here, task);
}

// static
base::SequencedWorkerPool* WebThread::GetBlockingPool() {
  return content::BrowserThread::GetBlockingPool();
}

// static
bool WebThread::CurrentlyOn(ID identifier) {
  return content::BrowserThread::CurrentlyOn(
      BrowserThreadIDFromWebThreadID(identifier));
}

// static
std::string WebThread::GetDCheckCurrentlyOnErrorMessage(ID expected) {
  return content::BrowserThread::GetDCheckCurrentlyOnErrorMessage(
      BrowserThreadIDFromWebThreadID(expected));
}

}  // namespace web
