// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/category_info.h"

namespace ntp_snippets {

CategoryInfo::CategoryInfo(const base::string16& title,
                           ContentSuggestionsCardLayout card_layout)
    : title_(title), card_layout_(card_layout) {}

CategoryInfo::~CategoryInfo() {}

}  // namespace ntp_snippets
