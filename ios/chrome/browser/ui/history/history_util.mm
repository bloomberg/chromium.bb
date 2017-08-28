// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/history/history_util.h"

#include "base/i18n/time_formatting.h"
#include "base/time/time.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace history {

base::string16 GetRelativeDateLocalized(const base::Time& visit_time) {
  base::Time midnight = base::Time::Now().LocalMidnight();
  base::string16 date_str = ui::TimeFormat::RelativeDate(visit_time, &midnight);
  if (date_str.empty()) {
    date_str = base::TimeFormatFriendlyDate(visit_time);
  } else {
    date_str = l10n_util::GetStringFUTF16(
        IDS_HISTORY_DATE_WITH_RELATIVE_TIME, date_str,
        base::TimeFormatFriendlyDate(visit_time));
  }
  return date_str;
}

}  // namespace history
