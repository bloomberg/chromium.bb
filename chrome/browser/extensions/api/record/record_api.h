// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_RECORD_RECORD_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_RECORD_RECORD_API_H_

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/time.h"
#include "chrome/browser/extensions/extension_function.h"

namespace {

const base::FilePath::CharType kURLErrorsSuffix[] =
    FILE_PATH_LITERAL(".errors");
const char kErrorsKey[] = "errors";
const char kStatsKey[] = "stats";

};

namespace extensions {

// ProcessStrategy abstracts the API's starting and waiting on a test
// browser instance.  This lets us browser-test the API without actually
// firing up a sub browser instance.
class ProcessStrategy {
 public:
  // Needed to void build warnings
  virtual ~ProcessStrategy();

  // Used only in test version to pump the blocking pool queue,
  // which doesn't otherwise happen during test.
  virtual void PumpBlockingPool() {}

  // Start up process with given commandline.  Real version does just
  // that; test version mocks it up, generating errors or good results,
  // as configured.  If there are any problems running the process itself,
  // as opposed to errors in the cycler URL list, etc, report these via
  // |errors|
  virtual void RunProcess(const CommandLine& line,
      std::vector<std::string> *errors) = 0;
};

// Production (default) version of ProcessStrategy.  See ProcessStrategy
// comments for more info.  This subclass actually starts a sub browser
// instance.
class ProductionProcessStrategy : public ProcessStrategy {
 public:
  virtual void RunProcess(const CommandLine& line,
      std::vector<std::string> *errors) OVERRIDE;
};

// Both page cycler calls (capture and replay) have a great deal in common,
// including the need to build and write a url list file, set the user
// data dir, start a sub-instance of Chrome, and parse a resultant error
// file.  This base class encapslates those common elements.
class RunPageCyclerFunction : public AsyncExtensionFunction {
 public:
  explicit RunPageCyclerFunction(ProcessStrategy* strategy);

  // Make a CommandLine copy of |original|, removing all switches in
  // |to_remove|.
  static CommandLine RemoveSwitches(const CommandLine& original,
      const std::vector<std::string>& to_remove);

  // Return ProcessStrategy, to allow for test versions.
  virtual const ProcessStrategy &GetProcessStrategy();

 protected:
  virtual ~RunPageCyclerFunction();

  // Gather common page cycler parameters and store them, then do blocking
  // thread invocation of RunTestBrowser.
  virtual bool RunImpl() OVERRIDE;

  // Parse the JS parameters, and store them as member data.
  virtual bool ParseJSParameters() = 0;

  // Do a vanilla test browser run, bracketing it immediately before and
  // after with a call of AddSwitches to add special commandline options
  // for Capture or Replay, and ReadReplyFiles to do any special post-run
  // data collection.  Gather any error results into |errors_| and then do a
  // BrowserThread call of Finish to return JS values.
  virtual void RunTestBrowser();
  virtual void AddSwitches(CommandLine* command_line) {}

  // The test browser communicates URL errors, performance statistics, and
  // possibly other data by posting them to text files.  ReadReplyFiles
  // collects these data for return to the JS side caller.
  virtual void ReadReplyFiles() {}

  // Return the values gathered in RunTestBrowser.  No common code here; all
  // logic is in subclasses.
  virtual void Finish() {}

  base::FilePath user_data_dir_;
  std::string url_contents_;
  int repeat_count_;
  std::vector<std::string> errors_;

  // Base CommandLine on which to build the test browser CommandLine
  CommandLine base_command_line_;

  // ProcessStrategy to use for this run.
  scoped_ptr<ProcessStrategy> process_strategy_;
};

class RecordCaptureURLsFunction : public RunPageCyclerFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("experimental.record.captureURLs",
                             EXPERIMENTAL_RECORD_CAPTUREURLS)

  RecordCaptureURLsFunction();
  explicit RecordCaptureURLsFunction(ProcessStrategy* strategy);

 private:
  virtual ~RecordCaptureURLsFunction() {}

  // Read the ReplayDetails parameter if it exists.
  virtual bool ParseJSParameters() OVERRIDE;

  // Add record-mode.
  virtual void AddSwitches(CommandLine* command_line) OVERRIDE;

  // Return error list.
  virtual void Finish() OVERRIDE;
};

class RecordReplayURLsFunction : public RunPageCyclerFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("experimental.record.replayURLs",
                             EXPERIMENTAL_RECORD_REPLAYURLS)

  RecordReplayURLsFunction();
  explicit RecordReplayURLsFunction(ProcessStrategy* strategy);

 private:
  virtual ~RecordReplayURLsFunction();

  // Read the ReplayDetails parameter if it exists.
  virtual bool ParseJSParameters() OVERRIDE;

  // Add visit-urls-count and load-extension.
  virtual void AddSwitches(CommandLine* command_line) OVERRIDE;

  // Read stats file.
  virtual void ReadReplyFiles() OVERRIDE;

  // Return error list, statistical results, and runtime.
  virtual void Finish() OVERRIDE;

  // These data are additional information added to the sub-browser
  // commandline or used to repeatedly run the sub-browser.
  base::FilePath extension_path_;
  base::FilePath stats_file_path_;

  // This time datum marks the start and end of the sub-browser run.
  base::Time timer_;

  // These two data are additional information returned to caller.
  double run_time_ms_;
  std::string stats_;
};

} // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_RECORD_RECORD_API_H_
