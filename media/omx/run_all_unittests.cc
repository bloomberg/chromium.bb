// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/logging.h"
#include "base/test/test_suite.h"
#include "media/base/media.h"

int main(int argc, char** argv) {
  // Load the OpenMAX library.
  if (!media::InitializeOpenMaxLibrary(FilePath())) {
    LOG(ERROR) << "Unable to initialize OpenMAX library.";
    return -1;
  }
  return base::TestSuite(argc, argv).Run();
}
