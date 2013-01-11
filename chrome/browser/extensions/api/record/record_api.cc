// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/record/record_api.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/process_util.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"

#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/experimental_record.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"

namespace extensions {

namespace record = api::experimental_record;

ProcessStrategy::~ProcessStrategy() {}

void ProductionProcessStrategy::RunProcess(const CommandLine& line,
    std::vector<std::string>* errors) {
  base::LaunchOptions options;
  base::ProcessHandle handle;

  base::LaunchProcess(line, options, &handle);

  int exit_code = 0;

  if (!base::WaitForExitCode(handle, &exit_code) || exit_code != 0)
    errors->push_back("Test browser exited abnormally");
}

RunPageCyclerFunction::RunPageCyclerFunction(ProcessStrategy* strategy)
    : repeat_count_(1), base_command_line_(*CommandLine::ForCurrentProcess()),
    process_strategy_(strategy) {}

RunPageCyclerFunction::~RunPageCyclerFunction() {}

bool RunPageCyclerFunction::RunImpl() {
  if (!ParseJSParameters())
     return false;

  // If we've had any errors reportable to the JS caller so far (in
  // parameter parsing) then return a list of such errors, else perform
  // RunTestBrowser on the BlockingPool.
  if (!errors_.empty()) {
    results_ = record::CaptureURLs::Results::Create(errors_);
    SendResponse(true);
  } else {
    content::BrowserThread::PostBlockingPoolTask(FROM_HERE,
        base::Bind(&RunPageCyclerFunction::RunTestBrowser, this));
    process_strategy_->PumpBlockingPool();  // Test purposes only.
  }

  return true;
}

CommandLine RunPageCyclerFunction::RemoveSwitches(const CommandLine& original,
    const std::vector<std::string>& to_remove) {
  std::vector<const char*> to_keep;
  const CommandLine::SwitchMap& current_switches = original.GetSwitches();
  CommandLine filtered(original.GetProgram());

  // Retain in |to_keep| all current swtiches *not* in |to_remove|.
  for (CommandLine::SwitchMap::const_iterator itr = current_switches.begin();
      itr != current_switches.end(); ++itr) {
    if (std::find(to_remove.begin(), to_remove.end(), (*itr).first) ==
        to_remove.end()) {
      to_keep.push_back((*itr).first.c_str());
    }
  }

  // Rely on std::vector keeping its contents in contiguous order.
  // (This is documented STL spec.)
  filtered.CopySwitchesFrom(original, &to_keep.front(), to_keep.size());

  return filtered;
}

// Runs on BlockingPool thread.  Invoked from UI thread and passes back to
// UI thread for |Final()| callback to JS side.
void RunPageCyclerFunction::RunTestBrowser() {
  // Remove any current switch settings that would interfere with test browser
  // commandline setup.
  std::vector<std::string> remove_switches;
  remove_switches.push_back(switches::kUserDataDir);
  remove_switches.push_back(switches::kVisitURLs);
  remove_switches.push_back(switches::kPlaybackMode);
  remove_switches.push_back(switches::kRecordStats);
  remove_switches.push_back(switches::kLoadExtension);

  CommandLine line = RemoveSwitches(base_command_line_, remove_switches);

  // Add the user-data-dir switch, since this is common to both call types.
  line.AppendSwitchPath(switches::kUserDataDir, user_data_dir_);

  // Test browsers must run as if they are not in first-run mode
  line.AppendSwitch(switches::kNoFirstRun);

  // Create and fill a temp file to communicate the URL list to the test
  // browser.
  FilePath url_path;
  file_util::CreateTemporaryFile(&url_path);
  file_util::WriteFile(url_path, url_contents_.c_str(), url_contents_.size());
  line.AppendSwitchPath(switches::kVisitURLs, url_path);

  // Set up Capture- or Replay-specific commandline switches.
  AddSwitches(&line);

  FilePath error_file_path = url_path.DirName().
      Append(url_path.BaseName().value() +
      FilePath::StringType(kURLErrorsSuffix));

  LOG(ERROR) << "Test browser commandline: " << line.GetCommandLineString() <<
      " will be repeated " << repeat_count_ << " times....";

  // Run the test browser (or a mockup, depending on |process_strategy_|.
  while (repeat_count_-- && errors_.empty() &&
      !file_util::PathExists(error_file_path))
    process_strategy_->RunProcess(line, &errors_);

  // Read URL errors file if there is one, and save errors in |errors_|.
  // Odd extension handling needed because temp files have lots of "."s in
  // their names, and we need to cleanly add kURLErrorsSuffix as a final
  // extension.
  if (errors_.empty() && file_util::PathExists(error_file_path)) {
    std::string error_content;
    file_util::ReadFileToString(error_file_path, &error_content);

    base::SplitString(error_content, '\n', &errors_);
  }

  // Do any special post-test-browser file reading (e.g. stats report))
  // while we're on the BlockingPool thread.
  ReadReplyFiles();

  // Back to UI thread to finish up the JS call.
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&RunPageCyclerFunction::Finish, this));
}

