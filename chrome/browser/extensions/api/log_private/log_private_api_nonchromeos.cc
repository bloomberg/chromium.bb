// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/log_private/log_private_api.h"

namespace extensions {

namespace {

const char kErrorNotImplemented[] = "Not implemented";

}  // namespace

LogPrivateGetHistoricalFunction::LogPrivateGetHistoricalFunction() {
}

LogPrivateGetHistoricalFunction::~LogPrivateGetHistoricalFunction() {
}

bool LogPrivateGetHistoricalFunction::RunImpl() {
  SetError(kErrorNotImplemented);
  SendResponse(error_.empty());
  return false;
}

void LogPrivateGetHistoricalFunction::OnSystemLogsLoaded(
    scoped_ptr<chromeos::SystemLogsResponse> sys_info) {
}

}  // namespace extensions
