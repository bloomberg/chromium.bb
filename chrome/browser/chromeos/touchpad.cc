// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/touchpad.h"

#include <stdlib.h>
#include <string>
#include <vector>

#include "base/string_util.h"
#include "base/process_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_member.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"

// Allows InvokeLater without adding refcounting.  The object is only deleted
// when its last InvokeLater is run anyway.
template<>
void RunnableMethodTraits<Touchpad>::RetainCallee(
    Touchpad* remover) {
}
template<>
void RunnableMethodTraits<Touchpad>::ReleaseCallee(
    Touchpad* remover) {
}

// static
void Touchpad::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kTapToClickEnabled, true);
  prefs->RegisterBooleanPref(prefs::kVertEdgeScrollEnabled, true);
}

void Touchpad::Init(PrefService* prefs) {
  tap_to_click_enabled_.Init(prefs::kTapToClickEnabled, prefs, this);
  vert_edge_scroll_enabled_.Init(prefs::kVertEdgeScrollEnabled, prefs, this);

  // Initialize touchpad settings to what's saved in user preferences.
  SetTapToClick();
  SetVertEdgeScroll();
}

void Touchpad::Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
  if (type == NotificationType::PREF_CHANGED)
    NotifyPrefChanged(Details<std::wstring>(details).ptr());
}

void Touchpad::NotifyPrefChanged(const std::wstring* pref_name) {
  if (!pref_name || *pref_name == prefs::kTapToClickEnabled)
    SetTapToClick();
  if (!pref_name || *pref_name == prefs::kVertEdgeScrollEnabled)
    SetVertEdgeScroll();
}

void Touchpad::SetSynclientParam(const std::string& param,
                                 const std::string& value) {
  // If not running on the file thread, then re-run on the file thread.
  if (!ChromeThread::CurrentlyOn(ChromeThread::FILE)) {
    base::Thread* file_thread = g_browser_process->file_thread();
    if (file_thread)
      file_thread->message_loop()->PostTask(FROM_HERE,
          NewRunnableMethod(this, &Touchpad::SetSynclientParam, param, value));
  } else {
    // launch binary synclient to set the parameter
    std::vector<std::string> argv;
    argv.push_back("/usr/bin/synclient");
    argv.push_back(param + "=" + value);
    base::file_handle_mapping_vector no_files;
    base::ProcessHandle handle;
    if (!base::LaunchApp(argv, no_files, true, &handle))
      LOG(ERROR) << "Failed to call /usr/bin/synclient";
  }
}

void Touchpad::SetTapToClick() {
  // To disable tap-to-click (i.e. a tap on the touchpad is recognized as a left
  // mouse click event), we set MaxTapTime to 0. MaxTapTime is the maximum time
  // (in milliseconds) for detecting a tap. The default is 180.
  if (tap_to_click_enabled_.GetValue())
    SetSynclientParam("MaxTapTime", "180");
  else
    SetSynclientParam("MaxTapTime", "0");
}

void Touchpad::SetVertEdgeScroll() {
  // To disable vertical edge scroll, we set VertEdgeScroll to 0. Vertical edge
  // scroll lets you use the right edge of the touchpad to control the movement
  // of the vertical scroll bar.
  if (vert_edge_scroll_enabled_.GetValue())
    SetSynclientParam("VertEdgeScroll", "1");
  else
    SetSynclientParam("VertEdgeScroll", "0");
}
