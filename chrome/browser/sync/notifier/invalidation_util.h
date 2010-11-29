// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Various utilities for dealing with invalidation data types.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_INVALIDATION_UTIL_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_INVALIDATION_UTIL_H_
#pragma once

#include <string>

#include "chrome/browser/sync/syncable/model_type.h"
#include "google/cacheinvalidation/invalidation-client.h"

namespace sync_notifier {

void RunAndDeleteClosure(invalidation::Closure* task);

bool RealModelTypeToObjectId(syncable::ModelType model_type,
                             invalidation::ObjectId* object_id);

bool ObjectIdToRealModelType(const invalidation::ObjectId& object_id,
                             syncable::ModelType* model_type);

// We need to write our own protobuf-to-string functions because we
// use LITE_RUNTIME, which doesn't support DebugString().

std::string ObjectIdToString(const invalidation::ObjectId& object_id);

std::string ObjectIdPToString(const invalidation::ObjectIdP& object_id);

std::string StatusToString(const invalidation::Status& status);

std::string InvalidationToString(
    const invalidation::Invalidation& invalidation);

std::string RegistrationUpdateToString(
    const invalidation::RegistrationUpdate& update);

std::string RegistrationUpdateResultToString(
    const invalidation::RegistrationUpdateResult& update_result);

}  // namespace sync_notifier

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_INVALIDATION_UTIL_H_
