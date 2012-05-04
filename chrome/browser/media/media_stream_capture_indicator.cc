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
#include "content/public/browser/web_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using content::BrowserThread;
using content::WebContents;

MediaStreamCaptureIndicator::TabEquals::TabEquals(
    int render_process_id,
    int render_view_id,
    content::MediaStreamDeviceType type)
        : render_process_id_(render_process_id),
          render_view_id_(render_view_id),
          type_(type) {}

MediaStreamCaptureIndicator::TabEquals::TabEquals(int render_process_id,
                                                  int render_view_id)
    : render_process_id_(render_process_id),
      render_view_id_(render_view_id),
      type_(content::MEDIA_STREAM_DEVICE_TYPE_NO_SERVICE) {}

bool MediaStreamCaptureIndicator::TabEquals::operator() (
    const MediaStreamCaptureIndicator::CaptureDeviceTab& tab) {
  if (type_ == content::MEDIA_STREAM_DEVICE_TYPE_NO_SERVICE) {
    return (render_process_id_ == tab.render_process_id &&
            render_view_id_ == tab.render_view_id);
  } else {
    return (render_process_id_ == tab.render_process_id &&
            render_view_id_ == tab.render_view_id &&
            type_ == tab.type);
  }
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
  // TODO(xians) : Implement all the following execute command function.
  switch (command_id) {
    case IDC_MEDIA_CONTEXT_MEDIA_STREAM_CAPTURE_LIST_FIRST:
      break;
    default:
      NOTREACHED();
      break;
  }
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

  ShowBalloon(render_process_id, render_view_id, devices);
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

  if (tabs_.empty())
    Hide();
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

  status_icon_->SetToolTip(l10n_util::GetStringFUTF16(
      IDS_MEDIA_STREAM_STATUS_TRAY_TOOLTIP,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));

  EnsureStatusTrayIcon();
  DCHECK(!icon_image_.empty());

  status_icon_->SetImage(icon_image_);
}

void MediaStreamCaptureIndicator::EnsureStatusTrayIcon() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (icon_image_.empty()) {
    icon_image_ = *ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_MEDIA_STREAM_CAPTURE_LED);
  }
}

void MediaStreamCaptureIndicator::ShowBalloon(
    int render_process_id,
    int render_view_id,
    const content::MediaStreamDevices& devices) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  string16 title = l10n_util::GetStringFUTF16(
      IDS_MEDIA_STREAM_STATUS_TRAY_BALLOON_TITLE,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));

  int message_id = IDS_MEDIA_STREAM_STATUS_TRAY_BALLOON_BODY_AUDIO_AND_VIDEO;
  if (devices.size() == 1) {
    if (devices.front().type == content::MEDIA_STREAM_DEVICE_TYPE_AUDIO_CAPTURE)
      message_id = IDS_MEDIA_STREAM_STATUS_TRAY_BALLOON_BODY_AUDIO_ONLY;
    else
      message_id = IDS_MEDIA_STREAM_STATUS_TRAY_BALLOON_BODY_VIDEO_ONLY;
  }

  string16 message = l10n_util::GetStringFUTF16(
      message_id, GetTitle(render_process_id, render_view_id));

  status_icon_->DisplayBalloon(icon_image_, title, message);
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

  for (CaptureDeviceTabList::iterator iter = tabs_.begin();
       iter != tabs_.end();  ++iter) {
    // Search backward to see if the tab has been added.
    CaptureDeviceTabList::iterator iter_backward = std::find_if(
        tabs_.begin(), iter, TabEquals(iter->render_process_id,
                                       iter->render_view_id));
    // Do nothing if the tab has been added to the menu.
    if (iter_backward != iter)
      continue;

    string16 tab_title = GetTitle(iter->render_process_id,
                                  iter->render_view_id);
    // The tab has gone away.
    if (tab_title.empty())
      continue;

    string16 message = l10n_util::GetStringFUTF16(
        IDS_MEDIA_STREAM_STATUS_TRAY_MENU_ITEM, tab_title);
    menu->AddItem(IDC_MEDIA_CONTEXT_MEDIA_STREAM_CAPTURE_LIST_FIRST, message);
    menu->AddSeparator();
  }

  // The icon will take the ownership of the passed context menu.
  status_icon_->SetContextMenu(menu.release());
}

void MediaStreamCaptureIndicator::AddCaptureDeviceTab(
    int render_process_id,
    int render_view_id,
    const content::MediaStreamDevices& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  content::MediaStreamDevices::const_iterator dev = devices.begin();
  for (; dev != devices.end(); ++dev) {
    DCHECK(dev->type == content::MEDIA_STREAM_DEVICE_TYPE_AUDIO_CAPTURE ||
           dev->type == content::MEDIA_STREAM_DEVICE_TYPE_VIDEO_CAPTURE);
    tabs_.push_back(CaptureDeviceTab(render_process_id,
                                     render_view_id,
                                     dev->type));
  }

  UpdateStatusTrayIconContextMenu();
}

void MediaStreamCaptureIndicator::RemoveCaptureDeviceTab(
    int render_process_id,
    int render_view_id,
    const content::MediaStreamDevices& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  content::MediaStreamDevices::const_iterator dev = devices.begin();
  for (; dev != devices.end(); ++dev) {
    CaptureDeviceTabList::iterator iter = std::find_if(
        tabs_.begin(), tabs_.end(), TabEquals(render_process_id,
                                              render_view_id,
                                              dev->type));
    if (iter != tabs_.end()) {
      tabs_.erase(iter);
    } else {
      DLOG(ERROR) << "Failed to find MediaStream host "
                  << GetTitle(render_process_id, render_view_id)
                  << " for device " << dev->name
                  << " for type " << dev->type;
    }
  }

  if (!tabs_.empty())
    UpdateStatusTrayIconContextMenu();
}

string16 MediaStreamCaptureIndicator::GetTitle(int render_process_id,
                                               int render_view_id) const {
  WebContents* tab_content = tab_util::GetWebContentsByID(
      render_process_id, render_view_id);
  if (!tab_content)
    return string16();

  string16 tab_title = tab_content->GetTitle();
  if (tab_title.empty()) {
    GURL url = tab_content->GetURL();
    tab_title = UTF8ToUTF16(url.spec());
    // Force URL to be LTR.
    tab_title = base::i18n::GetDisplayStringInLTRDirectionality(tab_title);
  } else {
    // Sets the title explicitly as LTR format. Please look at the comments in
    // TaskManagerTabContentsResource::GetTitle() for the reasons of doing this.
    base::i18n::AdjustStringForLocaleDirection(&tab_title);
  }

  return tab_title;
}
