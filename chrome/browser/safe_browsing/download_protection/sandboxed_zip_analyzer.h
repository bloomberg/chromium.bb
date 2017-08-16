// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_SANDBOXED_ZIP_ANALYZER_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_SANDBOXED_ZIP_ANALYZER_H_

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/common/safe_browsing/archive_analyzer_results.h"
#include "chrome/common/safe_browsing/safe_archive_analyzer.mojom.h"
#include "content/public/browser/utility_process_mojo_client.h"

namespace safe_browsing {

// This class is used to analyze zip files in a sandboxed utility process
// for file download protection. This class lives on the UI thread, which
// is where the result callback will be invoked.
class SandboxedZipAnalyzer
    : public base::RefCountedThreadSafe<SandboxedZipAnalyzer> {
 public:
  using ResultCallback = base::Callback<void(const ArchiveAnalyzerResults&)>;

  SandboxedZipAnalyzer(const base::FilePath& zip_file,
                       const ResultCallback& callback);

  // Starts the analysis. Must be called on the UI thread.
  void Start();

 private:
  friend class base::RefCountedThreadSafe<SandboxedZipAnalyzer>;

  ~SandboxedZipAnalyzer();

  // Prepare the file for analysis.
  void PrepareFileToAnalyze();

  // If file preparation failed, analysis has failed: report failure.
  void ReportFileFailure();

  // Starts the utility process and sends it a file analyze request.
  void AnalyzeFile(base::File file, base::File temp);

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

  DISALLOW_COPY_AND_ASSIGN(SandboxedZipAnalyzer);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_SANDBOXED_ZIP_ANALYZER_H_
