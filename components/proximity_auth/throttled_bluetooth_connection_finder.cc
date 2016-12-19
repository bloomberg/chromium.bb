// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/throttled_bluetooth_connection_finder.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/task_runner.h"
#include "base/time/time.h"
#include "components/cryptauth/bluetooth_throttler.h"
#include "components/cryptauth/connection.h"
#include "components/proximity_auth/bluetooth_connection_finder.h"

namespace proximity_auth {

ThrottledBluetoothConnectionFinder::ThrottledBluetoothConnectionFinder(
    std::unique_ptr<BluetoothConnectionFinder> connection_finder,
    scoped_refptr<base::TaskRunner> task_runner,
    cryptauth::BluetoothThrottler* throttler)
    : connection_finder_(std::move(connection_finder)),
      task_runner_(task_runner),
      throttler_(throttler),
      weak_ptr_factory_(this) {}

ThrottledBluetoothConnectionFinder::~ThrottledBluetoothConnectionFinder() {
}

void ThrottledBluetoothConnectionFinder::Find(
    const cryptauth::ConnectionFinder::ConnectionCallback&
        connection_callback) {
  const base::TimeDelta delay = throttler_->GetDelay();

  // Wait, if needed.
  if (!delay.is_zero()) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::Bind(&ThrottledBluetoothConnectionFinder::Find,
                   weak_ptr_factory_.GetWeakPtr(), connection_callback),
        delay);
    return;
  }

  connection_finder_->Find(
      base::Bind(&ThrottledBluetoothConnectionFinder::OnConnection,
                 weak_ptr_factory_.GetWeakPtr(), connection_callback));
}

void ThrottledBluetoothConnectionFinder::OnConnection(
    const cryptauth::ConnectionFinder::ConnectionCallback& connection_callback,
    std::unique_ptr<cryptauth::Connection> connection) {
  throttler_->OnConnection(connection.get());
  connection_callback.Run(std::move(connection));
}

}  // namespace proximity_auth
