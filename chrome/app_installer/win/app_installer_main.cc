// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <initguid.h>
#include <urlmon.h>
#pragma comment(lib, "urlmon.lib")

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/logging_win.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/launcher_support/chrome_launcher_support.h"
#include "chrome/installer/util/google_update_util.h"
#include "chrome/installer/util/html_dialog.h"
#include "chrome/installer/util/util_constants.h"
#include "third_party/omaha/src/omaha/base/extractor.h"

namespace app_installer {

enum ExitCode {
  SUCCESS = 0,
  COULD_NOT_GET_FILE_PATH,
  COULD_NOT_READ_TAG,
  COULD_NOT_PARSE_TAG,
  INVALID_APP_ID,
  EULA_CANCELLED,
  COULD_NOT_FIND_CHROME,
  COULD_NOT_GET_TMP_FILE_PATH,
  FAILED_TO_DOWNLOAD_CHROME_SETUP,
  FAILED_TO_LAUNCH_CHROME_SETUP,
};

namespace {

// Log provider UUID. Required for logging to Sawbuck.
// {d82c3b59-bacd-4625-8282-4d570c4dad12}
DEFINE_GUID(kAppInstallerLogProvider,
            0xd82c3b59,
            0xbacd,
            0x4625,
            0x82, 0x82, 0x4d, 0x57, 0x0c, 0x4d, 0xad, 0x12);

const int kMaxTagLength = 4096;

const char kInstallChromeApp[] = "install-chrome-app";

const wchar_t kDownloadAndEulaPage[] =
    L"https://tools.google.com/dlpage/chromeappinstaller";

const wchar_t kSxSDownloadAndEulaPage[] =
    L"https://tools.google.com/dlpage/chromeappinstaller?sxs=true";

const wchar_t kDialogDimensions[] = L"dialogWidth:750px;dialogHeight:500px";

// This uses HTMLDialog to show a Chrome download page as a modal dialog.
// The page includes the EULA and returns a download URL.
class DownloadAndEulaHTMLDialog {
 public:
  explicit DownloadAndEulaHTMLDialog(bool is_canary) {
    dialog_.reset(installer::CreateNativeHTMLDialog(
        is_canary ? kSxSDownloadAndEulaPage : kDownloadAndEulaPage,
        base::string16()));
  }
  ~DownloadAndEulaHTMLDialog() {}

  // Shows the dialog and blocks for user input. Returns the string passed back
  // by the web page via |window.returnValue|.
  base::string16 ShowModal() {
    Customizer customizer;
    dialog_->ShowModal(NULL, &customizer);
    return dialog_->GetExtraResult();
  }

 private:
  class Customizer : public installer::HTMLDialog::CustomizationCallback {
   public:
    void OnBeforeCreation(wchar_t** extra) override {
      *extra = const_cast<wchar_t*>(kDialogDimensions);
    }

    void OnBeforeDisplay(void* window) override {
      // Customize the window by removing the close button and replacing the
      // existing 'e' icon with the standard informational icon.
      if (!window)
        return;
      HWND top_window = static_cast<HWND>(window);
      LONG_PTR style = GetWindowLongPtrW(top_window, GWL_STYLE);
      SetWindowLongPtrW(top_window, GWL_STYLE, style & ~WS_SYSMENU);
      HICON ico = LoadIcon(NULL, IDI_INFORMATION);
      SendMessageW(
          top_window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(ico));
    }
  };

