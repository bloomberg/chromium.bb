// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/category_info.h"

namespace ntp_snippets {

CategoryInfo::CategoryInfo(const base::string16& title,
                           ContentSuggestionsCardLayout card_layout,
                           bool has_more_button,
                           bool show_if_empty,
                           const base::string16& no_suggestions_message)
    : title_(title),
      card_layout_(card_layout),
      has_more_button_(has_more_button),
      show_if_empty_(show_if_empty),
      no_suggestions_message_(no_suggestions_message) {}

CategoryInfo::~CategoryInfo() = default;

}  // namespace ntp_snippets
