// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app_installer/win/app_installer_util.h"

#include <windows.h>
#include <urlmon.h>
#pragma comment(lib, "urlmon.lib")

#include "base/base_paths.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
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

const char kInstallChromeApp[] = "install-chrome-app";

namespace {

const int kMaxTagLength = 4096;

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

}  // namespace

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

}  // namespace app_installer
