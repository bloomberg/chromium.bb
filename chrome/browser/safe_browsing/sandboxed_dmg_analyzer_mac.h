// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SANDBOXED_DMG_ANALYZER_MAC_H_
#define CHROME_BROWSER_SAFE_BROWSING_SANDBOXED_DMG_ANALYZER_MAC_H_

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"

namespace safe_browsing {

namespace zip_analyzer {
struct Results;
}

// This class is used to analyze DMG files in a sandboxed utility process for
// file download protection. This class must be created on the UI thread, and
// which is where the result callback will be invoked.
class SandboxedDMGAnalyzer : public content::UtilityProcessHostClient {
 public:
  // Callback that is invoked when the analysis results are ready.
  using ResultsCallback = base::Callback<void(const zip_analyzer::Results&)>;

  SandboxedDMGAnalyzer(const base::FilePath& dmg_file,
                       const ResultsCallback& callback);

  // Begins the analysis. This must be called on the UI thread.
  void Start();

 private:
  ~SandboxedDMGAnalyzer() override;

  // Called on the blocking pool, this opens the DMG file, in preparation for
  // sending the FD to the utility process.
  void OpenDMGFile();

  // Called on the IO thread, this starts the utility process.
  void StartAnalysis();

  // content::UtilityProcessHostClient:
  void OnProcessCrashed(int exit_code) override;
  void OnProcessLaunchFailed(int error_code) override;
  bool OnMessageReceived(const IPC::Message& message) override;

  // Message handler to receive the results of the analysis. Invokes the
  // |callback_|.
  void OnAnalysisFinished(const zip_analyzer::Results& results);

  const base::FilePath file_path_;  // The path of the DMG file.
  base::File file_;  // The opened file handle for |file_path_|.

  const ResultsCallback callback_;  // Result callback.
  bool callback_called_;  // Whether |callback_| has already been invoked.

  DISALLOW_COPY_AND_ASSIGN(SandboxedDMGAnalyzer);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SANDBOXED_DMG_ANALYZER_MAC_H_
