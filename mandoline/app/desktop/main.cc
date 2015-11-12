// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/runner/desktop/main_helper.h"

int main(int argc, char** argv) {
  return mojo::runner::RunnerMain(argc, argv, GURL("mojo:desktop_ui"),
                                  base::Closure());
}
