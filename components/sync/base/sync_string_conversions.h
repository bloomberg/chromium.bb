// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_SYNC_STRING_CONVERSIONS_H_
#define COMPONENTS_SYNC_BASE_SYNC_STRING_CONVERSIONS_H_

#include "components/sync/base/sync_export.h"
#include "components/sync/core/connection_status.h"
#include "components/sync/core/sync_encryption_handler.h"

namespace syncer {

SYNC_EXPORT const char* ConnectionStatusToString(ConnectionStatus status);

// Returns the string representation of a PassphraseRequiredReason value.
SYNC_EXPORT const char* PassphraseRequiredReasonToString(
    PassphraseRequiredReason reason);

SYNC_EXPORT const char* PassphraseTypeToString(PassphraseType type);

const char* BootstrapTokenTypeToString(BootstrapTokenType type);
}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_SYNC_STRING_CONVERSIONS_H_
