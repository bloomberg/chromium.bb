// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app_installer/win/app_installer_util.h"

#include <windows.h>
#include <urlmon.h>
#include <winhttp.h>
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "winhttp.lib")

#include "base/base_paths.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "base/win/scoped_handle.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/launcher_support/chrome_launcher_support.h"
#include "chrome/installer/util/google_update_util.h"
#include "chrome/installer/util/html_dialog.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/util_constants.h"
#include "net/base/escape.h"
#include "third_party/omaha/src/omaha/base/extractor.h"

namespace app_installer {

const char kInstallChromeApp[] = "install-chrome-app";

namespace {

// Copied from google_chrome_distribution.cc.
const char kBrowserAppGuid[] = "{8A69D345-D564-463c-AFF1-A69D9E530F96}";

// Copied frome google_chrome_sxs_distribution.cc.
const char kSxSBrowserAppGuid[] = "{4ea16ac7-fd5a-47c3-875b-dbf4a2008c20}";

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
  explicit DownloadAndEulaHTMLDialog(bool is_canary,
                                     const std::string& dialog_arguments) {
    dialog_.reset(installer::CreateNativeHTMLDialog(
        is_canary ? kSxSDownloadAndEulaPage : kDownloadAndEulaPage,
        base::SysUTF8ToWide(dialog_arguments)));
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

struct WinHttpHandleTraits {
  typedef HINTERNET Handle;
  static bool CloseHandle(Handle handle) {
    return WinHttpCloseHandle(handle) != FALSE;
  }
  static bool IsHandleValid(Handle handle) { return !!handle; }
  static Handle NullHandle() { return 0; }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(WinHttpHandleTraits);
};
typedef base::win::GenericScopedHandle<WinHttpHandleTraits,
                                       base::win::DummyVerifierTraits>
    ScopedWinHttpHandle;

// Reads the data portion of an HTTP response. WinHttpReceiveResponse() must
// have already succeeded on this request.
bool ReadHttpData(HINTERNET request_handle,
                  std::vector<uint8_t>* response_data) {
  BOOL result = TRUE;
  do {
    // Check for available data.
    DWORD bytes_available = 0;
    result = WinHttpQueryDataAvailable(request_handle, &bytes_available);
    if (!result) {
      PLOG(ERROR);
      break;
    }

    if (bytes_available == 0)
      break;

    do {
      // Allocate space for the buffer.
      size_t offset = response_data->size();
      response_data->resize(offset + bytes_available);

      // Read the data.
      DWORD bytes_read = 0;
      result = WinHttpReadData(request_handle, &((*response_data)[offset]),
                               bytes_available, &bytes_read);
      // MSDN for WinHttpQueryDataAvailable says:
      //   The amount of data that remains is not recalculated until all
      //   available data indicated by the call to WinHttpQueryDataAvailable is
      //   read.
      // So we should either read all of |bytes_available| or bail out here.
      if (!result) {
        PLOG(ERROR);
        response_data->resize(offset);
        return false;
      }

      // MSDN for WinHttpReadData says:
      //   If you are using WinHttpReadData synchronously, and the return value
      //   is TRUE and the number of bytes read is zero, the transfer has been
      //   completed and there are no more bytes to read on the handle.
      // Not sure if that's possible while |bytes_available| > 0, but better to
      // check and break out of both loops in this case.
      if (!bytes_read) {
        response_data->resize(offset);
        return true;
      }

      response_data->resize(offset + bytes_read);
      bytes_available -= bytes_read;
    } while (bytes_available);
  } while (true);

  return result != FALSE;
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

bool FetchUrl(const base::string16& user_agent,
              const base::string16& server_name,
              uint16_t server_port,
              const base::string16& object_name,
              std::vector<uint8_t>* response_data) {
  DCHECK(response_data);

  ScopedWinHttpHandle session_handle(
      WinHttpOpen(user_agent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
  if (!session_handle.IsValid()) {
    PLOG(ERROR);
    return false;
  }

  ScopedWinHttpHandle connection_handle(WinHttpConnect(
      session_handle.Get(), server_name.c_str(), server_port, 0));
  if (!connection_handle.IsValid()) {
    PLOG(ERROR);
    return false;
  }

  ScopedWinHttpHandle request_handle(WinHttpOpenRequest(
      connection_handle.Get(), L"GET", object_name.c_str(), NULL,
      WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
  if (!request_handle.IsValid()) {
    PLOG(ERROR);
    return false;
  }

  if (!WinHttpSendRequest(request_handle.Get(), WINHTTP_NO_ADDITIONAL_HEADERS,
                          0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
    PLOG(ERROR);
    return false;
  }

  if (!WinHttpReceiveResponse(request_handle.Get(), NULL)) {
    PLOG(ERROR);
    return false;
  }

  response_data->clear();
  return ReadHttpData(request_handle.Get(), response_data);
}

ExitCode GetChrome(bool is_canary, const std::string& inline_install_json) {
  // Show UI to install Chrome. The UI returns a download URL.
  base::string16 download_url =
      DownloadAndEulaHTMLDialog(is_canary, inline_install_json).ShowModal();
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

  // Construct the command line to pass to the installer so that it will not
  // launch Chrome upon completion.
  base::DictionaryValue installerdata_dict;
  base::DictionaryValue* distribution = new base::DictionaryValue();
  installerdata_dict.Set("distribution", distribution);
  distribution->SetBoolean(
      installer::master_preferences::kCreateAllShortcuts, false);
  distribution->SetBoolean(
      installer::master_preferences::kDoNotCreateDesktopShortcut, true);
  distribution->SetBoolean(
      installer::master_preferences::kDoNotCreateQuickLaunchShortcut, true);
  distribution->SetBoolean(
      installer::master_preferences::kDoNotCreateTaskbarShortcut, true);
  distribution->SetBoolean(
      installer::master_preferences::kDoNotLaunchChrome, true);
  distribution->SetBoolean(
      installer::master_preferences::kDoNotRegisterForUpdateLaunch, true);
  distribution->SetBoolean(
      installer::master_preferences::kDistroImportHistoryPref, false);
  distribution->SetBoolean(
      installer::master_preferences::kDistroImportSearchPref, false);
  distribution->SetBoolean(
      installer::master_preferences::kMakeChromeDefault, false);
  distribution->SetBoolean(
      installer::master_preferences::kSuppressFirstRunDefaultBrowserPrompt,
      true);
  std::string installerdata;
  JSONStringValueSerializer serializer(&installerdata);
  bool serialize_success = serializer.Serialize(installerdata_dict);
  DCHECK(serialize_success);
  std::string installerdata_url_encoded =
      net::EscapeQueryParamValue(installerdata, false);
  std::string appargs =
      base::StringPrintf("appguid=%s&installerdata=%s",
                         is_canary ? kSxSBrowserAppGuid : kBrowserAppGuid,
                         installerdata_url_encoded.c_str());
  base::CommandLine command_line(setup_file);
  command_line.AppendArg("/appargs");
  command_line.AppendArg(appargs);
  command_line.AppendArg("/install");  // Must be last.

  DVLOG(1) << "Chrome installer command line: "
           << command_line.GetCommandLineString();

  // Install Chrome. Wait for the installer to finish before returning.
  base::LaunchOptions options;
  options.wait = true;
  bool launch_success = base::LaunchProcess(command_line, options).IsValid();
  base::DeleteFile(setup_file, false);
  return launch_success ? SUCCESS : FAILED_TO_LAUNCH_CHROME_SETUP;
}

}  // namespace app_installer
