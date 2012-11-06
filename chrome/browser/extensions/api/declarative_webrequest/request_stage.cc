// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_webrequest/request_stage.h"

#include "base/basictypes.h"

namespace extensions {

const RequestStage kRequestStages[] = {
  ON_BEFORE_REQUEST,
  ON_BEFORE_SEND_HEADERS,
  ON_SEND_HEADERS,
  ON_HEADERS_RECEIVED,
  ON_AUTH_REQUIRED,
  ON_BEFORE_REDIRECT,
  ON_RESPONSE_STARTED,
  ON_COMPLETED,
  ON_ERROR
};

const size_t kRequestStagesLength = arraysize(kRequestStages);

}  // namespace extensions
