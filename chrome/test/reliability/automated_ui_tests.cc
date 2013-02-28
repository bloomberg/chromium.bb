// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fstream>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/file_util.h"
#include "base/file_version_info.h"
#include "base/files/file_path.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/reliability/automated_ui_tests.h"
#include "chrome/test/ui/ui_test.h"
#include "googleurl/src/gurl.h"
#include "ui/base/keycodes/keyboard_codes.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/views/view.h"
#endif

namespace {

const char kReproSwitch[] = "key";

const char kReproRepeatSwitch[] = "num-reproductions";

const char kInputFilePathSwitch[] = "input";

const char kOutputFilePathSwitch[] = "output";

const char kDebugModeSwitch[] = "debug";

const char kWaitSwitch[] = "wait-after-action";

const char kTestLogFilePathSwitch[] = "testlog";

const base::FilePath::CharType* const kDefaultInputFilePath =
FILE_PATH_LITERAL("automated_ui_tests.txt");

const base::FilePath::CharType* const kDefaultOutputFilePath =
FILE_PATH_LITERAL("automated_ui_tests_error_report.txt");

const base::FilePath::CharType* const kDefaultTestLogFilePath =
FILE_PATH_LITERAL("automated_ui_tests_log.txt");

const int kDebuggingTimeoutMsec = 5000;

// How many commands to run when testing a dialog box.
const int kTestDialogActionsToRun = 7;

// String name of local chrome dll for looking up file information.
const wchar_t kChromeDll[] = L"chrome.dll";

void SilentRuntimeReportHandler(const std::string& str) {
}

base::FilePath GetInputFilePath() {
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  if (parsed_command_line.HasSwitch(kInputFilePathSwitch)) {
    return parsed_command_line.GetSwitchValuePath(kInputFilePathSwitch);
  } else {
    return base::FilePath(kDefaultInputFilePath);
  }
}

base::FilePath GetOutputFilePath() {
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  if (parsed_command_line.HasSwitch(kOutputFilePathSwitch)) {
    return parsed_command_line.GetSwitchValuePath(kOutputFilePathSwitch);
  } else {
    return base::FilePath(kDefaultOutputFilePath);
  }
}

base::FilePath GetTestLogFilePath() {
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  if (parsed_command_line.HasSwitch(kTestLogFilePathSwitch)) {
    return parsed_command_line.GetSwitchValuePath(kTestLogFilePathSwitch);
  } else {
    return base::FilePath(kDefaultTestLogFilePath);
  }
}

std::string GetChromeRevision() {
  // Get Chrome version
  std::string last_change;
#if defined(OS_WIN)
  // Check file version info for chrome dll.
  scoped_ptr<FileVersionInfo> file_info;
  file_info.reset(
      FileVersionInfo::CreateFileVersionInfo(base::FilePath(kChromeDll)));
  last_change = WideToASCII(file_info->last_change());
#elif defined(OS_POSIX)
  chrome::VersionInfo version_info;
  last_change = version_info.LastChange();
#endif // !defined(OS_WIN)
  return last_change;
}

void InitTestLog(base::Time start_time) {
  base::FilePath path = GetTestLogFilePath();
  std::ofstream test_log_file;
  if (!path.empty())
    test_log_file.open(path.value().c_str(), std::ios::out);

  const std::string time =
      UTF16ToASCII(base::TimeFormatFriendlyDateAndTime(start_time));

  test_log_file << "Last Change: " << GetChromeRevision() << std::endl;
  test_log_file << "Test Start: " << time << std::endl;
  test_log_file.close();
}

void AppendToTestLog(const std::string& append_string) {
  base::FilePath path = GetTestLogFilePath();
  std::ofstream test_log_file;
  if (!path.empty()) {
    test_log_file.open(path.value().c_str(),
        std::ios::out | std::ios_base::app);
  }

  test_log_file << append_string << std::endl;
  test_log_file.close();
}

double CalculateTestDuration(base::Time start_time) {
  base::Time time_now = base::Time::Now();
  return time_now.ToDoubleT() - start_time.ToDoubleT();
}

}  // namespace

