// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_stream_capture_indicator.h"

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_view_host_delegate.h"
#include "content/public/browser/web_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using content::BrowserThread;
using content::WebContents;

MediaStreamCaptureIndicator::TabEquals::TabEquals(int render_process_id,
                                                  int render_view_id)
    : render_process_id_(render_process_id),
      render_view_id_(render_view_id) {}

bool MediaStreamCaptureIndicator::TabEquals::operator() (
    const MediaStreamCaptureIndicator::CaptureDeviceTab& tab) {
    return (render_process_id_ == tab.render_process_id &&
            render_view_id_ == tab.render_view_id);
}

MediaStreamCaptureIndicator::MediaStreamCaptureIndicator()
    : status_icon_(NULL) {
}

MediaStreamCaptureIndicator::~MediaStreamCaptureIndicator() {
  // The user is responsible for cleaning up by closing all the opened devices.
  DCHECK(tabs_.empty());
}

bool MediaStreamCaptureIndicator::IsCommandIdChecked(
    int command_id) const {
  NOTIMPLEMENTED() << "There are no checked items in the MediaStream menu.";
  return false;
}

bool MediaStreamCaptureIndicator::IsCommandIdEnabled(
    int command_id) const {
  return command_id != IDC_MinimumLabelValue;
}

bool MediaStreamCaptureIndicator::GetAcceleratorForCommandId(
    int command_id, ui::Accelerator* accelerator) {
  // No accelerators for status icon context menu.
  return false;
}

void MediaStreamCaptureIndicator::ExecuteCommand(int command_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(command_id >= IDC_MEDIA_CONTEXT_MEDIA_STREAM_CAPTURE_LIST_FIRST &&
         command_id <= IDC_MEDIA_CONTEXT_MEDIA_STREAM_CAPTURE_LIST_LAST);
  int index = command_id - IDC_MEDIA_CONTEXT_MEDIA_STREAM_CAPTURE_LIST_FIRST;
  WebContents* tab_content = tab_util::GetWebContentsByID(
      tabs_[index].render_process_id, tabs_[index].render_view_id);
  DCHECK(tab_content);
  if (!tab_content || !tab_content->GetRenderViewHost() ||
      !tab_content->GetRenderViewHost()->GetDelegate()) {
    NOTREACHED();
    return;
  }

  tab_content->GetRenderViewHost()->GetDelegate()->Activate();
}

void MediaStreamCaptureIndicator::CaptureDevicesOpened(
    int render_process_id,
    int render_view_id,
    const content::MediaStreamDevices& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!devices.empty());

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&MediaStreamCaptureIndicator::DoDevicesOpenedOnUIThread,
                 this, render_process_id, render_view_id, devices));
}

void MediaStreamCaptureIndicator::CaptureDevicesClosed(
    int render_process_id,
    int render_view_id,
    const content::MediaStreamDevices& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!devices.empty());

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&MediaStreamCaptureIndicator::DoDevicesClosedOnUIThread,
                 this, render_process_id, render_view_id, devices));
}

void MediaStreamCaptureIndicator::DoDevicesOpenedOnUIThread(
    int render_process_id,
    int render_view_id,
    const content::MediaStreamDevices& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  CreateStatusTray();

  // If we don't have a status icon or one could not be created successfully,
  // then no need to continue.
  if (!status_icon_)
    return;

  AddCaptureDeviceTab(render_process_id, render_view_id, devices);
}

void MediaStreamCaptureIndicator::DoDevicesClosedOnUIThread(
    int render_process_id,
    int render_view_id,
    const content::MediaStreamDevices& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!status_icon_)
    return;

  DCHECK(!tabs_.empty());
  RemoveCaptureDeviceTab(render_process_id, render_view_id, devices);
}

