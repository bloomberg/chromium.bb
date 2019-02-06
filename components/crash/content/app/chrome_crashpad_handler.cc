// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/crashpad/crashpad/handler/handler_main.h"

int main(int argc, char* argv[]) {
  return crashpad::HandlerMain(argc, argv, nullptr);
}