// This subset of commands is used to test dialog boxes, which aren't likely
// to respond to most other commands.
const std::string kTestDialogPossibleActions[] = {
  // See FuzzyTestDialog for details on why Enter and SpaceBar must appear first
  // in this list.
  "PressEnterKey",
  "PressSpaceBar",
  "PressTabKey",
  "DownArrow"
};

// The list of dialogs that can be shown.
const std::string kDialogs[] = {
  "About",
  "Options",
  "TaskManager",
  "JavaScriptConsole",
  "ClearBrowsingData",
  "ImportSettings",
  "EditSearchEngines",
  "ViewPasswords"
};

AutomatedUITest::AutomatedUITest()
    : test_start_time_(base::Time::NowFromSystemTime()),
      total_crashes_(0),
      debug_logging_enabled_(false),
      post_action_delay_(0) {
  show_window_ = true;
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  if (parsed_command_line.HasSwitch(kDebugModeSwitch))
    debug_logging_enabled_ = true;
  if (parsed_command_line.HasSwitch(kWaitSwitch)) {
    std::string str = parsed_command_line.GetSwitchValueASCII(kWaitSwitch);
    if (str.empty()) {
      post_action_delay_ = 1;
    } else {
      base::StringToInt(str, &post_action_delay_);
    }
  }
  scoped_ptr<base::Environment> env(base::Environment::Create());
  if (env->HasVar(env_vars::kHeadless))
    logging::SetLogReportHandler(SilentRuntimeReportHandler);
}

AutomatedUITest::~AutomatedUITest() {}

void AutomatedUITest::RunReproduction() {
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();

  InitTestLog(test_start_time_);
  xml_writer_.StartWriting();
  xml_writer_.StartElement("Report");
  std::string action_string =
      parsed_command_line.GetSwitchValueASCII(kReproSwitch);

  int64 num_reproductions = 1;
  if (parsed_command_line.HasSwitch(kReproRepeatSwitch)) {
    base::StringToInt64(
        parsed_command_line.GetSwitchValueASCII(kReproRepeatSwitch),
        &num_reproductions);
  }
  std::vector<std::string> actions;
  base::SplitString(action_string, ',', &actions);
  bool did_crash = false;
  bool command_complete = false;

  for (int64 i = 0; i < num_reproductions && !did_crash; ++i) {
    bool did_teardown = false;
    xml_writer_.StartElement("Executed");
    for (size_t j = 0; j < actions.size(); ++j) {
      DoAction(actions[j]);
      if (DidCrash(true)) {
        did_crash = true;
        if (j >= (actions.size() - 1))
          command_complete = true;
        break;
      }
      if (LowerCaseEqualsASCII(actions[j], "teardown"))
        did_teardown = true;
    }

    // Force proper teardown after each run, if it didn't already happen. But
    // don't teardown after crashes.
    if (!did_teardown && !did_crash)
      DoAction("TearDown");

    xml_writer_.EndElement();  // End "Executed" element.
  }

  if (did_crash) {
    base::FilePath crash_dump = GetMostRecentCrashDump();
    base::FilePath::StringType result =
        FILE_PATH_LITERAL("*** Crash dump produced. ")
        FILE_PATH_LITERAL("See result file for more details. Dump = ");
    result.append(crash_dump.value());
    result.append(FILE_PATH_LITERAL(" ***\n"));
    printf("%s", result.c_str());
    LogCrashResult(crash_dump, command_complete);
    EXPECT_TRUE(false) << "Crash detected.";
  } else {
    printf("*** No crashes. See result file for more details. ***\n");
    LogSuccessResult();
  }

  AppendToTestLog(StringPrintf("total_duration_seconds=%f",
                  CalculateTestDuration(test_start_time_)));
  WriteReportToFile();
}


