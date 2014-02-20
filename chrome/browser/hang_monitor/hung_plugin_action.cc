// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "chrome/browser/hang_monitor/hung_plugin_action.h"

#include "base/metrics/histogram.h"
#include "base/version.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/common/logging_chrome.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/webplugininfo.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/win/hwnd_util.h"

namespace {

const wchar_t kGTalkPluginName[] = L"Google Talk Plugin";
const int kGTalkPluginLogMinVersion = 26;  // For version 2.6 and below.

enum GTalkPluginLogVersion {
  GTALK_PLUGIN_VERSION_MIN = 0,
  GTALK_PLUGIN_VERSION_27,
  GTALK_PLUGIN_VERSION_28,
  GTALK_PLUGIN_VERSION_29,
  GTALK_PLUGIN_VERSION_30,
  GTALK_PLUGIN_VERSION_31,
  GTALK_PLUGIN_VERSION_32,
  GTALK_PLUGIN_VERSION_33,
  GTALK_PLUGIN_VERSION_34,
  GTALK_PLUGIN_VERSION_MAX
};

// Converts the version string of Google Talk Plugin to a version enum. The
// version format is "major(1 digit).minor(1 digit).sub(1 or 2 digits)",
// for example, "2.7.10" and "2.8.1". Converts the string to a number as
// 10 * major + minor - kGTalkPluginLogMinVersion.
GTalkPluginLogVersion GetGTalkPluginVersion(const base::string16& version) {
  int gtalk_plugin_version = GTALK_PLUGIN_VERSION_MIN;
  Version plugin_version;
  content::WebPluginInfo::CreateVersionFromString(version, &plugin_version);
  if (plugin_version.IsValid() && plugin_version.components().size() >= 2) {
    gtalk_plugin_version = 10 * plugin_version.components()[0] +
        plugin_version.components()[1] - kGTalkPluginLogMinVersion;
  }

  if (gtalk_plugin_version < GTALK_PLUGIN_VERSION_MIN)
    return GTALK_PLUGIN_VERSION_MIN;
  if (gtalk_plugin_version > GTALK_PLUGIN_VERSION_MAX)
    return GTALK_PLUGIN_VERSION_MAX;
  return static_cast<GTalkPluginLogVersion>(gtalk_plugin_version);
}

}  // namespace

HungPluginAction::HungPluginAction() : current_hung_plugin_window_(NULL) {
}

HungPluginAction::~HungPluginAction() {
}

