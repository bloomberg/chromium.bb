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

#if defined(OS_WIN)
static const char kNpapiTestPluginName[] = "npapi_test_plugin.dll";
static const char kPepperTestPluginName[] = "pepper_test_plugin.dll";
#elif defined(OS_MACOSX)
static const char kNpapiTestPluginName[] = "npapi_test_plugin.plugin";
static const char kPepperTestPluginName[] = "PepperTestPlugin.plugin";
static const char kLayoutPluginName[] = "TestNetscapePlugIn.plugin";
#elif defined(OS_LINUX)
static const char kNpapiTestPluginName[] = "libnpapi_test_plugin.so";
static const char kPepperTestPluginName[] = "libpepper_test_plugin.so";
#endif

NPAPITesterBase::NPAPITesterBase(const std::string& test_plugin_name)
  : test_plugin_name_(test_plugin_name) {
}

void NPAPITesterBase::SetUp() {
  // We need to copy our test-plugin into the plugins directory so that
  // the browser can load it.
  FilePath plugins_directory = GetPluginsDirectory();
  FilePath plugin_src = browser_directory_.AppendASCII(test_plugin_name_);
  ASSERT_TRUE(file_util::PathExists(plugin_src));
  test_plugin_path_ = plugins_directory.AppendASCII(test_plugin_name_);

  file_util::CreateDirectory(plugins_directory);
  ASSERT_TRUE(file_util::CopyDirectory(plugin_src, test_plugin_path_, true))
      << "Copy failed from " << plugin_src.value()
      << " to " << test_plugin_path_.value();

  UITest::SetUp();
}

void NPAPITesterBase::TearDown() {
  // Tear down the UI test first so that the browser stops using the plugin
  // files.
  UITest::TearDown();
  EXPECT_TRUE(file_util::DieFileDie(test_plugin_path_, true));
}

FilePath NPAPITesterBase::GetPluginsDirectory() {
#if defined(OS_MACOSX)
  std::wstring plugin_subpath(chrome::kBrowserProcessExecutableName);
  plugin_subpath.append(L".app/Contents/PlugIns");
  FilePath plugins_directory = browser_directory_.Append(
      FilePath::FromWStringHack(plugin_subpath));
#else
  FilePath plugins_directory = browser_directory_.AppendASCII("plugins");
#endif
  return plugins_directory;
}

NPAPITester::NPAPITester() : NPAPITesterBase(kNpapiTestPluginName) {
}

void NPAPITester::SetUp() {
#if defined(OS_MACOSX)
  // On Windows and Linux, the layout plugin is copied into plugins/ as part of
  // its build process; we can't do the equivalent on the Mac since Chromium
  // doesn't exist during the Test Shell build.
  FilePath layout_src = browser_directory_.AppendASCII(kLayoutPluginName);
  ASSERT_TRUE(file_util::PathExists(layout_src));
  FilePath plugins_directory = GetPluginsDirectory();
  layout_plugin_path_ = plugins_directory.AppendASCII(kLayoutPluginName);
  file_util::CreateDirectory(plugins_directory);
  ASSERT_TRUE(file_util::CopyDirectory(layout_src, layout_plugin_path_, true));
#endif

  NPAPITesterBase::SetUp();
}

void NPAPITester::TearDown() {
  // Tear down the base class first so that the browser stops using the plugin
  // files.
  NPAPITesterBase::TearDown();
#if defined(OS_MACOSX)
  EXPECT_TRUE(file_util::DieFileDie(layout_plugin_path_, true));
#endif  // OS_MACOSX
}

// NPAPIVisiblePluginTester members.
void NPAPIVisiblePluginTester::SetUp() {
  show_window_ = true;
  NPAPITester::SetUp();
}

// NPAPIIncognitoTester members.
void NPAPIIncognitoTester::SetUp() {
  launch_arguments_.AppendSwitch(switches::kIncognito);
  NPAPITester::SetUp();
}

PepperTester::PepperTester() : NPAPITesterBase(kPepperTestPluginName) {
}

void PepperTester::SetUp() {
  // TODO(alokp): Add command-line arguments
  // --no-sandbox --internal-pepper --enable-gpu-plugin
  NPAPITesterBase::SetUp();
}