void AutomatedUITest::RunAutomatedUITest() {
  InitTestLog(test_start_time_);

  ASSERT_TRUE(InitXMLReader()) << "Error initializing XMLReader";
  xml_writer_.StartWriting();
  xml_writer_.StartElement("Report");

  while (init_reader_.Read()) {
    init_reader_.SkipToElement();
    std::string node_name = init_reader_.NodeName();
    if (LowerCaseEqualsASCII(node_name, "command")) {
      bool no_errors = true;
      xml_writer_.StartElement("Executed");
      std::string command_number;
      if (init_reader_.NodeAttribute("number", &command_number)) {
        xml_writer_.AddAttribute("command_number", command_number);
      }
      xml_writer_.StopIndenting();

      // Starts the browser, logging it as an action.
      DoAction("SetUp");

      // Record the depth of the root of the command subtree, then advance to
      // the first element in preparation for parsing.
      int start_depth = init_reader_.Depth();
      ASSERT_TRUE(init_reader_.Read()) << "Malformed XML file.";
      init_reader_.SkipToElement();

      // Check for a crash right after startup.
      if (DidCrash(true)) {
        LogCrashResult(GetMostRecentCrashDump(), false);
        // Try and start up again.
        CloseBrowserAndServer();
        LaunchBrowserAndServer();
        set_active_browser(automation()->GetBrowserWindow(0));
        if (DidCrash(true)) {
          no_errors = false;
          // We crashed again, so skip to the end of the this command.
          while (init_reader_.Depth() != start_depth) {
            ASSERT_TRUE(init_reader_.Read()) << "Malformed XML file.";
          }
        } else {
          // We didn't crash, so end the old element, logging a crash for that.
          // Then start a new element to log this command.
          xml_writer_.StartIndenting();
          xml_writer_.EndElement();
          xml_writer_.StartElement("Executed");
          xml_writer_.AddAttribute("command_number", command_number);
          xml_writer_.StopIndenting();
          xml_writer_.StartElement("SetUp");
          xml_writer_.EndElement();
        }
      }
      // Parse the command, performing the specified actions and checking
      // for a crash after each one.
      while (init_reader_.Depth() != start_depth) {
        node_name = init_reader_.NodeName();

        DoAction(node_name);

        // Advance to the next element
        ASSERT_TRUE(init_reader_.Read()) << "Malformed XML file.";
        init_reader_.SkipToElement();
        if (DidCrash(true)) {
          no_errors = false;
          // This was the last action if we've returned to the initial depth
          // of the command subtree.
          bool wasLastAction = init_reader_.Depth() == start_depth;
          LogCrashResult(GetMostRecentCrashDump(), wasLastAction);
          // Skip to the beginning of the next command.
          while (init_reader_.Depth() != start_depth) {
            ASSERT_TRUE(init_reader_.Read()) << "Malformed XML file.";
          }
        }
      }

      if (no_errors) {
        // If there were no previous crashes, log our tear down and check for
        // a crash, log success for the entire command if this doesn't crash.
        DoAction("TearDown");
        if (DidCrash(true))
          LogCrashResult(GetMostRecentCrashDump(), true);
        else
          LogSuccessResult();
      } else {
        // If there was a previous crash, just tear down without logging, so
        // that we know what the last command was before we crashed.
        CloseBrowserAndServer();
      }

      xml_writer_.StartIndenting();
      xml_writer_.EndElement();  // End "Executed" element.
    }
  }

  AppendToTestLog(StringPrintf("total_duration_seconds=%f",
                  CalculateTestDuration(test_start_time_)));

  // The test is finished so write our report.
  WriteReportToFile();
}

