// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/service/win/cloud_print_service.h"

#include "base/at_exit.h"

CloudPrintService _AtlModule;

int main() {
  base::AtExitManager at_exit;
  return _AtlModule.WinMain(0);
}
