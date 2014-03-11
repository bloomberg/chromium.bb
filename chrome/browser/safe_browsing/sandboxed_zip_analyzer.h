// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Browser-side interface to analyze zip files for SafeBrowsing download
// protection.  The actual zip decoding is performed in a sandboxed utility
// process.
//
// This class lives on the UI thread.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SANDBOXED_ZIP_ANALYZER_H_
#define CHROME_BROWSER_SAFE_BROWSING_SANDBOXED_ZIP_ANALYZER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"

namespace IPC {
class Message;
}

namespace safe_browsing {
namespace zip_analyzer {
struct Results;
}

class SandboxedZipAnalyzer : public content::UtilityProcessHostClient {
 public:
  // Callback that is invoked when the analysis results are ready.
  typedef base::Callback<void(const zip_analyzer::Results&)> ResultCallback;

  SandboxedZipAnalyzer(const base::FilePath& zip_file,
                       const ResultCallback& result_callback);

  // Posts a task to start the zip analysis in the utility process.
  void Start();

 private:
  virtual ~SandboxedZipAnalyzer();

  // Creates the sandboxed utility process and tells it to start analysis.
  // Runs on a worker thread.
  void AnalyzeInSandbox();

  // content::UtilityProcessHostClient implementation.
  // These notifications run on the IO thread.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Notification that the utility process is running, and we can now get its
  // process handle.
  void OnUtilityProcessStarted();

  // Notification from the utility process that the zip file has been analyzed,
  // with the given results.  Runs on the IO thread.
  void OnAnalyzeZipFileFinished(const zip_analyzer::Results& results);

  // Launches the utility process.  Must run on the IO thread.
  void StartProcessOnIOThread();

  const base::FilePath zip_file_name_;
  // Once we have opened the file, we store the handle so that we can use it
  // once the utility process has launched.
  base::File zip_file_;
  base::WeakPtr<content::UtilityProcessHost> utility_process_host_;
  const ResultCallback callback_;
  // Initialized on the UI thread, but only accessed on the IO thread.
  bool callback_called_;

  DISALLOW_COPY_AND_ASSIGN(SandboxedZipAnalyzer);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SANDBOXED_ZIP_ANALYZER_H_
