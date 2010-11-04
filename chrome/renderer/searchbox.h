// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SEARCHBOX_H_
#define CHROME_RENDERER_SEARCHBOX_H_
#pragma once

#include "base/string16.h"

struct SearchBox {
  SearchBox();
  ~SearchBox();

  string16 value;
  bool verbatim;
  uint32 selection_start;
  uint32 selection_end;
  uint32 x;
  uint32 y;
  uint32 width;
  uint32 height;
};

#endif  // CHROME_RENDERER_SEARCHBOX_H_
