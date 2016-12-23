// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/component_updater/component_patcher_operation_out_of_process.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/update_client/component_patcher_operation.h"
#include "content/public/browser/browser_thread.h"
#include "courgette/courgette.h"
#include "courgette/third_party/bsdiff/bsdiff.h"

class OutOfProcessPatchTest : public InProcessBrowserTest {
 public:
  OutOfProcessPatchTest() {
    EXPECT_TRUE(installed_dir_.CreateUniqueTempDir());
    EXPECT_TRUE(input_dir_.CreateUniqueTempDir());
    EXPECT_TRUE(unpack_dir_.CreateUniqueTempDir());
  }

  static base::FilePath test_file(const char* name) {
    base::FilePath path;
    PathService::Get(base::DIR_SOURCE_ROOT, &path);
    return path.AppendASCII("components")
        .AppendASCII("test")
        .AppendASCII("data")
        .AppendASCII("update_client")
        .AppendASCII(name);
  }

  base::FilePath InputFilePath(const char* name) {
    base::FilePath file = installed_dir_.GetPath().AppendASCII(name);

    base::RunLoop run_loop;
    content::BrowserThread::PostBlockingPoolTaskAndReply(
        FROM_HERE,
        base::Bind(&OutOfProcessPatchTest::CopyFile, test_file(name), file),
        run_loop.QuitClosure());

    run_loop.Run();
    return file;
  }

  base::FilePath PatchFilePath(const char* name) {
    base::FilePath file = input_dir_.GetPath().AppendASCII(name);

    base::RunLoop run_loop;
    content::BrowserThread::PostBlockingPoolTaskAndReply(
        FROM_HERE,
        base::Bind(&OutOfProcessPatchTest::CopyFile, test_file(name), file),
        run_loop.QuitClosure());

    run_loop.Run();
    return file;
  }

  base::FilePath OutputFilePath(const char* name) {
    base::FilePath file = unpack_dir_.GetPath().AppendASCII(name);
    return file;
  }

  void RunPatchTest(const std::string& operation,
                    const base::FilePath& input_file,
                    const base::FilePath& patch_file,
                    const base::FilePath& output_file,
                    int expected_result) {
    scoped_refptr<base::SequencedTaskRunner> task_runner =
        base::ThreadTaskRunnerHandle::Get();

    scoped_refptr<update_client::OutOfProcessPatcher> patcher =
        make_scoped_refptr(new component_updater::ChromeOutOfProcessPatcher);

    base::RunLoop run_loop;
    patch_done_called_ = false;
    patcher->Patch(
        operation, task_runner, input_file, patch_file, output_file,
        base::Bind(&OutOfProcessPatchTest::PatchDone, base::Unretained(this),
                   run_loop.QuitClosure(), expected_result));

    run_loop.Run();
    EXPECT_TRUE(patch_done_called_);
  }

 private:
  void PatchDone(const base::Closure& quit_closure, int expected, int result) {
    EXPECT_EQ(expected, result);
    patch_done_called_ = true;
    quit_closure.Run();
  }

  static void CopyFile(const base::FilePath& source,
                       const base::FilePath& target) {
    EXPECT_TRUE(base::CopyFile(source, target));
  }

  base::ScopedTempDir installed_dir_;
  base::ScopedTempDir input_dir_;
  base::ScopedTempDir unpack_dir_;
  bool patch_done_called_;

  DISALLOW_COPY_AND_ASSIGN(OutOfProcessPatchTest);
};

// TODO(noel): Add a test where one or more of the files can't be opened.

// Verify that a 'courgette' delta update operation works correctly.
IN_PROC_BROWSER_TEST_F(OutOfProcessPatchTest, CheckCourgetteOperation) {
  const int kExpectedResult = courgette::C_OK;

  base::FilePath input_file = InputFilePath("binary_input.bin");
  base::FilePath patch_file = PatchFilePath("binary_courgette_patch.bin");
  base::FilePath output_file = OutputFilePath("output.bin");

  RunPatchTest(update_client::kCourgette, input_file, patch_file, output_file,
               kExpectedResult);

  EXPECT_TRUE(base::ContentsEqual(test_file("binary_output.bin"), output_file));
}

// Verify that a 'bsdiff' delta update operation works correctly.
IN_PROC_BROWSER_TEST_F(OutOfProcessPatchTest, CheckBsdiffOperation) {
  const int kExpectedResult = bsdiff::OK;

  base::FilePath input_file = InputFilePath("binary_input.bin");
  base::FilePath patch_file = PatchFilePath("binary_bsdiff_patch.bin");
  base::FilePath output_file = OutputFilePath("output.bin");

  RunPatchTest(update_client::kBsdiff, input_file, patch_file, output_file,
               kExpectedResult);

  EXPECT_TRUE(base::ContentsEqual(test_file("binary_output.bin"), output_file));
}
