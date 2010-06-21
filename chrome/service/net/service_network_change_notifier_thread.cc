// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/service/net/service_network_change_notifier_thread.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "net/base/network_change_notifier.h"

ServiceNetworkChangeNotifierThread::ServiceNetworkChangeNotifierThread(
    MessageLoop* io_thread_message_loop)
        : io_thread_message_loop_(io_thread_message_loop) {
  DCHECK(io_thread_message_loop_);
}

ServiceNetworkChangeNotifierThread::~ServiceNetworkChangeNotifierThread() {
  io_thread_message_loop_->DeleteSoon(FROM_HERE,
                                      network_change_notifier_.release());
}

void ServiceNetworkChangeNotifierThread::Initialize() {
  io_thread_message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this,
          &ServiceNetworkChangeNotifierThread::CreateNetworkChangeNotifier));
}

MessageLoop* ServiceNetworkChangeNotifierThread::GetMessageLoop() const {
  DCHECK(io_thread_message_loop_);
  return io_thread_message_loop_;
}

net::NetworkChangeNotifier*
ServiceNetworkChangeNotifierThread::GetNetworkChangeNotifier() const {
  DCHECK(MessageLoop::current() == io_thread_message_loop_);
  return network_change_notifier_.get();
}

void ServiceNetworkChangeNotifierThread::CreateNetworkChangeNotifier() {
  DCHECK(MessageLoop::current() == io_thread_message_loop_);
  network_change_notifier_.reset(
      net::NetworkChangeNotifier::CreateDefaultNetworkChangeNotifier());
}

