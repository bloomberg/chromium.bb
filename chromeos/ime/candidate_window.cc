// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ime/candidate_window.h"

#include <string>
#include "base/logging.h"
#include "base/values.h"

namespace chromeos {
namespace input_method {

namespace {
// The default entry number of a page in CandidateWindow.
const int kDefaultPageSize = 9;
}  // namespace

CandidateWindow::CandidateWindow()
    : property_(new CandidateWindowProperty) {
}

CandidateWindow::~CandidateWindow() {
}

bool CandidateWindow::IsEqual(const CandidateWindow& cw) const {
  if (page_size() != cw.page_size() ||
      cursor_position() != cw.cursor_position() ||
      is_cursor_visible() != cw.is_cursor_visible() ||
      orientation() != cw.orientation() ||
      show_window_at_composition() != cw.show_window_at_composition() ||
      candidates_.size() != cw.candidates_.size())
    return false;

  for (size_t i = 0; i < candidates_.size(); ++i) {
    const Entry& left = candidates_[i];
    const Entry& right = cw.candidates_[i];
    if (left.value != right.value ||
        left.label != right.label ||
        left.annotation != right.annotation ||
        left.description_title != right.description_title ||
        left.description_body != right.description_body)
      return false;
  }
  return true;
}

void CandidateWindow::CopyFrom(const CandidateWindow& cw) {
  SetProperty(cw.GetProperty());
  candidates_.clear();
  candidates_ = cw.candidates_;
}


// When the default values are changed, please modify
// InputMethodEngineInterface::CandidateWindowProperty too.
CandidateWindow::CandidateWindowProperty::CandidateWindowProperty()
    : page_size(kDefaultPageSize),
      cursor_position(0),
      is_cursor_visible(true),
      is_vertical(false),
      show_window_at_composition(false) {
}

CandidateWindow::CandidateWindowProperty::~CandidateWindowProperty() {
}

CandidateWindow::Entry::Entry() {
}

CandidateWindow::Entry::~Entry() {
}

}  // namespace input_method
}  // namespace chromeos
