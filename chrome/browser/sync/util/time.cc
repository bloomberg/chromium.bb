// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/time.h"

#include "base/i18n/time_formatting.h"
#include "base/utf_string_conversions.h"

namespace browser_sync {

int64 TimeToProtoTime(const base::Time& t) {
  return (t - base::Time::UnixEpoch()).InMilliseconds();
}

base::Time ProtoTimeToTime(int64 proto_t) {
  return base::Time::UnixEpoch() + base::TimeDelta::FromMilliseconds(proto_t);
}

std::string GetTimeDebugString(const base::Time& t) {
  return UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(t));
}

}  // namespace browser_sync
