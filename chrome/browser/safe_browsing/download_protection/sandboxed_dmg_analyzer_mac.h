// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_SANDBOXED_DMG_ANALYZER_MAC_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_SANDBOXED_DMG_ANALYZER_MAC_H_

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/common/safe_browsing/safe_archive_analyzer.mojom.h"
#include "content/public/browser/utility_process_mojo_client.h"

namespace safe_browsing {

// This class is used to analyze DMG files in a sandboxed utility process
// for file download protection. This class lives on the UI thread, which
// is where the result callback will be invoked.
class SandboxedDMGAnalyzer
    : public base::RefCountedThreadSafe<SandboxedDMGAnalyzer> {
 public:
  using ResultCallback = base::Callback<void(const ArchiveAnalyzerResults&)>;

  SandboxedDMGAnalyzer(const base::FilePath& dmg_file,
                       const ResultCallback& callback);

  // Starts the analysis. Must be called on the UI thread.
  void Start();

 private:
  friend class base::RefCountedThreadSafe<SandboxedDMGAnalyzer>;

  ~SandboxedDMGAnalyzer();

  // Prepare the file for analysis.
  void PrepareFileToAnalyze();

  // If file preparation failed, analysis has failed: report failure.
  void ReportFileFailure();

  // Starts the utility process and sends it a file analyze request.
  void AnalyzeFile(base::File file);

  // The response containing the file analyze results.
  void AnalyzeFileDone(const ArchiveAnalyzerResults& results);

  // The file path of the file to analyze.
  const base::FilePath file_path_;

  // Utility client used to send analyze tasks to the utility process.
  std::unique_ptr<
      content::UtilityProcessMojoClient<chrome::mojom::SafeArchiveAnalyzer>>
      utility_process_mojo_client_;

  // Callback invoked on the UI thread with the file analyze results.
  const ResultCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(SandboxedDMGAnalyzer);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_SANDBOXED_DMG_ANALYZER_MAC_H_