bool AutomatedUITest::DoAction(const std::string& action) {
  bool did_complete_action = false;
  xml_writer_.StartElement(action);
  if (debug_logging_enabled_)
    AppendToOutputFile(action);

  if (LowerCaseEqualsASCII(action, "about")) {
    did_complete_action = OpenAboutDialog();
  } else if (LowerCaseEqualsASCII(action, "back")) {
    did_complete_action = BackButton();
  } else if (LowerCaseEqualsASCII(action, "changeencoding")) {
    did_complete_action = ChangeEncoding();
  } else if (LowerCaseEqualsASCII(action, "closetab")) {
    did_complete_action = CloseActiveTab();
  } else if (LowerCaseEqualsASCII(action, "clearbrowsingdata")) {
    did_complete_action = OpenClearBrowsingDataDialog();
  } else if (LowerCaseEqualsASCII(action, "crash")) {
    did_complete_action = ForceCrash();
  } else if (LowerCaseEqualsASCII(action, "dialog")) {
    did_complete_action = ExerciseDialog();
  } else if (LowerCaseEqualsASCII(action, "downarrow")) {
    did_complete_action = PressDownArrow();
  } else if (LowerCaseEqualsASCII(action, "downloads")) {
    did_complete_action = ShowDownloads();
  } else if (LowerCaseEqualsASCII(action, "duplicatetab")) {
    did_complete_action = DuplicateTab();
  } else if (LowerCaseEqualsASCII(action, "editsearchengines")) {
    did_complete_action = OpenEditSearchEnginesDialog();
  } else if (LowerCaseEqualsASCII(action, "findinpage")) {
    did_complete_action = FindInPage();
  } else if (LowerCaseEqualsASCII(action, "forward")) {
    did_complete_action = ForwardButton();
  } else if (LowerCaseEqualsASCII(action, "goofftherecord")) {
    did_complete_action = GoOffTheRecord();
  } else if (LowerCaseEqualsASCII(action, "history")) {
    did_complete_action = ShowHistory();
  } else if (LowerCaseEqualsASCII(action, "home")) {
    did_complete_action = Home();
  } else if (LowerCaseEqualsASCII(action, "importsettings")) {
    did_complete_action = OpenImportSettingsDialog();
  } else if (LowerCaseEqualsASCII(action, "javascriptconsole")) {
    did_complete_action = JavaScriptConsole();
  } else if (LowerCaseEqualsASCII(action, "navigate")) {
    std::string url = chrome::kAboutBlankURL;
    if (init_reader_.NodeAttribute("url", &url)) {
      xml_writer_.AddAttribute("url", url);
    }
    GURL test_url(url);
    did_complete_action = Navigate(test_url);
  } else if (LowerCaseEqualsASCII(action, "newtab")) {
    did_complete_action = NewTab();
  } else if (LowerCaseEqualsASCII(action, "openwindow")) {
    did_complete_action = OpenAndActivateNewBrowserWindow(NULL);
  } else if (LowerCaseEqualsASCII(action, "options")) {
    did_complete_action = Options();
  } else if (LowerCaseEqualsASCII(action, "pagedown")) {
    did_complete_action = PressPageDown();
  } else if (LowerCaseEqualsASCII(action, "pageup")) {
    did_complete_action = PressPageUp();
  } else if (LowerCaseEqualsASCII(action, "pressenterkey")) {
    did_complete_action = PressEnterKey();
  } else if (LowerCaseEqualsASCII(action, "pressescapekey")) {
    did_complete_action = PressEscapeKey();
  } else if (LowerCaseEqualsASCII(action, "pressspacebar")) {
    did_complete_action = PressSpaceBar();
  } else if (LowerCaseEqualsASCII(action, "presstabkey")) {
    did_complete_action = PressTabKey();
  } else if (LowerCaseEqualsASCII(action, "reload")) {
    did_complete_action = ReloadPage();
  } else if (LowerCaseEqualsASCII(action, "restoretab")) {
    did_complete_action = RestoreTab();
  } else if (LowerCaseEqualsASCII(action, "selectnexttab")) {
    did_complete_action = SelectNextTab();
  } else if (LowerCaseEqualsASCII(action, "selectprevtab")) {
    did_complete_action = SelectPreviousTab();
  } else if (LowerCaseEqualsASCII(action, "setup")) {
    AutomatedUITestBase::SetUp();
    did_complete_action = true;
  } else if (LowerCaseEqualsASCII(action, "sleep")) {
    // This is for debugging, it probably shouldn't be used real tests.
    base::PlatformThread::Sleep(
        base::TimeDelta::FromMilliseconds(kDebuggingTimeoutMsec));
    did_complete_action = true;
  } else if (LowerCaseEqualsASCII(action, "star")) {
    did_complete_action = StarPage();
  } else if (LowerCaseEqualsASCII(action, "taskmanager")) {
    did_complete_action = OpenTaskManagerDialog();
  } else if (LowerCaseEqualsASCII(action, "teardown")) {
    CloseBrowserAndServer();
    did_complete_action = true;
  } else if (LowerCaseEqualsASCII(action, "testaboutchrome")) {
    did_complete_action = TestAboutChrome();
  } else if (LowerCaseEqualsASCII(action, "testclearbrowsingdata")) {
    did_complete_action = TestClearBrowsingData();
  } else if (LowerCaseEqualsASCII(action, "testeditsearchengines")) {
    did_complete_action = TestEditSearchEngines();
  } else if (LowerCaseEqualsASCII(action, "testimportsettings")) {
    did_complete_action = TestImportSettings();
  } else if (LowerCaseEqualsASCII(action, "testoptions")) {
    did_complete_action = TestOptions();
  } else if (LowerCaseEqualsASCII(action, "testtaskmanager")) {
    did_complete_action = TestTaskManager();
  } else if (LowerCaseEqualsASCII(action, "testviewpasswords")) {
    did_complete_action = TestViewPasswords();
  } else if (LowerCaseEqualsASCII(action, "uparrow")) {
    did_complete_action = PressUpArrow();
  } else if (LowerCaseEqualsASCII(action, "viewpasswords")) {
    did_complete_action = OpenViewPasswordsDialog();
  } else if (LowerCaseEqualsASCII(action, "viewsource")) {
    did_complete_action = ViewSource();
  } else if (LowerCaseEqualsASCII(action, "zoomplus")) {
    did_complete_action = ZoomPlus();
  } else if (LowerCaseEqualsASCII(action, "zoomminus")) {
    did_complete_action = ZoomMinus();
  } else {
    NOTREACHED() << "Unknown command passed into DoAction: "
                 << action.c_str();
  }

  EXPECT_TRUE(did_complete_action) << action;

  if (!did_complete_action)
    xml_writer_.AddAttribute("failed_to_complete", "yes");
  xml_writer_.EndElement();

  if (post_action_delay_) {
    base::PlatformThread::Sleep(
        base::TimeDelta::FromSeconds(post_action_delay_));
  }

  return did_complete_action;
}

