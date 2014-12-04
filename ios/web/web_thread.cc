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
bool WebThread::PostDelayedTask(ID identifier,
                                const tracked_objects::Location& from_here,
                                const base::Closure& task,
                                base::TimeDelta delay) {
  return content::BrowserThread::PostDelayedTask(
      BrowserThreadIDFromWebThreadID(identifier), from_here, task, delay);
}

// static
bool WebThread::PostTaskAndReply(ID identifier,
                                 const tracked_objects::Location& from_here,
                                 const base::Closure& task,
                                 const base::Closure& reply) {
  return content::BrowserThread::PostTaskAndReply(
      BrowserThreadIDFromWebThreadID(identifier), from_here, task, reply);
}

// static
bool WebThread::PostBlockingPoolTask(const tracked_objects::Location& from_here,
                                     const base::Closure& task) {
  return content::BrowserThread::PostBlockingPoolTask(from_here, task);
}

// static
bool WebThread::PostBlockingPoolSequencedTask(
    const std::string& sequence_token_name,
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  return content::BrowserThread::PostBlockingPoolSequencedTask(
      sequence_token_name, from_here, task);
}

// static
base::SequencedWorkerPool* WebThread::GetBlockingPool() {
  return content::BrowserThread::GetBlockingPool();
}

// static
base::MessageLoop* WebThread::UnsafeGetMessageLoopForThread(ID identifier) {
  return content::BrowserThread::UnsafeGetMessageLoopForThread(
      BrowserThreadIDFromWebThreadID(identifier));
}

// static
bool WebThread::CurrentlyOn(ID identifier) {
  return content::BrowserThread::CurrentlyOn(
      BrowserThreadIDFromWebThreadID(identifier));
}

// static
scoped_refptr<base::MessageLoopProxy> WebThread::GetMessageLoopProxyForThread(
    ID identifier) {
  return content::BrowserThread::GetMessageLoopProxyForThread(
      BrowserThreadIDFromWebThreadID(identifier));
}

// static
std::string WebThread::GetDCheckCurrentlyOnErrorMessage(ID expected) {
  return content::BrowserThread::GetDCheckCurrentlyOnErrorMessage(
      BrowserThreadIDFromWebThreadID(expected));
}

}  // namespace web
