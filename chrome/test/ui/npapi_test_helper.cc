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
#include "chrome/common/chrome_switches.h"

NPAPITester::NPAPITester()
    : UITest() {
}

void NPAPITester::SetUp() {
  // We need to copy our test-plugin into the plugins directory so that
  // the browser can load it.
  FilePath plugins_directory = browser_directory_.AppendASCII("plugins");

  FilePath plugin_src = browser_directory_.AppendASCII("npapi_test_plugin.dll");
  plugin_dll_ = plugins_directory.AppendASCII("npapi_test_plugin.dll");

  file_util::CreateDirectory(plugins_directory);
  file_util::CopyFile(plugin_src, plugin_dll_);

  UITest::SetUp();
}

void NPAPITester::TearDown() {
  file_util::Delete(plugin_dll_, false);
  UITest::TearDown();
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
