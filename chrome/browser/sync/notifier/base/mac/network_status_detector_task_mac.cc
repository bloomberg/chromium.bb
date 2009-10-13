// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/base/network_status_detector_task.h"

namespace notifier {

NetworkStatusDetectorTask* NetworkStatusDetectorTask::Create(
    talk_base::Task* parent) {
  // TODO(sync): No implementation for OS X.
  return NULL;
}

}  // namespace notifier

