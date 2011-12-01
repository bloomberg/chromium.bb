// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/flash_component_installer.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_value_serializer.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "content/test/test_browser_thread.h"

#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace {
// File name of the Pepper Flash plugin on different platforms.
const FilePath::CharType kDataPath[] =
#if defined(OS_MACOSX)
    FILE_PATH_LITERAL("components/flapper/mac");
#elif defined(OS_WIN)
    FILE_PATH_LITERAL("components\\flapper\\windows");
#else  // OS_LINUX, etc.
#if defined(ARCH_CPU_X86)
    FILE_PATH_LITERAL("components/flapper/linux");
#elif defined(ARCH_CPU_X86_64)
    FILE_PATH_LITERAL("components/flapper/linux_x64");
#else
    FILE_PATH_LITERAL("components/flapper/NONEXISTENT");
#endif
#endif
}

// TODO(viettrungluu): Separate out into two separate tests; use a test fixture.
TEST(ComponentInstallerTest, PepperFlashCheck) {
  MessageLoop message_loop;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop);

  // The test directory is chrome/test/data/components/flapper.
  FilePath manifest;
  PathService::Get(chrome::DIR_TEST_DATA, &manifest);
  manifest = manifest.Append(kDataPath);
  manifest = manifest.AppendASCII("manifest.json");

  if (!file_util::PathExists(manifest)) {
    LOG(WARNING) << "No test manifest available. Skipping.";
    return;
  }

  JSONFileValueSerializer serializer(manifest);
  std::string error;
  scoped_ptr<base::Value> root(serializer.Deserialize(NULL, &error));
  ASSERT_TRUE(root.get() != NULL);
  ASSERT_TRUE(root->IsType(base::Value::TYPE_DICTIONARY));

  // This just tests the API (i.e., Pepper interfaces).
  EXPECT_TRUE(VetoPepperFlashIntefaces(
      static_cast<base::DictionaryValue*>(root.get())));

  // This checks that the whole manifest is compatible.
  Version version;
  EXPECT_TRUE(CheckPepperFlashManifest(
      static_cast<base::DictionaryValue*>(root.get()), &version));
  EXPECT_TRUE(version.IsValid());
}
