// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ime/composition_text.h"

namespace chromeos {

CompositionText::CompositionText()
  : selection_start_(0),
    selection_end_(0) {}

CompositionText::~CompositionText() {}

void CompositionText::CopyFrom(const CompositionText& obj) {
  text_ = obj.text();
  underline_attributes_ = obj.underline_attributes();
  selection_start_ = obj.selection_start();
  selection_end_ = obj.selection_end();
}

}  // namespace chromeos
