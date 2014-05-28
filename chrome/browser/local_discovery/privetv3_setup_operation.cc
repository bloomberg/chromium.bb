// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privetv3_setup_operation.h"

#include "base/logging.h"

namespace local_discovery {

scoped_ptr<PrivetV3SetupOperation> PrivetV3SetupOperation::Create(
    PrivetV3Session* session,
    const SetupStatusCallback& callback,
    const std::string& ticket_id) {
  NOTIMPLEMENTED();
  return scoped_ptr<PrivetV3SetupOperation>();
}

}  // namespace local_discovery
