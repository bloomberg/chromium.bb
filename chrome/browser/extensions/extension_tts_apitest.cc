// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/command_line.h"
#include "chrome/browser/chromeos/cros/cros_mock.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"
#include "testing/gmock/include/gmock/gmock.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Tts) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  #if defined(OS_CHROMEOS)
  chromeos::CrosMock crosMock;
  crosMock.InitMockSpeechSynthesisLibrary();
  crosMock.SetSpeechSynthesisLibraryExpectations();
  #endif

  ASSERT_TRUE(RunExtensionTest("tts")) << message_;
}
