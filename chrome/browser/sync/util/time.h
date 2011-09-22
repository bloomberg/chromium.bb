// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Time-related sync functions.

#ifndef CHROME_BROWSER_SYNC_UTIL_TIME_H_
#define CHROME_BROWSER_SYNC_UTIL_TIME_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/time.h"

namespace browser_sync {

// Converts a time object to the format used in sync protobufs (ms
// since the Unix epoch).
int64 TimeToProtoTime(const base::Time& t);

// Converts a time field from sync protobufs to a time object.
base::Time ProtoTimeToTime(int64 proto_t);

std::string GetTimeDebugString(const base::Time& t);

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_UTIL_TIME_H_