bool AutomatedUITest::ChangeEncoding() {
  // Get the encoding list that is used to populate the UI (encoding menu)
  std::string cur_locale = g_browser_process->GetApplicationLocale();
  const std::vector<CharacterEncoding::EncodingInfo>* encodings =
      CharacterEncoding::GetCurrentDisplayEncodings(
          cur_locale, "ISO-8859-1,windows-1252", "");
  DCHECK(encodings);
  DCHECK(!encodings->empty());
  unsigned len = static_cast<unsigned>(encodings->size());

  // The vector will contain mostly IDC values for encoding commands plus a few
  // menu separators (0 values). If we hit a separator we just retry.
  int index = base::RandInt(0, len);
  while ((*encodings)[index].encoding_id == 0) {
    index = base::RandInt(0, len);
  }

  return RunCommandAsync((*encodings)[index].encoding_id);
}

bool AutomatedUITest::JavaScriptConsole() {
  return RunCommandAsync(IDC_DEV_TOOLS);
}

bool AutomatedUITest::OpenAboutDialog() {
  return RunCommandAsync(IDC_ABOUT);
}

bool AutomatedUITest::OpenClearBrowsingDataDialog() {
  return RunCommandAsync(IDC_CLEAR_BROWSING_DATA);
}

bool AutomatedUITest::OpenEditSearchEnginesDialog() {
  return RunCommandAsync(IDC_EDIT_SEARCH_ENGINES);
}

bool AutomatedUITest::OpenImportSettingsDialog() {
  return RunCommandAsync(IDC_IMPORT_SETTINGS);
}

bool AutomatedUITest::OpenTaskManagerDialog() {
  return RunCommandAsync(IDC_TASK_MANAGER);
}

bool AutomatedUITest::OpenViewPasswordsDialog() {
  return RunCommandAsync(IDC_VIEW_PASSWORDS);
}

bool AutomatedUITest::Options() {
  return RunCommandAsync(IDC_OPTIONS);
}

bool AutomatedUITest::PressDownArrow() {
  return SimulateKeyPress(ui::VKEY_DOWN);
}

bool AutomatedUITest::PressEnterKey() {
  return SimulateKeyPress(ui::VKEY_RETURN);
}

bool AutomatedUITest::PressEscapeKey() {
  return SimulateKeyPress(ui::VKEY_ESCAPE);
}

bool AutomatedUITest::PressPageDown() {
  return SimulateKeyPress(ui::VKEY_PRIOR);
}

bool AutomatedUITest::PressPageUp() {
  return SimulateKeyPress(ui::VKEY_NEXT);
}

