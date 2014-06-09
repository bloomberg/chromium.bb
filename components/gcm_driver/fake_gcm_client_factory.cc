// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/fake_gcm_client_factory.h"

#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner.h"
#include "components/gcm_driver/gcm_client.h"

namespace gcm {

FakeGCMClientFactory::FakeGCMClientFactory(
    FakeGCMClient::StartMode gcm_client_start_mode,
    const scoped_refptr<base::SequencedTaskRunner>& ui_thread,
    const scoped_refptr<base::SequencedTaskRunner>& io_thread)
    : gcm_client_start_mode_(gcm_client_start_mode),
      ui_thread_(ui_thread),
      io_thread_(io_thread) {
}

FakeGCMClientFactory::~FakeGCMClientFactory() {
}

scoped_ptr<GCMClient> FakeGCMClientFactory::BuildInstance() {
  return scoped_ptr<GCMClient>(new FakeGCMClient(
      gcm_client_start_mode_, ui_thread_, io_thread_));
}

}  // namespace gcm
