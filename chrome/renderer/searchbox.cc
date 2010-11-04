// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/searchbox.h"

SearchBox::SearchBox()
    : verbatim(false),
      selection_start(0),
      selection_end(0),
      x(0),
      y(0),
      width(0),
      height(0) {
}

SearchBox::~SearchBox() {
}
