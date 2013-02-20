// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/blocking_method_caller.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_restrictions.h"
#include "dbus/bus.h"
#include "dbus/object_proxy.h"

namespace chromeos {

namespace {

// This function is a part of CallMethodAndBlock implementation.
void CallMethodAndBlockInternal(
    scoped_ptr<dbus::Response>* response,
    base::ScopedClosureRunner* signaler,
    dbus::ObjectProxy* proxy,
    dbus::MethodCall* method_call) {
  *response = proxy->CallMethodAndBlock(
      method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
}

}  // namespace

BlockingMethodCaller::BlockingMethodCaller(dbus::Bus* bus,
                                           dbus::ObjectProxy* proxy)
    : bus_(bus),
      proxy_(proxy),
      on_blocking_method_call_(false /* manual_reset */,
                               false /* initially_signaled */) {
}

BlockingMethodCaller::~BlockingMethodCaller() {
}

scoped_ptr<dbus::Response> BlockingMethodCaller::CallMethodAndBlock(
    dbus::MethodCall* method_call) {
  // on_blocking_method_call_->Signal() will be called when |signaler| is
  // destroyed.
  base::Closure signal_task(
      base::Bind(&base::WaitableEvent::Signal,
                 base::Unretained(&on_blocking_method_call_)));
  base::ScopedClosureRunner* signaler =
      new base::ScopedClosureRunner(signal_task);

  scoped_ptr<dbus::Response> response;
  bus_->PostTaskToDBusThread(
      FROM_HERE,
      base::Bind(&CallMethodAndBlockInternal,
                 &response,
                 base::Owned(signaler),
                 base::Unretained(proxy_),
                 method_call));
  // http://crbug.com/125360
  base::ThreadRestrictions::ScopedAllowWait allow_wait;
  on_blocking_method_call_.Wait();
  return response.Pass();
}

}  // namespace chromeos
