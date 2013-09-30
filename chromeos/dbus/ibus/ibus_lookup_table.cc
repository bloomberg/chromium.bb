// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/ibus_lookup_table.h"

#include <string>
#include "base/logging.h"
#include "base/values.h"

namespace chromeos {

namespace {
// The default entry number of a page in IBusLookupTable.
const int kDefaultPageSize = 9;
}  // namespace

///////////////////////////////////////////////////////////////////////////////
// IBusLookupTable
IBusLookupTable::IBusLookupTable()
    : property_(new CandidateWindowProperty) {
}

IBusLookupTable::~IBusLookupTable() {
}

bool IBusLookupTable::IsEqual(const IBusLookupTable& table) const {
  if (page_size() != table.page_size() ||
      cursor_position() != table.cursor_position() ||
      is_cursor_visible() != table.is_cursor_visible() ||
      orientation() != table.orientation() ||
      show_window_at_composition() != table.show_window_at_composition() ||
      candidates_.size() != table.candidates_.size())
    return false;

  for (size_t i = 0; i < candidates_.size(); ++i) {
    const Entry& left = candidates_[i];
    const Entry& right = table.candidates_[i];
    if (left.value != right.value ||
        left.label != right.label ||
        left.annotation != right.annotation ||
        left.description_title != right.description_title ||
        left.description_body != right.description_body)
      return false;
  }
  return true;
}

void IBusLookupTable::CopyFrom(const IBusLookupTable& table) {
  SetProperty(table.GetProperty());
  candidates_.clear();
  candidates_ = table.candidates_;
}


// When the default values are changed, please modify
// InputMethodEngine::CandidateWindowProperty too.
IBusLookupTable::CandidateWindowProperty::CandidateWindowProperty()
    : page_size(kDefaultPageSize),
      cursor_position(0),
      is_cursor_visible(true),
      is_vertical(false),
      show_window_at_composition(false) {
}

IBusLookupTable::CandidateWindowProperty::~CandidateWindowProperty() {
}

IBusLookupTable::Entry::Entry() {
}

IBusLookupTable::Entry::~Entry() {
}

}  // namespace chromeos
