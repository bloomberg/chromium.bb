// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/net/network_change_notifier_io_thread.h"

#include "base/logging.h"
#include "chrome/browser/io_thread.h"

NetworkChangeNotifierIOThread::NetworkChangeNotifierIOThread(
    IOThread* io_thread) : io_thread_(io_thread) {
  DCHECK(io_thread_);
}

NetworkChangeNotifierIOThread::~NetworkChangeNotifierIOThread() {}

MessageLoop* NetworkChangeNotifierIOThread::GetMessageLoop() const {
  return io_thread_->message_loop();
}

net::NetworkChangeNotifier*
NetworkChangeNotifierIOThread::GetNetworkChangeNotifier() const {
  return io_thread_->globals()->network_change_notifier.get();
}
