// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/app/chrome_main_delegate.h"
#include "chrome/app/mash/chrome_test_catalog.h"

extern "C" {
int ChromeMain(int argc, const char** argv);
}

int main(int argc, const char** argv) {
  ChromeMainDelegate::InstallServiceCatalogFactory(
      base::Bind(&CreateChromeTestCatalog));
  return ChromeMain(argc, argv);
}
