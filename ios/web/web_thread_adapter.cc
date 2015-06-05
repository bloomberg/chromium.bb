// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/web_thread_adapter.h"

#include "content/public/browser/browser_thread.h"

namespace web {

namespace {

WebThread::ID WebThreadIDFromBrowserThreadID(
    content::BrowserThread::ID identifier) {
  switch (identifier) {
    case content::BrowserThread::UI:
      return WebThread::UI;
    case content::BrowserThread::DB:
      return WebThread::DB;
    case content::BrowserThread::FILE:
      return WebThread::FILE;
    case content::BrowserThread::FILE_USER_BLOCKING:
      return WebThread::FILE_USER_BLOCKING;
    case content::BrowserThread::CACHE:
      return WebThread::CACHE;
    case content::BrowserThread::IO:
      return WebThread::IO;
    default:
      NOTREACHED() << "Unknown content::BrowserThread::ID: " << identifier;
      return WebThread::UI;
  }
}

}  // namespace

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

#pragma mark - web_thread.h implementation

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
bool WebThread::PostNonNestableTask(ID identifier,
                                    const tracked_objects::Location& from_here,
                                    const base::Closure& task) {
  return content::BrowserThread::PostNonNestableTask(
      BrowserThreadIDFromWebThreadID(identifier), from_here, task);
}

// static
bool WebThread::PostNonNestableDelayedTask(
    ID identifier,
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  return content::BrowserThread::PostNonNestableDelayedTask(
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
bool WebThread::PostBlockingPoolTaskAndReply(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    const base::Closure& reply) {
  return content::BrowserThread::PostBlockingPoolTaskAndReply(from_here, task,
                                                              reply);
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
bool WebThread::IsThreadInitialized(ID identifier) {
  return content::BrowserThread::IsThreadInitialized(
      BrowserThreadIDFromWebThreadID(identifier));
}

// static
bool WebThread::CurrentlyOn(ID identifier) {
  return content::BrowserThread::CurrentlyOn(
      BrowserThreadIDFromWebThreadID(identifier));
}

// static
bool WebThread::IsMessageLoopValid(ID identifier) {
  return content::BrowserThread::IsMessageLoopValid(
      BrowserThreadIDFromWebThreadID(identifier));
}

// static
bool WebThread::GetCurrentThreadIdentifier(ID* identifier) {
  content::BrowserThread::ID content_identifier;
  bool result =
      content::BrowserThread::GetCurrentThreadIdentifier(&content_identifier);
  if (result)
    *identifier = WebThreadIDFromBrowserThreadID(content_identifier);
  return result;
}

// static
scoped_refptr<base::SingleThreadTaskRunner> WebThread::GetTaskRunnerForThread(
    ID identifier) {
  return content::BrowserThread::GetTaskRunnerForThread(
      BrowserThreadIDFromWebThreadID(identifier));
}

// static
std::string WebThread::GetDCheckCurrentlyOnErrorMessage(ID expected) {
  return content::BrowserThread::GetDCheckCurrentlyOnErrorMessage(
      BrowserThreadIDFromWebThreadID(expected));
}

}  // namespace web
