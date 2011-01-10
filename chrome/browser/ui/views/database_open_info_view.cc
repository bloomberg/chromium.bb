// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/database_open_info_view.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "grit/generated_resources.h"

namespace {
const int kInfoLabelIds[] = {
    IDS_COOKIES_COOKIE_DOMAIN_LABEL,
    IDS_COOKIES_WEB_DATABASE_NAME,
    IDS_COOKIES_WEB_DATABASE_DESCRIPTION_LABEL,
    IDS_COOKIES_SIZE_LABEL
};
}  // namespace

///////////////////////////////////////////////////////////////////////////////
// DatabaseOpenInfoView, public:

DatabaseOpenInfoView::DatabaseOpenInfoView()
    : GenericInfoView(ARRAYSIZE(kInfoLabelIds), kInfoLabelIds) {
}

void DatabaseOpenInfoView::SetFields(const std::string& host,
                                     const string16& database_name,
                                     const string16& display_name,
                                     unsigned long estimated_size) {
  string16 url = UTF8ToUTF16(host);
  string16 size = FormatBytes(estimated_size,
                              GetByteDisplayUnits(estimated_size),
                              true);
  int row = 0;
  SetValue(row++, url);
  SetValue(row++, database_name);
  SetValue(row++, display_name);
  SetValue(row++, size);
}
