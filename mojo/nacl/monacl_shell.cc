// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "mojo/embedder/embedder.h"
#include "mojo/embedder/simple_platform_support.h"
#include "mojo/nacl/monacl_sel_main.h"


int main(int argc, char* argv[]) {
  if (argc < 3) {
    std::cout << "Usage: " << argv[0] << " irt.nexe app.nexe [args for app]" <<
        std::endl;
    return 1;
  }

  const char* irt_file = argv[1];
  const char* nexe_file = argv[2];

  mojo::embedder::Init(scoped_ptr<mojo::embedder::PlatformSupport>(
      new mojo::embedder::SimplePlatformSupport()));

  // Does not return.
  mojo::LaunchNaCl(nexe_file, irt_file, argc - 2, argv + 2);
  return 1;
}
