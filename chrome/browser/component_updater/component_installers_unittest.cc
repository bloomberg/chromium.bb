// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/flash_component_installer.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/scoped_ptr.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "content/browser/browser_thread.h"
#include "content/common/json_value_serializer.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {
// File name of the Pepper Flash plugin on different platforms.
const FilePath::CharType kDataPath[] =
#if defined(OS_MACOSX)
    FILE_PATH_LITERAL("components\\flapper\\macosx");
#elif defined(OS_WIN)
    FILE_PATH_LITERAL("components\\flapper\\windows");
#else  // OS_LINUX, etc.
    FILE_PATH_LITERAL("components\\flapper\\linux");
#endif
}

TEST(ComponentInstallerTest, PepperFlashAPICheck) {
  MessageLoop message_loop;
  BrowserThread ui_thread(BrowserThread::UI, &message_loop);

  // The test directory is chrome/test/data/components/flapper.
  FilePath manifest;
  PathService::Get(chrome::DIR_TEST_DATA, &manifest);
  manifest = manifest.Append(kDataPath);
  manifest = manifest.AppendASCII("manifest.json");

  if (!file_util::PathExists(manifest))
    return;

  JSONFileValueSerializer serializer(manifest);
  std::string error;
  scoped_ptr<base::Value> root(serializer.Deserialize(NULL, &error));
  ASSERT_TRUE(root.get() != NULL);
  ASSERT_TRUE(root->IsType(base::Value::TYPE_DICTIONARY));

  EXPECT_TRUE(VetoPepperFlashIntefaces(
      static_cast<base::DictionaryValue*>(root.get())));
}
