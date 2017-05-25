// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_MOCK_CHROME_CLEANER_PROCESS_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_MOCK_CHROME_CLEANER_PROCESS_WIN_H_

#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "components/chrome_cleaner/public/interfaces/chrome_prompt.mojom.h"

namespace safe_browsing {

// Class that mocks the behaviour of the Chrome Cleaner process. Intended to be
// used in multi process tests. Example Usage:
//
// MULTIPROCESS_TEST_MAIN(MockChromeCleanerProcessMain) {
//   base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
//   MockChromeCleanerProcess::Options options;
//   EXPECT_TRUE(MockChromeCleanerProcess::Options::FromCommandLine(
//       *command_line, &options));
//   std::string chrome_mojo_pipe_token = ...
//   EXPECT_FALSE(chrome_mojo_pipe_token.empty())
//
//   if (::testing::Test::HasFailure())
//     return MockChromeCleanerProcess::kInternalTestFailureExitCode;
//   MockChromeCleanerProcess mock_cleaner_process(options,
//                                                 chrome_mojo_pipe_token);
//   return mock_cleaner_process.Run();
// }
class MockChromeCleanerProcess {
 public:
  enum class CrashPoint {
    kNone,
    kOnStartup,
    kAfterConnection,
    kAfterRequestSent,
    kAfterResponseReceived,
    kNumCrashPoints,
  };

  static constexpr int kInternalTestFailureExitCode = -1;
  static constexpr int kDeliberateCrashExitCode = -2;
  static constexpr int kNothingFoundExitCode = 2;
  static constexpr int kDeclinedExitCode = 44;
  static constexpr int kRebootRequiredExitCode = 15;
  static constexpr int kRebootNotRequiredExitCode = 0;

  class Options {
   public:
    static bool FromCommandLine(const base::CommandLine& command_line,
                                Options* options);

    Options();
    Options(const Options& other);
    Options& operator=(const Options& other);
    ~Options();

    void AddSwitchesToCommandLine(base::CommandLine* command_line) const;

    void SetDoFindUws(bool do_find_uws);
    const std::vector<chrome_cleaner::mojom::UwSPtr>& found_uws() const {
      return found_uws_;
    }
    const std::set<base::FilePath>& files_to_delete() const {
      return files_to_delete_;
    }

    void set_reboot_required(bool reboot_required) {
      reboot_required_ = reboot_required;
    }
    bool reboot_required() const { return reboot_required_; }

    void set_crash_point(CrashPoint crash_point) { crash_point_ = crash_point; }
    CrashPoint crash_point() const { return crash_point_; }

    int ExpectedExitCode(chrome_cleaner::mojom::PromptAcceptance
                             received_prompt_acceptance) const;

   private:
    std::vector<chrome_cleaner::mojom::UwSPtr> found_uws_;
    std::set<base::FilePath> files_to_delete_;
    bool reboot_required_ = false;
    CrashPoint crash_point_ = CrashPoint::kNone;
  };

  MockChromeCleanerProcess(const Options& options,
                           const std::string& chrome_mojo_pipe_token);

  // Call this in the main function of the mock Chrome Cleaner process. Returns
  // the exit code that should be used when the process exits.
  //
  // If a crash point has been specified in the options passed to the
  // constructor, the process will exit with a kDeliberateCrashExitCode exit
  // code, and this function will not return.
  int Run();

 private:
  // Function that receives the Mojo response to the PromptUser message.
  void SendScanResults(
      chrome_cleaner::mojom::ChromePromptPtrInfo prompt_ptr_info,
      base::OnceClosure quit_closure);
  void PromptUserCallback(
      base::OnceClosure quit_closure,
      chrome_cleaner::mojom::PromptAcceptance prompt_acceptance);

  Options options_;
  std::string chrome_mojo_pipe_token_;
  // The PromptAcceptance received in PromptUserCallback().
  chrome_cleaner::mojom::PromptAcceptance received_prompt_acceptance_ =
      chrome_cleaner::mojom::PromptAcceptance::UNSPECIFIED;
  chrome_cleaner::mojom::ChromePromptPtr* chrome_prompt_ptr_ = nullptr;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_MOCK_CHROME_CLEANER_PROCESS_WIN_H_
