// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ime/ibus_text.h"

namespace chromeos {

IBusText::IBusText()
  : selection_start_(0),
    selection_end_(0) {}

IBusText::~IBusText() {}

void IBusText::CopyFrom(const IBusText& obj) {
  text_ = obj.text();
  underline_attributes_ = obj.underline_attributes();
  selection_start_ = obj.selection_start();
  selection_end_ = obj.selection_end();
}

}  // namespace chromeos
