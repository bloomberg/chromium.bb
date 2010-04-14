// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIAGNOSTICS_RECON_DIAGNOSTICS_H_
#define CHROME_BROWSER_DIAGNOSTICS_RECON_DIAGNOSTICS_H_

#include "base/string16.h"
#include "chrome/browser/diagnostics/diagnostics_test.h"

DiagnosticTest* MakeOperatingSystemTest();
DiagnosticTest* MakeInstallTypeTest();
DiagnosticTest* MakeVersionTest();
DiagnosticTest* MakeUserDirTest();
DiagnosticTest* MakeLocalStateFileTest();
DiagnosticTest* MakeDictonaryDirTest();
DiagnosticTest* MakeInspectorDirTest();
DiagnosticTest* MakeDiskSpaceTest();
DiagnosticTest* MakePreferencesTest();
DiagnosticTest* MakeBookMarksTest();
DiagnosticTest* MakeLocalStateTest();

#endif  // CHROME_BROWSER_DIAGNOSTICS_RECON_DIAGNOSTICS_H_