const ProcessStrategy &RunPageCyclerFunction::GetProcessStrategy() {
  return *process_strategy_;
}

// RecordCaptureURLsFunction  ------------------------------------------------

RecordCaptureURLsFunction::RecordCaptureURLsFunction()
    : RunPageCyclerFunction(new ProductionProcessStrategy()) {}

RecordCaptureURLsFunction::RecordCaptureURLsFunction(ProcessStrategy* strategy)
    : RunPageCyclerFunction(strategy) {}

// Fetch data for possible optional switch for an extension to load.
bool RecordCaptureURLsFunction::ParseJSParameters() {
  scoped_ptr<record::CaptureURLs::Params> params(
      record::CaptureURLs::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  url_contents_ = JoinString(params->urls, '\n');
  // TODO(cstaley): Can't just use captureName -- gotta stick it in a temp dir.
  // TODO(cstaley): Ensure that capture name is suitable as directory name.
  user_data_dir_ = FilePath::FromUTF8Unsafe(params->capture_name);

  return true;
}

// RecordCaptureURLsFunction adds "record-mode" to sub-browser call, and returns
// just the (possibly empty) error list.
void RecordCaptureURLsFunction::AddSwitches(CommandLine* line) {
  if (!line->HasSwitch(switches::kRecordMode))
    line->AppendSwitch(switches::kRecordMode);
}

void RecordCaptureURLsFunction::Finish() {
  results_ = record::CaptureURLs::Results::Create(errors_);
  SendResponse(true);
}

// RecordReplayURLsFunction ------------------------------------------------

RecordReplayURLsFunction::RecordReplayURLsFunction()
    : RunPageCyclerFunction(new ProductionProcessStrategy()),
    run_time_ms_(0.0) {
}

RecordReplayURLsFunction::RecordReplayURLsFunction(ProcessStrategy* strategy)
    : RunPageCyclerFunction(strategy), run_time_ms_(0.0) {
}

RecordReplayURLsFunction::~RecordReplayURLsFunction() {}

// Fetch data for possible optional switches for a repeat count and an
// extension to load.
bool RecordReplayURLsFunction::ParseJSParameters() {
  scoped_ptr<record::ReplayURLs::Params> params(
      record::ReplayURLs::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());


  // TODO(cstaley): Must build full temp dir from capture_name
  user_data_dir_ = FilePath::FromUTF8Unsafe(params->capture_name);

  // TODO(cstaley): Get this from user data dir ultimately
  url_contents_ = "http://www.google.com\nhttp://www.amazon.com";

  repeat_count_ = params->repeat_count;

  if (params->details.get()) {
    if (params->details->extension_path.get())
      extension_path_ =
          FilePath::FromUTF8Unsafe(*params->details->extension_path);
  }

  return true;
}

// Add special switches, if indicated, for repeat count and extension to load,
// plus temp file into which to place stats. (Can't do this in
// ParseJSParameters because file creation can't go on the UI thread.)
// Plus, initialize time to create run time statistic.
void RecordReplayURLsFunction::AddSwitches(CommandLine* line) {
  file_util::CreateTemporaryFile(&stats_file_path_);

  if (!extension_path_.empty())
    line->AppendSwitchPath(switches::kLoadExtension, extension_path_);
  line->AppendSwitch(switches::kPlaybackMode);
  line->AppendSwitchPath(switches::kRecordStats, stats_file_path_);

  timer_ = base::Time::NowFromSystemTime();
}

// Read stats file, and get run time.
void RecordReplayURLsFunction::ReadReplyFiles() {
  file_util::ReadFileToString(stats_file_path_, &stats_);

  run_time_ms_ = (base::Time::NowFromSystemTime() - timer_).InMillisecondsF();
}

void RecordReplayURLsFunction::Finish() {
  record::ReplayURLsResult result;

  result.run_time = run_time_ms_;
  result.stats = stats_;
  result.errors = errors_;

  results_ = record::ReplayURLs::Results::Create(result);
  SendResponse(true);
}

} // namespace extensions
