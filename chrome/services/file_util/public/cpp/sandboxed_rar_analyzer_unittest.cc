// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/public/cpp/sandboxed_rar_analyzer.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/services/file_util/file_util_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SandboxedRarAnalyzerTest : public testing::Test {
 public:
  SandboxedRarAnalyzerTest()
      : browser_thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        test_connector_factory_(
            service_manager::TestConnectorFactory::CreateForUniqueService(
                std::make_unique<FileUtilService>())),
        connector_(test_connector_factory_->CreateConnector()) {}

  void AnalyzeFile(const base::FilePath& path,
                   safe_browsing::ArchiveAnalyzerResults* results) {
    base::RunLoop run_loop;
    ResultsGetter results_getter(run_loop.QuitClosure(), results);
    scoped_refptr<SandboxedRarAnalyzer> analyzer(new SandboxedRarAnalyzer(
        path, results_getter.GetCallback(), connector_.get()));
    analyzer->Start();
    run_loop.Run();
  }

  base::FilePath GetFilePath(const char* file_name) {
    base::FilePath test_data;
    EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data));
    return test_data.AppendASCII("safe_browsing")
        .AppendASCII("rar")
        .AppendASCII(file_name);
  }

 private:
  // A helper that provides a SandboxedRarAnalyzer::ResultCallback that will
  // store a copy of an analyzer's results and then run a closure.
  class ResultsGetter {
   public:
    ResultsGetter(const base::RepeatingClosure& next_closure,
                  safe_browsing::ArchiveAnalyzerResults* results)
        : next_closure_(next_closure), results_(results) {}

    SandboxedRarAnalyzer::ResultCallback GetCallback() {
      return base::BindRepeating(&ResultsGetter::ResultsCallback,
                                 base::Unretained(this));
    }

   private:
    void ResultsCallback(const safe_browsing::ArchiveAnalyzerResults& results) {
      *results_ = results;
      next_closure_.Run();
    }

    base::RepeatingClosure next_closure_;
    safe_browsing::ArchiveAnalyzerResults* results_;

    DISALLOW_COPY_AND_ASSIGN(ResultsGetter);
  };

  content::TestBrowserThreadBundle browser_thread_bundle_;
  content::InProcessUtilityThreadHelper utility_thread_helper_;
  std::unique_ptr<service_manager::TestConnectorFactory>
      test_connector_factory_;
  std::unique_ptr<service_manager::Connector> connector_;
};

TEST_F(SandboxedRarAnalyzerTest, AnalyzeBenignRar) {
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("small_archive.rar"));

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(path, &results);

  ASSERT_TRUE(results.success);
  EXPECT_FALSE(results.has_executable);
  EXPECT_EQ(0, results.archived_binary.size());
  EXPECT_EQ(0u, results.archived_archive_filenames.size());
}

TEST_F(SandboxedRarAnalyzerTest, AnalyzeRarWithPassword) {
  // Can list files inside an archive that has password protected data.
  // passwd.rar contains 1 file: file1.txt
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("passwd.rar"));

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(path, &results);

  ASSERT_TRUE(results.success);
  EXPECT_FALSE(results.has_executable);
  EXPECT_EQ(0, results.archived_binary.size());
  EXPECT_EQ(0u, results.archived_archive_filenames.size());
}

TEST_F(SandboxedRarAnalyzerTest, AnalyzeRarContainingExecutable) {
  // Can detect when .rar contains executable files.
  // has_exe.rar contains 1 file: Yahoo-Warez.exe
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("has_exe.rar"));

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(path, &results);

  ASSERT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  EXPECT_EQ(0, results.archived_binary.size());
  EXPECT_EQ(0u, results.archived_archive_filenames.size());
}

TEST_F(SandboxedRarAnalyzerTest, AnalyzeTextAsRar) {
  // Catches when a file isn't a a valid RAR file.
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("not_a_rar.rar"));

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(path, &results);

  ASSERT_FALSE(results.success);
  EXPECT_FALSE(results.has_executable);
  EXPECT_EQ(0, results.archived_binary.size());
  EXPECT_EQ(0u, results.archived_archive_filenames.size());
}

TEST_F(SandboxedRarAnalyzerTest, AnalyzeRarContainingArchive) {
  // Can detect when .rar contains executable files.
  // has_archive.rar contains 1 file: empty.zip
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("has_archive.rar"));

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(path, &results);

  ASSERT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);  // .zip is considered binary executable.
  EXPECT_EQ(0, results.archived_binary.size());
  EXPECT_EQ(1u, results.archived_archive_filenames.size());
}

TEST_F(SandboxedRarAnalyzerTest, AnalyzeRarContainingAssortmentOfFiles) {
  // Can detect when .rar contains a mix of different intereting types.
  // has_exe_rar_text_zip.rar contains: content.exe, not_a_rar.rar, text.txt,
  // empty.zip
  base::FilePath path;
  ASSERT_NO_FATAL_FAILURE(path = GetFilePath("has_exe_rar_text_zip.rar"));

  safe_browsing::ArchiveAnalyzerResults results;
  AnalyzeFile(path, &results);

  ASSERT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  EXPECT_EQ(0, results.archived_binary.size());
  EXPECT_EQ(2u, results.archived_archive_filenames.size());
}

}  // namespace
