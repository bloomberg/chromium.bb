// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/hotword_background_activity_monitor.h"

#include "chrome/browser/ui/app_list/hotword_background_activity_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

#if defined(USE_ASH)
#include "ash/shell.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/idle_detector.h"
#endif

namespace app_list {

#if defined(OS_CHROMEOS)
namespace {
const int kIdleTimeoutInMin = 5;
}
#endif

HotwordBackgroundActivityMonitor::HotwordBackgroundActivityMonitor(
    HotwordBackgroundActivityDelegate* delegate)
    : delegate_(delegate),
      locked_(false),
      is_idle_(false) {
  BrowserList::AddObserver(this);
  BrowserList* browsers =
      BrowserList::GetInstance(chrome::GetActiveDesktop());
  for (BrowserList::const_iterator iter = browsers->begin();
       iter != browsers->end(); ++iter) {
    (*iter)->tab_strip_model()->AddObserver(this);
  }
  MediaCaptureDevicesDispatcher::GetInstance()->AddObserver(this);

#if defined(USE_ASH)
  if (ash::Shell::HasInstance())
    ash::Shell::GetInstance()->AddShellObserver(this);
#endif

#if defined(OS_CHROMEOS)
  idle_detector_.reset(new chromeos::IdleDetector(
      base::Bind(&HotwordBackgroundActivityMonitor::OnIdleStateChanged,
                 base::Unretained(this),
                 false),
      base::Bind(&HotwordBackgroundActivityMonitor::OnIdleStateChanged,
                 base::Unretained(this),
                 true)));
  idle_detector_->Start(base::TimeDelta::FromMinutes(kIdleTimeoutInMin));
#endif
}

HotwordBackgroundActivityMonitor::~HotwordBackgroundActivityMonitor() {
#if defined(USE_ASH)
  if (ash::Shell::HasInstance())
    ash::Shell::GetInstance()->RemoveShellObserver(this);
#endif

  BrowserList* browsers =
      BrowserList::GetInstance(chrome::GetActiveDesktop());
  for (BrowserList::const_iterator iter = browsers->begin();
       iter != browsers->end(); ++iter) {
    (*iter)->tab_strip_model()->RemoveObserver(this);
  }
  BrowserList::RemoveObserver(this);
}

bool HotwordBackgroundActivityMonitor::IsHotwordBackgroundActive() {
  return !is_idle_ && !locked_ && recording_renderer_ids_.empty();
}

void HotwordBackgroundActivityMonitor::OnBrowserAdded(Browser* browser) {
  browser->tab_strip_model()->AddObserver(this);
}

void HotwordBackgroundActivityMonitor::OnBrowserRemoved(Browser* browser) {
  browser->tab_strip_model()->RemoveObserver(this);
}

void HotwordBackgroundActivityMonitor::TabClosingAt(
    TabStripModel* tab_strip_model,
    content::WebContents* contents,
    int index) {
  std::set<int>::const_iterator iter =
      recording_renderer_ids_.find(contents->GetRenderProcessHost()->GetID());
  if (iter == recording_renderer_ids_.end())
    return;
  recording_renderer_ids_.erase(iter);
  NotifyActivityChange();
}

void HotwordBackgroundActivityMonitor::TabChangedAt(
    content::WebContents* contents,
    int index,
    TabStripModelObserver::TabChangeType change_type) {
  // Audio recording state change may emit TabChangedAt with ALL change_type.
  if (change_type != TabStripModelObserver::ALL)
    return;

  bool was_empty = recording_renderer_ids_.empty();

  bool recording = chrome::GetTabMediaStateForContents(contents) ==
      TAB_MEDIA_STATE_RECORDING;
  recording |= recording_contents_for_test_.find(contents) !=
      recording_contents_for_test_.end();
  if (recording) {
    recording_renderer_ids_.insert(contents->GetRenderProcessHost()->GetID());
  } else {
    recording_renderer_ids_.erase(contents->GetRenderProcessHost()->GetID());
  }
  if (was_empty != recording_renderer_ids_.empty())
    NotifyActivityChange();
}

#if defined(USE_ASH)
void HotwordBackgroundActivityMonitor::OnLockStateChanged(bool locked) {
  if (locked_ == locked)
    return;

  // TODO(mukai): consider non-ash environment. Right now this feature is
  // ChromeOS only, so ash always exists.
  locked_ = locked;
  NotifyActivityChange();
}
#endif

void HotwordBackgroundActivityMonitor::OnRequestUpdate(
    int render_process_id,
    int render_view_id,
    const content::MediaStreamDevice& device,
    const content::MediaRequestState state) {
  // Don't care the request from myself.
  if (render_process_id == delegate_->GetRenderProcessID())
    return;

  bool was_empty = recording_renderer_ids_.empty();
  if (state == content::MEDIA_REQUEST_STATE_REQUESTED ||
      state == content::MEDIA_REQUEST_STATE_PENDING_APPROVAL ||
      state == content::MEDIA_REQUEST_STATE_OPENING ||
      state == content::MEDIA_REQUEST_STATE_DONE) {
    recording_renderer_ids_.insert(render_process_id);
  } else {
    recording_renderer_ids_.erase(render_process_id);
  }
  if (was_empty != recording_renderer_ids_.empty())
    NotifyActivityChange();
}

void HotwordBackgroundActivityMonitor::OnIdleStateChanged(bool is_idle) {
  if (is_idle_ == is_idle)
    return;

  is_idle_ = is_idle;
  NotifyActivityChange();
}

void HotwordBackgroundActivityMonitor::NotifyActivityChange() {
  delegate_->OnHotwordBackgroundActivityChanged();
}

void HotwordBackgroundActivityMonitor::AddWebContentsToWhitelistForTest(
    content::WebContents* contents) {
  recording_contents_for_test_.insert(contents);
}

void HotwordBackgroundActivityMonitor::RemoveWebContentsFromWhitelistForTest(
    content::WebContents* contents) {
  recording_contents_for_test_.erase(contents);
}

}  // namespace app_list
