// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui/npapi_test_helper.h"

#include "base/file_util.h"
#include "base/test/test_file_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "webkit/glue/plugins/plugin_list.h"

#if defined(OS_WIN)
static const char kNpapiTestPluginName[] = "npapi_test_plugin.dll";
#elif defined(OS_MACOSX)
static const char kNpapiTestPluginName[] = "npapi_test_plugin.plugin";
#elif defined(OS_LINUX)
static const char kNpapiTestPluginName[] = "libnpapi_test_plugin.so";
#endif

namespace npapi_test {
const char kTestCompleteCookie[] = "status";
const char kTestCompleteSuccess[] = "OK";
}  // namespace npapi_test.

NPAPITesterBase::NPAPITesterBase(const std::string& test_plugin_name)
  : test_plugin_name_(test_plugin_name) {
}

void NPAPITesterBase::SetUp() {
  // We need to copy our test-plugin into the plugins directory so that
  // the browser can load it.
  // TODO(tc): We should copy the plugins as a build step, not during
  // the tests.  Then we don't have to clean up after the copy in the test.
  FilePath plugins_directory = GetPluginsDirectory();
  FilePath plugin_src = browser_directory_.AppendASCII(test_plugin_name_);
  ASSERT_TRUE(file_util::PathExists(plugin_src));
  test_plugin_path_ = plugins_directory.AppendASCII(test_plugin_name_);

  file_util::CreateDirectory(plugins_directory);
#if defined(OS_WIN)
  file_util::DieFileDie(test_plugin_path_, false);
#endif
  ASSERT_TRUE(file_util::CopyDirectory(plugin_src, test_plugin_path_, true))
      << "Copy failed from " << plugin_src.value()
      << " to " << test_plugin_path_.value();

#if defined(OS_MACOSX)
  // The plugins directory isn't read by default on the Mac, so it needs to be
  // explicitly registered.
  launch_arguments_.AppendSwitchPath(switches::kExtraPluginDir,
                                     plugins_directory);
#endif

  UITest::SetUp();
}

FilePath NPAPITesterBase::GetPluginsDirectory() {
  FilePath plugins_directory = browser_directory_.AppendASCII("plugins");
  return plugins_directory;
}

NPAPITester::NPAPITester() : NPAPITesterBase(kNpapiTestPluginName) {
}

// NPAPIVisiblePluginTester members.
void NPAPIVisiblePluginTester::SetUp() {
  show_window_ = true;
  NPAPITesterBase::SetUp();
}

// NPAPIIncognitoTester members.
void NPAPIIncognitoTester::SetUp() {
  launch_arguments_.AppendSwitch(switches::kIncognito);
  NPAPITesterBase::SetUp();
}
