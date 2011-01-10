// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/appcache_info_view.h"

#include "base/i18n/time_formatting.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "grit/generated_resources.h"

namespace {
const int kInfoLabelIds[] = {
    IDS_COOKIES_APPLICATION_CACHE_MANIFEST_LABEL,
    IDS_COOKIES_SIZE_LABEL,
    IDS_COOKIES_COOKIE_CREATED_LABEL,
    IDS_COOKIES_LAST_ACCESSED_LABEL
};
}  // namespace

AppCacheInfoView::AppCacheInfoView()
    : GenericInfoView(ARRAYSIZE(kInfoLabelIds), kInfoLabelIds) {
}

void AppCacheInfoView::SetAppCacheInfo(const appcache::AppCacheInfo* info) {
  DCHECK(info);
  string16 manifest_url =
      UTF8ToUTF16(info->manifest_url.spec());
  string16 size =
      FormatBytes(info->size, GetByteDisplayUnits(info->size), true);
  string16 creation_date =
      base::TimeFormatFriendlyDateAndTime(info->creation_time);
  string16 last_access_date =
      base::TimeFormatFriendlyDateAndTime(info->last_access_time);
  int row = 0;
  SetValue(row++, manifest_url);
  SetValue(row++, size);
  SetValue(row++, creation_date);
  SetValue(row++, last_access_date);
}
