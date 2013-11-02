// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/install_verification/win/module_list.h"

#include <Windows.h>
#include <vector>
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/install_verification/win/loaded_modules_snapshot.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ModuleListTest, TestCase) {
  std::vector<HMODULE> snapshot;
  ASSERT_TRUE(GetLoadedModulesSnapshot(&snapshot));
  scoped_ptr<ModuleList> module_list(
      ModuleList::FromLoadedModuleSnapshot(snapshot));

  // Lookup the number of loaded modules.
  size_t original_list_size = module_list->size();
  ASSERT_GT(original_list_size, 0u);
  snapshot.clear();

  // Load in a new module.
  HMODULE chrome_dll = ::LoadLibrary(L"chrome.dll");
  ASSERT_NE(static_cast<HMODULE>(NULL), chrome_dll);
  base::ScopedClosureRunner release_chrome_dll(
      base::Bind(base::IgnoreResult(&::FreeLibrary), chrome_dll));

  // Verify that there is an increase in the snapshot size.
  ASSERT_TRUE(GetLoadedModulesSnapshot(&snapshot));
  module_list = ModuleList::FromLoadedModuleSnapshot(snapshot);
  ASSERT_GT(module_list->size(), original_list_size);

  // Unload the module.
  release_chrome_dll.Reset();

  // Reset module_list here. That should typically be the last ref on
  // chrome.dll, so it will be unloaded now.
  module_list.reset();
  ASSERT_EQ(NULL, ::GetModuleHandle(L"chrome.dll"));

  // List the modules from the stale snapshot (including a dangling HMODULE to
  // chrome.dll), simulating a race condition.
  module_list = ModuleList::FromLoadedModuleSnapshot(snapshot);
  ASSERT_EQ(module_list->size(), original_list_size);
}
