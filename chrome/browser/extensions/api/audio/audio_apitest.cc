// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/extensions/api/audio/audio_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"
#include "extensions/test/extension_test_message_listener.h"

namespace extensions {

class AudioApiTest: public ExtensionApiTest {
 public:
  AudioApiTest() {}
  virtual ~AudioApiTest() {}
};

#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(AudioApiTest, Audio) {
  EXPECT_TRUE(RunExtensionTest("audio")) << message_;
}
#endif

} // namespace extensions
