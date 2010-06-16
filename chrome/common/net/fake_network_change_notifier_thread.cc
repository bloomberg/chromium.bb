// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/fake_network_change_notifier_thread.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/common/net/thread_blocker.h"
#include "net/base/mock_network_change_notifier.h"

// We manage the lifetime of
// chrome_common_net::FakeNetworkChangeNotifierThread ourselves.
DISABLE_RUNNABLE_METHOD_REFCOUNT(
    chrome_common_net::FakeNetworkChangeNotifierThread);

namespace chrome_common_net {

FakeNetworkChangeNotifierThread::FakeNetworkChangeNotifierThread()
    : thread_("FakeNetworkChangeNotifierThread") {}

FakeNetworkChangeNotifierThread::~FakeNetworkChangeNotifierThread() {
  CHECK(!network_change_notifier_.get());
  CHECK(!thread_blocker_.get());
  CHECK(!thread_.IsRunning());
}

void FakeNetworkChangeNotifierThread::Start() {
  network_change_notifier_.reset(new net::MockNetworkChangeNotifier());
  CHECK(thread_.Start());
  thread_blocker_.reset(new ThreadBlocker(&thread_));
  thread_blocker_->Block();
}

void FakeNetworkChangeNotifierThread::Pump() {
  thread_blocker_->Unblock();
  thread_blocker_->Block();
}

void FakeNetworkChangeNotifierThread::Stop() {
  thread_blocker_->Unblock();
  thread_blocker_.reset();
  thread_.Stop();
  network_change_notifier_.reset();
}

void FakeNetworkChangeNotifierThread::NotifyIPAddressChange() {
  CHECK(thread_.IsRunning());
  GetMessageLoop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this,
          &FakeNetworkChangeNotifierThread::
          NotifyIPAddressChangeOnSourceThread));
}

void FakeNetworkChangeNotifierThread::NotifyIPAddressChangeOnSourceThread() {
  CHECK_EQ(MessageLoop::current(), GetMessageLoop());
  CHECK(network_change_notifier_.get());
  network_change_notifier_->NotifyIPAddressChange();
}

MessageLoop* FakeNetworkChangeNotifierThread::GetMessageLoop() const {
  CHECK(thread_.IsRunning());
  MessageLoop* message_loop = thread_.message_loop();
  CHECK(message_loop);
  return message_loop;
}

net::NetworkChangeNotifier*
FakeNetworkChangeNotifierThread::GetNetworkChangeNotifier() const {
  CHECK_EQ(MessageLoop::current(), GetMessageLoop());
  CHECK(network_change_notifier_.get());
  return network_change_notifier_.get();
}

}  // namespace chrome_common_net
