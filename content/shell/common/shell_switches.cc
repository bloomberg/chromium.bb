// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/common/shell_switches.h"

#include "base/command_line.h"
#include "base/strings/string_split.h"

namespace switches {

// Allow access to external pages during layout tests.
const char kAllowExternalPages[] = "allow-external-pages";

// Check whether all system dependencies for running layout tests are met.
const char kCheckLayoutTestSysDeps[] = "check-layout-test-sys-deps";

// Tells Content Shell that it's running as a content_browsertest.
const char kContentBrowserTest[] = "browser-test";

// Makes Content Shell use the given path for its data directory.
const char kContentShellDataPath[] = "data-path";

// The directory breakpad should store minidumps in.
const char kCrashDumpsDir[] = "crash-dumps-dir";

// When specified to "enable-leak-detection" command-line option,
// causes the leak detector to cause immediate crash when found leak.
const char kCrashOnFailure[] = "crash-on-failure";

// When run-layout-test is enabled, this causes the line box tree for
// each LayoutBlockFlow to be dumped as well.
const char kDumpLineBoxTrees[] = "dump-line-box-trees";

// Enable accelerated 2D canvas.
const char kEnableAccelerated2DCanvas[] = "enable-accelerated-2d-canvas";

// Enable font antialiasing for pixel tests.
const char kEnableFontAntialiasing[] = "enable-font-antialiasing";

// Always use the complex text path for layout tests.
const char kAlwaysUseComplexText[] = "always-use-complex-text";

// Enables the leak detection of loading webpages. This allows us to check
// whether or not reloading a webpage releases web-related objects correctly.
const char kEnableLeakDetection[] = "enable-leak-detection";

// Encode binary layout test results (images, audio) using base64.
const char kEncodeBinary[] = "encode-binary";

// Exposes the window.internals object to JavaScript for interactive development
// and debugging of layout tests that rely on it.
const char kExposeInternalsForTesting[] = "expose-internals-for-testing";

// Enable site isolation (--site-per-process style isolation) for a subset of
// sites. The argument is a wildcard pattern which will be matched against the
// site URL to determine which sites to isolate. This can be used to isolate
// just one top-level domain, or just one scheme. Example usages:
//     --isolate-sites-for-testing=*.com
//     --isolate-sites-for-testing=https://*
const char kIsolateSitesForTesting[] = "isolate-sites-for-testing";

// Registers additional font files on Windows (for fonts outside the usual
// %WINDIR%\Fonts location). Multiple files can be used by separating them
// with a semicolon (;).
const char kRegisterFontFiles[] = "register-font-files";

// Request the render trees of pages to be dumped as text once they have
// finished loading.
const char kRunLayoutTest[] = "run-layout-test";

// This makes us disable some web-platform runtime features so that we test
// content_shell as if it was a stable release. It is only followed when
// kRunLayoutTest is set. For the features' level, see
// http://dev.chromium.org/blink/runtime-enabled-features.
const char kStableReleaseMode[] = "stable-release-mode";

// Size for the content_shell's host window (i.e. "800x600").
const char kContentShellHostWindowSize[] = "content-shell-host-window-size";

std::vector<std::string> GetSideloadFontFiles() {
  std::vector<std::string> files;
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kRegisterFontFiles)) {
    files = base::SplitString(
        command_line.GetSwitchValueASCII(switches::kRegisterFontFiles),
        ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  }
  return files;
}

}  // namespace switches