void MediaStreamCaptureIndicator::CreateStatusTray() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (status_icon_)
    return;

  // If there is no browser process, we should not create the status tray.
  if (!g_browser_process)
    return;

  StatusTray* status_tray = g_browser_process->status_tray();
  if (!status_tray)
    return;

  status_icon_ = status_tray->CreateStatusIcon();

  EnsureStatusTrayIcon();
  DCHECK(!tray_image_.empty());
  DCHECK(!balloon_image_.empty());

  status_icon_->SetImage(tray_image_);
}

void MediaStreamCaptureIndicator::EnsureStatusTrayIcon() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (tray_image_.empty()) {
    tray_image_ = *ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_MEDIA_STREAM_CAPTURE_LED);
  }
  if (balloon_image_.empty()) {
    balloon_image_ = *ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_PRODUCT_LOGO_32);
  }
}

void MediaStreamCaptureIndicator::ShowBalloon(
    int render_process_id,
    int render_view_id,
    bool audio,
    bool video) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(audio || video);
  string16 title = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);

  int message_id = IDS_MEDIA_STREAM_STATUS_TRAY_BALLOON_BODY_AUDIO_AND_VIDEO;
  if (audio && !video)
    message_id = IDS_MEDIA_STREAM_STATUS_TRAY_BALLOON_BODY_AUDIO_ONLY;
  else if (!audio && video)
    message_id = IDS_MEDIA_STREAM_STATUS_TRAY_BALLOON_BODY_VIDEO_ONLY;

  string16 body = l10n_util::GetStringFUTF16(
      message_id, GetSecurityOrigin(render_process_id, render_view_id));

  status_icon_->DisplayBalloon(balloon_image_, title, body);
}

void MediaStreamCaptureIndicator::Hide() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!status_icon_)
    return;

  // If there is no browser process, we should not do anything.
  if (!g_browser_process)
    return;

  StatusTray* status_tray = g_browser_process->status_tray();
  if (status_tray != NULL) {
    status_tray->RemoveStatusIcon(status_icon_);
    status_icon_ = NULL;
  }
}

void MediaStreamCaptureIndicator::UpdateStatusTrayIconContextMenu() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<ui::SimpleMenuModel> menu(new ui::SimpleMenuModel(this));

  bool audio = false;
  bool video = false;
  int command_id = IDC_MEDIA_CONTEXT_MEDIA_STREAM_CAPTURE_LIST_FIRST;
  for (CaptureDeviceTabs::iterator iter = tabs_.begin();
       iter != tabs_.end();  ++iter) {
    string16 tab_title = GetTitle(iter->render_process_id,
                                  iter->render_view_id);
    // The tab has gone away.
    if (tab_title.empty())
      continue;

    // Check if any audio and video devices have been used.
    audio = audio || iter->audio_ref_count > 0;
    video = video || iter->video_ref_count > 0;

    string16 message = l10n_util::GetStringFUTF16(
        IDS_MEDIA_STREAM_STATUS_TRAY_MENU_ITEM, tab_title);
    menu->AddItem(command_id, message);

    // If reaching the maximum number, no more item will be added to the menu.
    if (command_id == IDC_MEDIA_CONTEXT_MEDIA_STREAM_CAPTURE_LIST_LAST)
      break;

    ++command_id;
  }

  // All the tabs have gone away.
  if (!audio && !video)
    return;

  // The icon will take the ownership of the passed context menu.
  status_icon_->SetContextMenu(menu.release());
  UpdateStatusTrayIconTooltip(audio, video);
}

