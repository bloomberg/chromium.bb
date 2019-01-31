// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_network_provider.h"

#include "base/atomic_sequence_num.h"

namespace content {

// Must be unique in the child process.
int GetNextServiceWorkerProviderId() {
  static base::AtomicSequenceNumber sequence;
  return sequence.GetNext();  // We start at zero.
}

}  // namespace content
