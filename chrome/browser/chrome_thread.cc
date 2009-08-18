// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_thread.h"

// Friendly names for the well-known threads.
static const char* chrome_thread_names[ChromeThread::ID_COUNT] = {
  "",  // UI (name assembled in browser_main.cc).
  "Chrome_IOThread",  // IO
  "Chrome_FileThread",  // FILE
  "Chrome_DBThread",  // DB
  "Chrome_WebKitThread",  // WEBKIT
  "Chrome_HistoryThread",  // HISTORY
#if defined(OS_LINUX)
  "Chrome_Background_X11Thread",  // BACKGROUND_X11
#endif
};

Lock ChromeThread::lock_;

ChromeThread* ChromeThread::chrome_threads_[ID_COUNT] = {
  NULL,  // UI
  NULL,  // IO
  NULL,  // FILE
  NULL,  // DB
  NULL,  // WEBKIT
  NULL,  // HISTORY
#if defined(OS_LINUX)
  NULL,  // BACKGROUND_X11
#endif
};

ChromeThread::ChromeThread(ChromeThread::ID identifier)
    : Thread(chrome_thread_names[identifier]),
      identifier_(identifier) {
  Initialize();
}

ChromeThread::ChromeThread()
    : Thread(MessageLoop::current()->thread_name().c_str()),
      identifier_(UI) {
  set_message_loop(MessageLoop::current());
  Initialize();
}

void ChromeThread::Initialize() {
  AutoLock lock(lock_);
  DCHECK(identifier_ >= 0 && identifier_ < ID_COUNT);
  DCHECK(chrome_threads_[identifier_] == NULL);
  chrome_threads_[identifier_] = this;
}

ChromeThread::~ChromeThread() {
  AutoLock lock(lock_);
  chrome_threads_[identifier_] = NULL;
}

// static
MessageLoop* ChromeThread::GetMessageLoop(ID identifier) {
  AutoLock lock(lock_);
  DCHECK(identifier >= 0 && identifier < ID_COUNT);

  if (chrome_threads_[identifier])
    return chrome_threads_[identifier]->message_loop();

  return NULL;
}

// static
bool ChromeThread::CurrentlyOn(ID identifier) {
  // MessageLoop::current() will return NULL if none is running.  This is often
  // true when running under unit tests.  This behavior actually works out
  // pretty convienently (as is mentioned in the header file comment), but it's
  // worth noting here.
  MessageLoop* message_loop = GetMessageLoop(identifier);
  return MessageLoop::current() == message_loop;
}

