// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/app/shell_content_main.h"

#include <unistd.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/path_service.h"
#include "content/public/app/content_main.h"
#include "content/public/common/content_switches.h"
#include "content/shell/app/paths_mac.h"
#include "content/shell/app/shell_main_delegate.h"
#include "content/shell/common/shell_switches.h"

namespace {

// Set NSHighResolutionCapable to false when running layout tests, so we match
// the expected pixel results on retina capable displays.
void EnsureCorrectResolutionSettings(int argc, const char** argv) {
  CommandLine command_line(argc, argv);

  // Exit early if this isn't a browser process.
  if (command_line.HasSwitch(switches::kProcessType))
    return;

  NSString* const kHighResolutionCapable = @"NSHighResolutionCapable";
  base::FilePath info_plist = GetInfoPlistPath();
  base::scoped_nsobject<NSMutableDictionary> info_dict(
      [[NSMutableDictionary alloc]
          initWithContentsOfFile:base::mac::FilePathToNSString(info_plist)]);

  bool running_layout_tests = command_line.HasSwitch(switches::kDumpRenderTree);
  bool not_high_resolution_capable =
      [info_dict objectForKey:kHighResolutionCapable] &&
      [[info_dict objectForKey:kHighResolutionCapable] isEqualToNumber:@(NO)];
  if (running_layout_tests == not_high_resolution_capable)
    return;

  // We need to update our Info.plist before we can continue.
  [info_dict setObject:@(!running_layout_tests) forKey:kHighResolutionCapable];
  CHECK([info_dict writeToFile:base::mac::FilePathToNSString(info_plist)
                    atomically:YES]);
  CHECK(execvp(argv[0], const_cast<char**>(argv)));
}

}  // namespace

int ContentMain(int argc, const char** argv) {
  EnsureCorrectResolutionSettings(argc, argv);
  content::ShellMainDelegate delegate;
  return content::ContentMain(argc, argv, &delegate);
}
