// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_version_info.h"

ChromeManifestTest::ChromeManifestTest()
    // CHANNEL_UNKNOWN == trunk.
    : current_channel_(chrome::VersionInfo::CHANNEL_UNKNOWN) {}

ChromeManifestTest::~ChromeManifestTest() {
}

base::FilePath ChromeManifestTest::GetTestDataDir() {
  base::FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  return path.AppendASCII("extensions").AppendASCII("manifest_tests");
}