bool HungPluginAction::OnHungWindowDetected(HWND hung_window,
                                            HWND top_level_window,
                                            ActionOnHungWindow* action) {
  if (NULL == action) {
    return false;
  }
  if (!IsWindow(hung_window)) {
    return false;
  }

  bool continue_hang_detection = true;

  DWORD hung_window_process_id = 0;
  DWORD top_level_window_process_id = 0;
  GetWindowThreadProcessId(hung_window, &hung_window_process_id);
  GetWindowThreadProcessId(top_level_window, &top_level_window_process_id);

  *action = HungWindowNotification::HUNG_WINDOW_IGNORE;
  if (top_level_window_process_id != hung_window_process_id) {
    base::string16 plugin_name;
    base::string16 plugin_version;
    GetPluginNameAndVersion(hung_window,
                            top_level_window_process_id,
                            &plugin_name,
                            &plugin_version);
    if (plugin_name.empty()) {
      plugin_name = l10n_util::GetStringUTF16(IDS_UNKNOWN_PLUGIN_NAME);
    } else if (kGTalkPluginName == plugin_name) {
      UMA_HISTOGRAM_ENUMERATION("GTalkPlugin.Hung",
          GetGTalkPluginVersion(plugin_version),
          GTALK_PLUGIN_VERSION_MAX + 1);
    }

    if (logging::DialogsAreSuppressed()) {
      NOTREACHED() << "Terminated a hung plugin process.";
      *action = HungWindowNotification::HUNG_WINDOW_TERMINATE_PROCESS;
    } else {
      const base::string16 title = l10n_util::GetStringUTF16(
          IDS_BROWSER_HANGMONITOR_TITLE);
      const base::string16 message = l10n_util::GetStringFUTF16(
          IDS_BROWSER_HANGMONITOR, plugin_name);
      // Before displaying the message box, invoke SendMessageCallback on the
      // hung window. If the callback ever hits, the window is not hung anymore
      // and we can dismiss the message box.
      SendMessageCallback(hung_window,
                          WM_NULL,
                          0,
                          0,
                          HungWindowResponseCallback,
                          reinterpret_cast<ULONG_PTR>(this));
      current_hung_plugin_window_ = hung_window;
      if (chrome::ShowMessageBox(
              NULL, title, message, chrome::MESSAGE_BOX_TYPE_QUESTION) ==
          chrome::MESSAGE_BOX_RESULT_YES) {
        *action = HungWindowNotification::HUNG_WINDOW_TERMINATE_PROCESS;
      } else {
        // If the user choses to ignore the hung window warning, the
        // message timeout for this window should be doubled. We only
        // double the timeout property on the window if the property
        // exists. The property is deleted if the window becomes
        // responsive.
        continue_hang_detection = false;
#pragma warning(disable:4311)
        int child_window_message_timeout =
            reinterpret_cast<int>(GetProp(
                hung_window, HungWindowDetector::kHungChildWindowTimeout));
#pragma warning(default:4311)
        if (child_window_message_timeout) {
          child_window_message_timeout *= 2;
#pragma warning(disable:4312)
          SetProp(hung_window, HungWindowDetector::kHungChildWindowTimeout,
                  reinterpret_cast<HANDLE>(child_window_message_timeout));
#pragma warning(default:4312)
        }
      }
      current_hung_plugin_window_ = NULL;
    }
  }
  if (HungWindowNotification::HUNG_WINDOW_TERMINATE_PROCESS == *action) {
    // Enable the top-level window just in case the plugin had been
    // displaying a modal box that had disabled the top-level window
    EnableWindow(top_level_window, TRUE);
  }
  return continue_hang_detection;
}

void HungPluginAction::OnWindowResponsive(HWND window) {
  if (window == current_hung_plugin_window_) {
    // The message timeout for this window should fallback to the default
    // timeout as this window is now responsive.
    RemoveProp(window, HungWindowDetector::kHungChildWindowTimeout);
    // The monitored plugin recovered. Let's dismiss the message box.
    EnumThreadWindows(GetCurrentThreadId(),
                      reinterpret_cast<WNDENUMPROC>(DismissMessageBox),
                      NULL);
  }
}

bool HungPluginAction::GetPluginNameAndVersion(HWND plugin_window,
                                               DWORD browser_process_id,
                                               base::string16* plugin_name,
                                               base::string16* plugin_version) {
  DCHECK(plugin_name);
  DCHECK(plugin_version);
  HWND window_to_check = plugin_window;
  while (NULL != window_to_check) {
    DWORD process_id = 0;
    GetWindowThreadProcessId(window_to_check, &process_id);
    if (process_id == browser_process_id) {
      // If we have reached a window the that belongs to the browser process
      // we have gone too far.
      return false;
    }
    if (content::PluginService::GetInstance()->GetPluginInfoFromWindow(
            window_to_check, plugin_name, plugin_version)) {
      return true;
    }
    window_to_check = GetParent(window_to_check);
  }
  return false;
}

// static
BOOL CALLBACK HungPluginAction::DismissMessageBox(HWND window, LPARAM ignore) {
  base::string16 class_name = gfx::GetClassName(window);
  // #32770 is the dialog window class which is the window class of
  // the message box being displayed.
  if (class_name == L"#32770") {
    EndDialog(window, IDNO);
    return FALSE;
  }
  return TRUE;
}

// static
void CALLBACK HungPluginAction::HungWindowResponseCallback(HWND target_window,
                                                           UINT message,
                                                           ULONG_PTR data,
                                                           LRESULT result) {
  HungPluginAction* instance = reinterpret_cast<HungPluginAction*>(data);
  DCHECK(NULL != instance);
  if (NULL != instance) {
    instance->OnWindowResponsive(target_window);
  }
}