bool AutomatedUITest::PressSpaceBar() {
  return SimulateKeyPress(ui::VKEY_SPACE);
}

bool AutomatedUITest::PressTabKey() {
  return SimulateKeyPress(ui::VKEY_TAB);
}

bool AutomatedUITest::PressUpArrow() {
  return SimulateKeyPress(ui::VKEY_UP);
}

bool AutomatedUITest::StarPage() {
  return RunCommandAsync(IDC_BOOKMARK_PAGE);
}

bool AutomatedUITest::ViewSource() {
  return RunCommandAsync(IDC_VIEW_SOURCE);
}

bool AutomatedUITest::ZoomMinus() {
  return RunCommandAsync(IDC_ZOOM_MINUS);
}

bool AutomatedUITest::ZoomPlus() {
  return RunCommandAsync(IDC_ZOOM_PLUS);
}

bool AutomatedUITest::TestAboutChrome() {
  DoAction("About");
  return FuzzyTestDialog(kTestDialogActionsToRun);
}

bool AutomatedUITest::TestClearBrowsingData() {
  DoAction("ClearBrowsingData");
  return FuzzyTestDialog(kTestDialogActionsToRun);
}

bool AutomatedUITest::TestEditSearchEngines() {
  DoAction("EditSearchEngines");
  return FuzzyTestDialog(kTestDialogActionsToRun);
}

bool AutomatedUITest::TestImportSettings() {
  DoAction("ImportSettings");
  return FuzzyTestDialog(kTestDialogActionsToRun);
}

bool AutomatedUITest::TestTaskManager() {
  DoAction("TaskManager");
  return FuzzyTestDialog(kTestDialogActionsToRun);
}

bool AutomatedUITest::TestOptions() {
  DoAction("Options");
  return FuzzyTestDialog(kTestDialogActionsToRun);
}

bool AutomatedUITest::TestViewPasswords() {
  DoAction("ViewPasswords");
  return FuzzyTestDialog(kTestDialogActionsToRun);
}

bool AutomatedUITest::ExerciseDialog() {
  int index = base::RandInt(0, arraysize(kDialogs) - 1);
  return DoAction(kDialogs[index]) && FuzzyTestDialog(kTestDialogActionsToRun);
}

bool AutomatedUITest::FuzzyTestDialog(int num_actions) {
  bool return_value = true;

  for (int i = 0; i < num_actions; i++) {
    // We want to make sure the first action performed on the dialog is not
    // Space or Enter because focus is likely on the Close button. Both Space
    // and Enter would close the dialog without performing more actions. We
    // rely on the fact that those two actions are first in the array and set
    // the lower bound to 2 if i == 0 to skip those two actions.
    int action_index = base::RandInt(i == 0 ? 2 : 0,
                                     arraysize(kTestDialogPossibleActions)
                                         - 1);
    return_value = return_value &&
                   DoAction(kTestDialogPossibleActions[action_index]);
    if (DidCrash(false))
      break;
  }
  return DoAction("PressEscapeKey") && return_value;
}

bool AutomatedUITest::ForceCrash() {
  scoped_refptr<TabProxy> tab(GetActiveTab());
  GURL test_url(chrome::kChromeUICrashURL);
  AutomationMsg_NavigationResponseValues result = tab->NavigateToURL(test_url);
  if (result != AUTOMATION_MSG_NAVIGATION_SUCCESS) {
    AddErrorAttribute("navigation_failed");
    return false;
  }
  return true;
}

bool AutomatedUITest::SimulateKeyPress(ui::KeyboardCode key) {
  scoped_refptr<TabProxy> tab(GetActiveTab());
  tab->SimulateKeyPress(key);
  return true;
}

bool AutomatedUITest::InitXMLReader() {
  base::FilePath input_path = GetInputFilePath();

  if (!file_util::ReadFileToString(input_path, &xml_init_file_))
    return false;
  return init_reader_.Load(xml_init_file_);
}

bool AutomatedUITest::WriteReportToFile() {
  base::FilePath path = GetOutputFilePath();
  std::ofstream error_file;
  if (!path.empty())
    error_file.open(path.value().c_str(), std::ios::out);

  // Closes all open elements and free the writer. This is required
  // in order to retrieve the contents of the buffer.
  xml_writer_.StopWriting();
  error_file << xml_writer_.GetWrittenString();
  error_file.close();
  return true;
}

