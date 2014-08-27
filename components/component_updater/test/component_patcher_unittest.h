// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_TEST_COMPONENT_PATCHER_UNITTEST_H_
#define COMPONENTS_COMPONENT_UPDATER_TEST_COMPONENT_PATCHER_UNITTEST_H_

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "courgette/courgette.h"
#include "courgette/third_party/bsdiff.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

class MockComponentPatcher;
class ReadOnlyTestInstaller;

const char binary_output_hash[] =
    "599aba6d15a7da390621ef1bacb66601ed6aed04dadc1f9b445dcfe31296142a";

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
  base::MessageLoopForIO loop_;
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_TEST_COMPONENT_PATCHER_UNITTEST_H_
