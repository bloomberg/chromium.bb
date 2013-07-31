// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_SAFE_ITUNES_LIBRARY_PARSER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_SAFE_ITUNES_LIBRARY_PARSER_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "chrome/common/media_galleries/itunes_library.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"

namespace IPC {
class Message;
}

namespace itunes {

// SafeITunesLibraryParser parses the given iTunes library XML file safely via
// a utility process. The SafeITunesLibraryParser object is ref-counted and
// kept alive after Start() is called until the ParserCallback is called.
// The ParserCallback is guaranteed to be called eventually either when the
// utility process replies or when it dies.
// Since iTunes library XML files can be big, SafeITunesLibraryParser passes
// the file handle to the utility process.
// SafeITunesLibraryParser lives on the Media Task Runner unless otherwise
// noted.
class SafeITunesLibraryParser : public content::UtilityProcessHostClient {
 public:
  typedef base::Callback<void(bool, const parser::Library&)> ParserCallback;

  SafeITunesLibraryParser(const base::FilePath& itunes_library_file,
                          const ParserCallback& callback);

  // Posts a task to start the XML parsing in the utility process.
  void Start();

 private:
  enum ParserState {
    INITIAL_STATE,
    PINGED_UTILITY_PROCESS_STATE,
    STARTED_PARSING_STATE,
    FINISHED_PARSING_STATE,
  };

  // content::UtilityProcessHostClient is ref-counted.
  virtual ~SafeITunesLibraryParser();

  // Launches the utility process.  Must run on the IO thread.
  void StartProcessOnIOThread();

  // Notification that the utility process is running, and we can now get its
  // process handle.
  // Runs on the IO thread.
  void OnUtilityProcessStarted();

  // Notification from the utility process when it finishes parsing the XML.
  // Runs on the IO thread.
  void OnGotITunesLibrary(bool result, const parser::Library& library);

  // Sets |parser_state_| in case the library XML file cannot be opened.
  // Runs on the IO thread.
  void OnOpenLibraryFileFailed();

  // UtilityProcessHostClient implementation.
  // Runs on the IO thread.
  virtual void OnProcessCrashed(int exit_code) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  const base::FilePath itunes_library_file_;

  // Once we have opened the file, we store the handle so that we can use it
  // once the utility process has launched.
  base::PlatformFile itunes_library_platform_file_;

  // Only accessed on the IO thread.
  base::WeakPtr<content::UtilityProcessHost> utility_process_host_;

  // Only accessed on the Media Task Runner.
  const ParserCallback callback_;

  // Verifies the messages from the utility process came at the right time.
  // Initialized on the Media Task Runner, but only accessed on the IO thread.
  ParserState parser_state_;

  DISALLOW_COPY_AND_ASSIGN(SafeITunesLibraryParser);
};

}  // namespace itunes

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_SAFE_ITUNES_LIBRARY_PARSER_H_
