// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_engine_interface.h"

namespace chromeos {

InputMethodEngineInterface::KeyboardEvent::KeyboardEvent()
    : alt_key(false),
      ctrl_key(false),
      shift_key(false),
      caps_lock(false) {
}

InputMethodEngineInterface::KeyboardEvent::~KeyboardEvent() {
}

InputMethodEngineInterface::MenuItem::MenuItem() {
}

InputMethodEngineInterface::MenuItem::~MenuItem() {
}

InputMethodEngineInterface::Candidate::Candidate() {
}

InputMethodEngineInterface::Candidate::~Candidate() {
}

namespace {
// The default entry number of a page in CandidateWindowProperty.
const int kDefaultPageSize = 9;
}  // namespace

// When the default values are changed, please modify
// CandidateWindow::CandidateWindowProperty defined in chromeos/ime/ too.
InputMethodEngineInterface::CandidateWindowProperty::CandidateWindowProperty()
    : page_size(kDefaultPageSize),
      is_cursor_visible(true),
      is_vertical(false),
      show_window_at_composition(false) {
}

InputMethodEngineInterface::CandidateWindowProperty::~CandidateWindowProperty()
{
}

InputMethodEngineInterface::Observer::~Observer() {
}
}  // namespace chromeos
