// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/protocol/sync_protocol_error.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"

namespace syncer {
#define ENUM_CASE(x) \
  case x:            \
    return #x;       \
    break;

const char* GetSyncErrorTypeString(SyncProtocolErrorType type) {
  switch (type) {
    ENUM_CASE(SYNC_SUCCESS);
    ENUM_CASE(NOT_MY_BIRTHDAY);
    ENUM_CASE(THROTTLED);
    ENUM_CASE(CLEAR_PENDING);
    ENUM_CASE(TRANSIENT_ERROR);
    ENUM_CASE(MIGRATION_DONE);
    ENUM_CASE(INVALID_CREDENTIAL);
    ENUM_CASE(DISABLED_BY_ADMIN);
    ENUM_CASE(PARTIAL_FAILURE);
    ENUM_CASE(CLIENT_DATA_OBSOLETE);
    ENUM_CASE(UNKNOWN_ERROR);
  }
  NOTREACHED();
  return "";
}

const char* GetClientActionString(ClientAction action) {
  switch (action) {
    ENUM_CASE(UPGRADE_CLIENT);
    ENUM_CASE(CLEAR_USER_DATA_AND_RESYNC);
    ENUM_CASE(ENABLE_SYNC_ON_ACCOUNT);
    ENUM_CASE(STOP_AND_RESTART_SYNC);
    ENUM_CASE(DISABLE_SYNC_ON_CLIENT);
    ENUM_CASE(STOP_SYNC_FOR_DISABLED_ACCOUNT);
    ENUM_CASE(RESET_LOCAL_SYNC_DATA);
    ENUM_CASE(UNKNOWN_ACTION);
  }
  NOTREACHED();
  return "";
}

SyncProtocolError::SyncProtocolError()
    : error_type(UNKNOWN_ERROR), action(UNKNOWN_ACTION) {}

SyncProtocolError::SyncProtocolError(const SyncProtocolError& other) = default;

SyncProtocolError::~SyncProtocolError() {}

std::unique_ptr<base::DictionaryValue> SyncProtocolError::ToValue() const {
  auto value = base::MakeUnique<base::DictionaryValue>();
  value->SetString("ErrorType", GetSyncErrorTypeString(error_type));
  value->SetString("ErrorDescription", error_description);
  value->SetString("url", url);
  value->SetString("action", GetClientActionString(action));
  return value;
}

}  // namespace syncer