  scoped_ptr<installer::HTMLDialog> dialog_;
  DISALLOW_COPY_AND_ASSIGN(DownloadAndEulaHTMLDialog);
};

// Gets the tag attached to a file by dl.google.com. This uses the same format
// as Omaha. Returns the empty string on failure.
std::string GetTag(const base::FilePath& file_name_path) {
  base::string16 file_name = file_name_path.value();
  omaha::TagExtractor extractor;
  if (!extractor.OpenFile(file_name.c_str()))
    return std::string();

  int tag_buffer_size = 0;
  if (!extractor.ExtractTag(NULL, &tag_buffer_size) || tag_buffer_size <= 1)
    return std::string();

  if (tag_buffer_size - 1 > kMaxTagLength) {
    LOG(ERROR) << "Tag length (" << tag_buffer_size - 1 << ") exceeds maximum ("
               << kMaxTagLength << ").";
    return std::string();
  }

  scoped_ptr<char[]> tag_buffer(new char[tag_buffer_size]);
  extractor.ExtractTag(tag_buffer.get(), &tag_buffer_size);

  return std::string(tag_buffer.get(), tag_buffer_size - 1);
}

bool IsStringPrintable(const std::string& s) {
  return std::find_if_not(s.begin(), s.end(), isprint) == s.end();
}

bool IsLegalDataKeyChar(int c) {
  return isalnum(c) || c == '-' || c == '_' || c == '$';
}

bool IsDataKeyValid(const std::string& key) {
  return std::find_if_not(key.begin(), key.end(), IsLegalDataKeyChar) ==
         key.end();
}

// Parses |tag| as key-value pairs and overwrites |parsed_pairs| with
// the result. |tag| should be a '&'-delimited list of '='-separated
// key-value pairs, e.g. "key1=value1&key2=value2".
// Returns true if the data could be parsed.
bool ParseTag(const std::string& tag,
              std::map<std::string, std::string>* parsed_pairs) {
  DCHECK(parsed_pairs);

  base::StringPairs kv_pairs;
  if (!base::SplitStringIntoKeyValuePairs(tag, '=', '&', &kv_pairs)) {
    LOG(ERROR) << "Failed to parse tag: " << tag;
    return false;
  }

  parsed_pairs->clear();
  for (const auto& pair : kv_pairs) {
    const std::string& key(pair.first);
    const std::string& value(pair.second);
    if (IsDataKeyValid(key) && IsStringPrintable(value)) {
      (*parsed_pairs)[key] = value;
    } else {
      LOG(ERROR) << "Illegal character found in tag.";
      return false;
    }
  }
  return true;
}

bool IsValidAppId(const std::string& app_id) {
  if (app_id.size() != 32)
    return false;

  for (size_t i = 0; i < app_id.size(); ++i) {
    char c = base::ToLowerASCII(app_id[i]);
    if (c < 'a' || c > 'p')
      return false;
  }

  return true;
}

base::FilePath GetChromeExePath(bool is_canary) {
  return is_canary ? chrome_launcher_support::GetAnyChromeSxSPath()
                   : chrome_launcher_support::GetAnyChromePath();
}

ExitCode GetChrome(bool is_canary) {
  // Show UI to install Chrome. The UI returns a download URL.
  base::string16 download_url =
      DownloadAndEulaHTMLDialog(is_canary).ShowModal();
  if (download_url.empty())
    return EULA_CANCELLED;

  DVLOG(1) << "Chrome download url: " << download_url;

  // Get a temporary file path.
  base::FilePath setup_file;
  if (!base::CreateTemporaryFile(&setup_file))
    return COULD_NOT_GET_TMP_FILE_PATH;

  // Download the Chrome installer.
  HRESULT hr = URLDownloadToFile(
      NULL, download_url.c_str(), setup_file.value().c_str(), 0, NULL);
  if (FAILED(hr)) {
    LOG(ERROR) << "Download failed: Error " << std::hex << hr << ". "
               << setup_file.value();
    return FAILED_TO_DOWNLOAD_CHROME_SETUP;
  }

  // Install Chrome. Wait for the installer to finish before returning.
  base::LaunchOptions options;
  options.wait = true;
  bool launch_success =
      base::LaunchProcess(base::CommandLine(setup_file), options, NULL);
  base::DeleteFile(setup_file, false);
  return launch_success ? SUCCESS : FAILED_TO_LAUNCH_CHROME_SETUP;
}

}  // namespace

extern "C"
int WINAPI wWinMain(HINSTANCE instance,
                    HINSTANCE prev_instance,
                    wchar_t* command_line,
                    int show_command) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(0, NULL);
  logging::LogEventProvider::Initialize(kAppInstallerLogProvider);
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  logging::InitLogging(settings);

  std::string app_id =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAppId);
  const char* sxs = installer::switches::kChromeSxS;
  // --chrome-sxs on the command line takes precedence over chrome-sxs in the
  // tag.
  bool is_canary = base::CommandLine::ForCurrentProcess()->HasSwitch(sxs);

  // --app-id on the command line inhibits tag parsing altogether.
  if (app_id.empty()) {
    base::FilePath current_exe;
    if (!PathService::Get(base::FILE_EXE, &current_exe))
      return COULD_NOT_GET_FILE_PATH;

    // Get the tag added by dl.google.com. Note that this is passed in via URL
    // parameters when requesting a file to download, so it must be validated
    // before use.
    std::string tag = GetTag(current_exe);
    if (tag.empty())
      return COULD_NOT_READ_TAG;

    DVLOG(1) << "Tag: " << tag;

    std::map<std::string, std::string> parsed_pairs;
    if (!ParseTag(tag, &parsed_pairs))
      return COULD_NOT_PARSE_TAG;

    auto result = parsed_pairs.find(switches::kAppId);
    if (result != parsed_pairs.end())
      app_id = result->second;

    if (!is_canary) {
      result = parsed_pairs.find(sxs);
      is_canary = result != parsed_pairs.end() && result->second == "1";
    }
  }

  if (!IsValidAppId(app_id))
    return INVALID_APP_ID;

  base::FilePath chrome_path = GetChromeExePath(is_canary);
  // If none found, show EULA, download, and install Chrome.
  if (chrome_path.empty()) {
    ExitCode get_chrome_result = GetChrome(is_canary);
    if (get_chrome_result != SUCCESS)
      return get_chrome_result;

    chrome_path = GetChromeExePath(is_canary);
    if (chrome_path.empty())
      return COULD_NOT_FIND_CHROME;
  }

  base::CommandLine cmd(chrome_path);
  cmd.AppendSwitchASCII(kInstallChromeApp, app_id);
  DVLOG(1) << "Install command: " << cmd.GetCommandLineString();
  bool launched = base::LaunchProcess(cmd, base::LaunchOptions(), NULL);
  DVLOG(1) << "Launch " << (launched ? "success." : "failed.");

  return SUCCESS;
}

}  // namespace app_installer
