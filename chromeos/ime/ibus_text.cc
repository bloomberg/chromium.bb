// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ime/ibus_text.h"

namespace chromeos {

IBusText::IBusText() {}

IBusText::~IBusText() {}

void IBusText::CopyFrom(const IBusText& obj) {
  text_ = obj.text();
  annotation_ = obj.annotation();
  description_title_ = obj.description_title();
  description_body_ = obj.description_body();
  underline_attributes_ = obj.underline_attributes();
  selection_attributes_ = obj.selection_attributes();
}

}  // namespace chromeos
