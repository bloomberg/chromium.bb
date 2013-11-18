// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/try_catch.h"

#include "gin/converter.h"

namespace gin {

TryCatch::TryCatch() {
}

TryCatch::~TryCatch() {
}

bool TryCatch::HasCaught() {
  return try_catch_.HasCaught();
}

std::string TryCatch::GetPrettyMessage() {
  std::string info;
  ConvertFromV8(try_catch_.Message()->Get(), &info);

  std::string sounce_line;
  if (ConvertFromV8(try_catch_.Message()->GetSourceLine(), &sounce_line))
    info += "\n" + sounce_line;

  return info;
}

}  // namespace gin
