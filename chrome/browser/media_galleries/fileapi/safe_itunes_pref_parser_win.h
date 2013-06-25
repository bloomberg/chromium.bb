// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_SAFE_ITUNES_PREF_PARSER_WIN_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_SAFE_ITUNES_PREF_PARSER_WIN_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "content/public/browser/utility_process_host_client.h"

namespace base {
class FilePath;
}

namespace IPC {
class Message;
}

namespace itunes {

// SafeITunesPrefParserWin parses the given iTunes preferences XML data safely
// via a utility process. The SafeITunesPrefParserWin object is ref-counted and
// kept alive after Start() is called until the ParserCallback is called.
// The ParserCallback is guaranteed to be called eventually either when the
// utility process replies or when it dies.
class SafeITunesPrefParserWin : public content::UtilityProcessHostClient {
 public:
  typedef base::Callback<void(const base::FilePath&)> ParserCallback;

  SafeITunesPrefParserWin(const std::string& unsafe_xml,
                          const ParserCallback& callback);

  void Start();

 private:
  enum ParserState {
    INITIAL_STATE,
    STARTED_PARSING_STATE,
    FINISHED_PARSING_STATE,
  };

  // Private because content::UtilityProcessHostClient is ref-counted.
  virtual ~SafeITunesPrefParserWin();

  void StartWorkOnIOThread();

  // Handles the results from OnProcessCrashed() and OnMessageReceived() on
  // the IO thread.
  void OnGotITunesDirectory(const base::FilePath& library_file);

  // UtilityProcessHostClient implementation.
  virtual void OnProcessCrashed(int exit_code) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  const std::string unsafe_xml_;
  const ParserCallback callback_;

  // Verifies the messages from the utility process came at the right time.
  // Initialized on the FILE thread, but only accessed on the IO thread.
  ParserState parser_state_;

  DISALLOW_COPY_AND_ASSIGN(SafeITunesPrefParserWin);
};

}  // namespace itunes

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_SAFE_ITUNES_PREF_PARSER_WIN_H_
