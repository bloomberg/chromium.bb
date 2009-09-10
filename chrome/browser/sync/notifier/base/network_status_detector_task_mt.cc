// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/base/network_status_detector_task_mt.h"

#include "chrome/browser/sync/notifier/base/async_network_alive.h"
#include "chrome/browser/sync/notifier/base/signal_thread_task.h"

#include "talk/base/common.h"

namespace notifier {

void NetworkStatusDetectorTaskMT::OnNetworkAliveDone(
    AsyncNetworkAlive* network_alive) {
  ASSERT(network_alive);
  SetNetworkAlive(network_alive->alive());
  // If we got an error from detecting the network alive state,
  // then stop retrying the detection.
  if (network_alive->error()) {
    return;
  }
  StartAsyncDetection(network_alive->ReleaseInfo());
}

void NetworkStatusDetectorTaskMT::StartAsyncDetection(
    PlatformNetworkInfo* previous_info) {
  // Use the AsyncNetworkAlive to determine the network state (and
  // changes in the network state).
  AsyncNetworkAlive* network_alive = AsyncNetworkAlive::Create();

  if (previous_info) {
    network_alive->SetWaitForNetworkChange(previous_info);
  }
  SignalThreadTask<AsyncNetworkAlive>* task =
      new SignalThreadTask<AsyncNetworkAlive>(this, &network_alive);
  task->SignalWorkDone.connect(
      this, &NetworkStatusDetectorTaskMT::OnNetworkAliveDone);
  task->Start();
}

NetworkStatusDetectorTask* NetworkStatusDetectorTask::Create(
    talk_base::Task* parent) {
  ASSERT(parent);
  return new NetworkStatusDetectorTaskMT(parent);
}

}  // namespace notifier
