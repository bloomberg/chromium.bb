// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_engine.h"

#include "chrome/browser/chromeos/input_method/input_method_engine_ibus.h"

namespace chromeos {

InputMethodEngine::KeyboardEvent::KeyboardEvent()
    : alt_key(false),
      ctrl_key(false),
      shift_key(false) {
}

InputMethodEngine::KeyboardEvent::~KeyboardEvent() {
}

InputMethodEngine::MenuItem::MenuItem() {
}

InputMethodEngine::MenuItem::~MenuItem() {
}

InputMethodEngine::Candidate::Candidate() {
}

InputMethodEngine::Candidate::~Candidate() {
}

namespace {
// The default entry number of a page in CandidateWindowProperty.
const int kDefaultPageSize = 9;
}  // namespace

// When the default values are changed, please modify
// CandidateWindow::CandidateWindowProperty defined in chromeos/ime/ too.
InputMethodEngine::CandidateWindowProperty::CandidateWindowProperty()
    : page_size(kDefaultPageSize),
      is_cursor_visible(true),
      is_vertical(false),
      show_window_at_composition(false) {
}

InputMethodEngine::CandidateWindowProperty::~CandidateWindowProperty() {
}

InputMethodEngine::Observer::~Observer() {
}
}  // namespace chromeos
