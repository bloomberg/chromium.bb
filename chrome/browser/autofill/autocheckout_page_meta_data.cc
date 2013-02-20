// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autocheckout_page_meta_data.h"

namespace autofill {

AutocheckoutPageMetaData::AutocheckoutPageMetaData()
    : current_page_number(-1),
      total_pages(-1) {}

AutocheckoutPageMetaData::~AutocheckoutPageMetaData() {}

bool AutocheckoutPageMetaData::IsStartOfAutofillableFlow() const {
  return current_page_number == 0 && total_pages > 0;
}

bool AutocheckoutPageMetaData::IsInAutofillableFlow() const {
  return current_page_number >= 0 && current_page_number < total_pages;
}

bool AutocheckoutPageMetaData::IsEndOfAutofillableFlow() const {
  return total_pages > 0 && current_page_number == total_pages - 1;
}

}  // namesapce autofill