void AutomatedUITest::AppendToOutputFile(const std::string& append_string) {
  base::FilePath path = GetOutputFilePath();
  std::ofstream error_file;
  if (!path.empty())
    error_file.open(path.value().c_str(), std::ios::out | std::ios_base::app);

  error_file << append_string << " ";
  error_file.close();
}

void AutomatedUITest::LogCrashResult(const base::FilePath& crash_dump,
                                     bool command_completed) {
  xml_writer_.StartElement("result");
  xml_writer_.AddAttribute("test_log_path",
      GetTestLogFilePath().MaybeAsASCII());
  xml_writer_.AddAttribute("revision", GetChromeRevision());
  xml_writer_.StartElement("crash");
#if defined(OS_WIN)
  xml_writer_.AddAttribute("crash_dump", WideToASCII(crash_dump.value()));
#else
  xml_writer_.AddAttribute("crash_dump", crash_dump.value());
#endif
  if (command_completed)
    xml_writer_.AddAttribute("command_completed", "yes");
  else
    xml_writer_.AddAttribute("command_completed", "no");
  xml_writer_.EndElement();
  xml_writer_.EndElement();
}

void AutomatedUITest::LogSuccessResult() {
  xml_writer_.StartElement("result");
  xml_writer_.AddAttribute("test_log_path",
      GetTestLogFilePath().MaybeAsASCII());
  xml_writer_.AddAttribute("revision", GetChromeRevision());
  xml_writer_.StartElement("success");
  xml_writer_.EndElement();
  xml_writer_.EndElement();
}

void AutomatedUITest::AddInfoAttribute(const std::string& info) {
  xml_writer_.AddAttribute("info", info);
}

void AutomatedUITest::AddWarningAttribute(const std::string& warning) {
  xml_writer_.AddAttribute("warning", warning);
}

void AutomatedUITest::AddErrorAttribute(const std::string& error) {
  xml_writer_.AddAttribute("error", error);
}

void AutomatedUITest::LogErrorMessage(const std::string& error) {
  AddErrorAttribute(error);
}

void AutomatedUITest::LogWarningMessage(const std::string& warning) {
  AddWarningAttribute(warning);
}

void AutomatedUITest::LogInfoMessage(const std::string& info) {
  AddWarningAttribute(info);
}

base::FilePath AutomatedUITest::GetMostRecentCrashDump() {
  base::FilePath crash_dump_path;
  base::FilePath most_recent_file_name;
  PathService::Get(chrome::DIR_CRASH_DUMPS, &crash_dump_path);
  base::Time most_recent_file_time;

  bool first_file = true;

  file_util::FileEnumerator enumerator(crash_dump_path,
                                       false,  // not recursive
                                       file_util::FileEnumerator::FILES);
  for (base::FilePath path = enumerator.Next(); !path.value().empty();
       path = enumerator.Next()) {
    base::PlatformFileInfo file_info;
    file_util::GetFileInfo(path, &file_info);
    if (first_file) {
      most_recent_file_time = file_info.last_modified;
      most_recent_file_name = path.BaseName();
      first_file = false;
    } else if (file_info.last_modified >= most_recent_file_time) {
      most_recent_file_time = file_info.last_modified;
      most_recent_file_name = path.BaseName();
    }
  }
  if (most_recent_file_name.empty()) {
    return base::FilePath();
  } else {
    crash_dump_path = crash_dump_path.Append(most_recent_file_name);
    return crash_dump_path;
  }
}

bool AutomatedUITest::DidCrash(bool update_total_crashes) {
  int actual_crashes = GetCrashCount();

  // If there are more crash dumps than the total dumps which we have recorded
  // then this is a new crash.
  if (actual_crashes > total_crashes_) {
    if (update_total_crashes)
      total_crashes_ = actual_crashes;
    return true;
  } else {
    return false;
  }
}

TEST_F(AutomatedUITest, TheOneAndOnlyTest) {
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  if (parsed_command_line.HasSwitch(kReproSwitch))
    RunReproduction();
  else
    RunAutomatedUITest();
}
