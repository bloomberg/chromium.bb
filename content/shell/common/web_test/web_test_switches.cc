// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/common/web_test/web_test_switches.h"

#include "base/command_line.h"
#include "base/strings/string_split.h"

namespace switches {

// Allow access to external pages during web tests.
const char kAllowExternalPages[] = "allow-external-pages";

// When specified to "enable-leak-detection" command-line option,
// causes the leak detector to cause immediate crash when found leak.
const char kCrashOnFailure[] = "crash-on-failure";

// Run devtools tests in debug mode (not bundled and minified)
const char kDebugDevTools[] = "debug-devtools";

// Enable accelerated 2D canvas.
const char kEnableAccelerated2DCanvas[] = "enable-accelerated-2d-canvas";

// Enable font antialiasing for pixel tests.
const char kEnableFontAntialiasing[] = "enable-font-antialiasing";

// Always use the complex text path for web tests.
const char kAlwaysUseComplexText[] = "always-use-complex-text";

// Enables the leak detection of loading webpages. This allows us to check
// whether or not reloading a webpage releases web-related objects correctly.
const char kEnableLeakDetection[] = "enable-leak-detection";

// Encode binary web test results (images, audio) using base64.
const char kEncodeBinary[] = "encode-binary";

// Disables the automatic origin isolation of web platform test domains.
// We normally origin-isolate them for better test coverage, but tests of opt-in
// origin isolation need to disable this.
const char kDisableAutoWPTOriginIsolation[] =
    "disable-auto-wpt-origin-isolation";

// This makes us disable some web-platform runtime features so that we test
// content_shell as if it was a stable release. It is only followed when
// kRunWebTest is set. For the features' level, see
// http://dev.chromium.org/blink/runtime-enabled-features.
const char kStableReleaseMode[] = "stable-release-mode";

// Disables the shell from beginning in headless mode. Tests will then attempt
// to use the hardware GPU for rendering. This is only followed when
// kRunWebTests is set.
const char kDisableHeadlessMode[] = "disable-headless-mode";

#if defined(OS_WIN)
// Registers additional font files on Windows (for fonts outside the usual
// %WINDIR%\Fonts location). Multiple files can be used by separating them
// with a semicolon (;).
const char kRegisterFontFiles[] = "register-font-files";

std::vector<std::string> GetSideloadFontFiles() {
  std::vector<std::string> files;
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kRegisterFontFiles)) {
    files = base::SplitString(
        command_line.GetSwitchValueASCII(switches::kRegisterFontFiles), ";",
        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  }
  return files;
}
#endif

}  // namespace switches
