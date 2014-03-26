// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_TEST_COMPONENT_PATCHER_UNITTEST_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_TEST_COMPONENT_PATCHER_UNITTEST_H_

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "courgette/courgette.h"
#include "courgette/third_party/bsdiff.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

class MockComponentPatcher;
class ReadOnlyTestInstaller;

const char binary_output_hash[] =
    "599aba6d15a7da390621ef1bacb66601ed6aed04dadc1f9b445dcfe31296142a";

// These constants are duplicated from chrome/installer/util/util_constants.h,
// to avoid introducing a dependency from the unit tests to the installer.
const int kCourgetteErrorOffset = 300;
const int kBsdiffErrorOffset = 600;

class ComponentPatcherOperationTest : public testing::Test {
 public:
  explicit ComponentPatcherOperationTest();
  virtual ~ComponentPatcherOperationTest();

 protected:
  base::ScopedTempDir input_dir_;
  base::ScopedTempDir installed_dir_;
  base::ScopedTempDir unpack_dir_;
  scoped_ptr<ReadOnlyTestInstaller> installer_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
};

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_TEST_COMPONENT_PATCHER_UNITTEST_H_
