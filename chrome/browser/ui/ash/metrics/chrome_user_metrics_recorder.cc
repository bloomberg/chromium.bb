// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/metrics/chrome_user_metrics_recorder.h"

#include "ash/metrics/task_switch_metrics_recorder.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/shell.h"
#include "base/logging.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

ChromeUserMetricsRecorder::ChromeUserMetricsRecorder() {
  BrowserList::AddObserver(this);
}

ChromeUserMetricsRecorder::~ChromeUserMetricsRecorder() {
  DCHECK(BrowserList::GetInstance(chrome::HOST_DESKTOP_TYPE_ASH)->empty());
  BrowserList::RemoveObserver(this);
}

void ChromeUserMetricsRecorder::OnBrowserAdded(Browser* browser) {
  browser->tab_strip_model()->AddObserver(this);
}

void ChromeUserMetricsRecorder::OnBrowserRemoved(Browser* browser) {
  browser->tab_strip_model()->RemoveObserver(this);
}

void ChromeUserMetricsRecorder::ActiveTabChanged(
    content::WebContents* old_contents,
    content::WebContents* new_contents,
    int index,
    int reason) {
  if (reason & CHANGE_REASON_USER_GESTURE)
    OnTabSwitchedByUserGesture();
}

void ChromeUserMetricsRecorder::OnTabSwitchedByUserGesture() {
  ash::Shell::GetInstance()
      ->metrics()
      ->task_switch_metrics_recorder()
      .OnTaskSwitch(ash::TaskSwitchMetricsRecorder::kTabStrip);
}
