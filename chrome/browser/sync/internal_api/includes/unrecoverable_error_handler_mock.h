// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_INTERNAL_API_INCLUDES_UNRECOVERABLE_ERROR_HANDLER_MOCK_H_
#define CHROME_BROWSER_SYNC_INTERNAL_API_INCLUDES_UNRECOVERABLE_ERROR_HANDLER_MOCK_H_
#pragma once
#include <string>

#include "base/location.h"
#include "chrome/browser/sync/internal_api/includes/unrecoverable_error_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browser_sync {
class MockUnrecoverableErrorHandler : public UnrecoverableErrorHandler {
 public:
  MockUnrecoverableErrorHandler();
  ~MockUnrecoverableErrorHandler();
  MOCK_METHOD2(OnUnrecoverableError, void(
      const tracked_objects::Location&,
      const std::string&));
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_INTERNAL_API_INCLUDES_UNRECOVERABLE_ERROR_HANDLER_MOCK_H_

