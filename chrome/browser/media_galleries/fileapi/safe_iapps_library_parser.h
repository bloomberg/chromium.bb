// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_SAFE_IAPPS_LIBRARY_PARSER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_SAFE_IAPPS_LIBRARY_PARSER_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/media_galleries/iphoto_library.h"
#include "chrome/common/media_galleries/itunes_library.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"

namespace IPC {
class Message;
}

namespace iapps {

// SafeIAppsLibraryParser parses the given iTunes library XML file safely via
// a utility process. The SafeIAppsLibraryParser object is ref-counted and
// kept alive after Start() is called until the ParserCallback is called.
// The ParserCallback is guaranteed to be called eventually either when the
// utility process replies or when it dies.
// Since iApps library XML files can be big, SafeIAppsLibraryParser passes
// the file handle to the utility process.
// SafeIAppsLibraryParser lives on the Media Task Runner unless otherwise
// noted.
class SafeIAppsLibraryParser : public content::UtilityProcessHostClient {
 public:
  typedef base::Callback<void(bool, const iphoto::parser::Library&)>
      IPhotoParserCallback;
  typedef base::Callback<void(bool, const itunes::parser::Library&)>
      ITunesParserCallback;

  SafeIAppsLibraryParser();

  // Start the parse of the iPhoto library file.
  void ParseIPhotoLibrary(const base::FilePath& library_file,
                          const IPhotoParserCallback& callback);

  // Start the parse of the iTunes library file.
  void ParseITunesLibrary(const base::FilePath& library_file,
                          const ITunesParserCallback& callback);


 private:
  enum ParserState {
    INITIAL_STATE,
    PINGED_UTILITY_PROCESS_STATE,
    STARTED_PARSING_STATE,
    FINISHED_PARSING_STATE,
  };

  // content::UtilityProcessHostClient is ref-counted.
  virtual ~SafeIAppsLibraryParser();

  // Posts a task to start the XML parsing in the utility process.
  void Start();

  // Launches the utility process.  Must run on the IO thread.
  void StartProcessOnIOThread();

  // Notification that the utility process is running, and we can now get its
  // process handle.
  // Runs on the IO thread.
  void OnUtilityProcessStarted();

  // Notification from the utility process when it finishes parsing the
  // iPhoto XML. Runs on the IO thread.
#if defined(OS_MACOSX)
  void OnGotIPhotoLibrary(bool result, const iphoto::parser::Library& library);
#endif

  // Notification from the utility process when it finishes parsing the
  // iTunes XML. Runs on the IO thread.
  void OnGotITunesLibrary(bool result, const itunes::parser::Library& library);

  // Sets |parser_state_| in case the library XML file cannot be opened.
  // Runs on the IO thread.
  void OnOpenLibraryFileFailed();

  // Communicates an error to the callback given to the constructor.
  void OnError();

  // UtilityProcessHostClient implementation.
  // Runs on the IO thread.
  virtual void OnProcessCrashed(int exit_code) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  base::FilePath library_file_path_;

  // Once we have opened the file, we store the handle so that we can use it
  // once the utility process has launched.
  base::File library_file_;

  // Only accessed on the IO thread.
  base::WeakPtr<content::UtilityProcessHost> utility_process_host_;

  // Only accessed on the Media Task Runner.
  ITunesParserCallback itunes_callback_;

  // Only accessed on the Media Task Runner.
  IPhotoParserCallback iphoto_callback_;

  // Verifies the messages from the utility process came at the right time.
  // Initialized on the Media Task Runner, but only accessed on the IO thread.
  ParserState parser_state_;

  DISALLOW_COPY_AND_ASSIGN(SafeIAppsLibraryParser);
};

}  // namespace iapps

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_SAFE_IAPPS_LIBRARY_PARSER_H_