void MediaStreamCaptureIndicator::UpdateStatusTrayIconTooltip(
    bool audio, bool video) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(audio || video);
  int message_id = IDS_MEDIA_STREAM_STATUS_TRAY_TEXT_AUDIO_AND_VIDEO;
  if (audio && !video)
    message_id = IDS_MEDIA_STREAM_STATUS_TRAY_TEXT_AUDIO_ONLY;
  else if (!audio && video)
    message_id = IDS_MEDIA_STREAM_STATUS_TRAY_TEXT_VIDEO_ONLY;

  status_icon_->SetToolTip(l10n_util::GetStringFUTF16(
      message_id, l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
}

void MediaStreamCaptureIndicator::AddCaptureDeviceTab(
    int render_process_id,
    int render_view_id,
    const content::MediaStreamDevices& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CaptureDeviceTabs::iterator iter = std::find_if(
      tabs_.begin(), tabs_.end(), TabEquals(render_process_id, render_view_id));
  if (iter == tabs_.end()) {
    tabs_.push_back(CaptureDeviceTab(render_process_id, render_view_id));
    iter = tabs_.end() - 1;
  }

  bool audio = false;
  bool video = false;
  content::MediaStreamDevices::const_iterator dev = devices.begin();
  for (; dev != devices.end(); ++dev) {
    DCHECK(dev->type == content::MEDIA_STREAM_DEVICE_TYPE_AUDIO_CAPTURE ||
           dev->type == content::MEDIA_STREAM_DEVICE_TYPE_VIDEO_CAPTURE);
    if (dev->type == content::MEDIA_STREAM_DEVICE_TYPE_AUDIO_CAPTURE) {
      ++iter->audio_ref_count;
      audio = true;
    } else {
      ++iter->video_ref_count;
      video = true;
    }
  }

  UpdateStatusTrayIconContextMenu();

  ShowBalloon(render_process_id, render_view_id, audio, video);
}

void MediaStreamCaptureIndicator::RemoveCaptureDeviceTab(
    int render_process_id,
    int render_view_id,
    const content::MediaStreamDevices& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  CaptureDeviceTabs::iterator iter = std::find_if(
      tabs_.begin(), tabs_.end(), TabEquals(render_process_id, render_view_id));
  DCHECK(iter != tabs_.end());

  content::MediaStreamDevices::const_iterator dev = devices.begin();
  for (; dev != devices.end(); ++dev) {
    DCHECK(dev->type == content::MEDIA_STREAM_DEVICE_TYPE_AUDIO_CAPTURE ||
           dev->type == content::MEDIA_STREAM_DEVICE_TYPE_VIDEO_CAPTURE);
    if (dev->type == content::MEDIA_STREAM_DEVICE_TYPE_AUDIO_CAPTURE)
      --iter->audio_ref_count;
    else
      --iter->video_ref_count;

    DCHECK_GE(iter->audio_ref_count, 0);
    DCHECK_GE(iter->video_ref_count, 0);
  }

  // Remove the tab if all the devices have been closed.
  if (iter->audio_ref_count == 0 && iter->video_ref_count == 0)
    tabs_.erase(iter);

  if (tabs_.empty())
    Hide();
  else
    UpdateStatusTrayIconContextMenu();
}

string16 MediaStreamCaptureIndicator::GetTitle(int render_process_id,
                                               int render_view_id) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  WebContents* tab_content = tab_util::GetWebContentsByID(
      render_process_id, render_view_id);
  if (!tab_content)
    return string16();

  string16 tab_title = tab_content->GetTitle();

  if (tab_title.empty()) {
    // If the page's title is empty use its security originator.
    tab_title = GetSecurityOrigin(render_process_id, render_view_id);
  } else {
    // If the page's title matches its URL, use its security originator.
    std::string languages =
        content::GetContentClient()->browser()->GetAcceptLangs(
            tab_content->GetBrowserContext());
    if (tab_title == net::FormatUrl(tab_content->GetURL(), languages))
      tab_title = GetSecurityOrigin(render_process_id, render_view_id);
  }

  return tab_title;
}

string16 MediaStreamCaptureIndicator::GetSecurityOrigin(
    int render_process_id, int render_view_id) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  WebContents* tab_content = tab_util::GetWebContentsByID(
      render_process_id, render_view_id);
  if (!tab_content)
    return string16();

  std::string security_origin = tab_content->GetURL().GetOrigin().spec();

  // Remove the last character if it is a '/'.
  if (!security_origin.empty()) {
    std::string::iterator it = security_origin.end() - 1;
    if (*it == '/')
      security_origin.erase(it);
  }

  return UTF8ToUTF16(security_origin);
}
