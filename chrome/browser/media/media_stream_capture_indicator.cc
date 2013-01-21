// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_stream_capture_indicator.h"

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/image_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/media_stream_request.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using content::BrowserThread;
using content::WebContents;

namespace {

const extensions::Extension* GetExtension(int render_process_id,
                                          int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  WebContents* web_contents = tab_util::GetWebContentsByID(
      render_process_id, render_view_id);
  if (!web_contents)
    return NULL;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile)
    return NULL;

  ExtensionService* extension_service = profile->GetExtensionService();
  if (!extension_service)
    return NULL;

  return extension_service->extensions()->GetExtensionOrAppByURL(
      ExtensionURLInfo(web_contents->GetURL()));
}

// Gets the security originator of the tab. It returns a string with no '/'
// at the end to display in the UI.
string16 GetSecurityOrigin(int render_process_id, int render_view_id) {
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

string16 GetTitle(int render_process_id, int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const extensions::Extension* extension =
      GetExtension(render_process_id, render_view_id);
  if (extension)
    return UTF8ToUTF16(extension->name());

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

}  // namespace

MediaStreamCaptureIndicator::TabEquals::TabEquals(WebContents* web_contents,
                                                  int render_process_id,
                                                  int render_view_id)
    : web_contents_(web_contents),
      render_process_id_(render_process_id),
      render_view_id_(render_view_id) {}

bool MediaStreamCaptureIndicator::TabEquals::operator() (
    const MediaStreamCaptureIndicator::CaptureDeviceTab& tab) {
  return (web_contents_ == tab.web_contents ||
          (render_process_id_ == tab.render_process_id &&
           render_view_id_ == tab.render_view_id));
}

MediaStreamCaptureIndicator::MediaStreamCaptureIndicator()
    : status_icon_(NULL),
      mic_image_(NULL),
      camera_image_(NULL),
      balloon_image_(NULL),
      should_show_balloon_(false) {
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
  WebContents* web_content = tab_util::GetWebContentsByID(
      tabs_[index].render_process_id, tabs_[index].render_view_id);
  DCHECK(web_content);
  if (!web_content) {
    NOTREACHED();
    return;
  }

  web_content->GetDelegate()->ActivateContents(web_content);
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

  AddCaptureDeviceTab(render_process_id, render_view_id, devices);
}

void MediaStreamCaptureIndicator::DoDevicesClosedOnUIThread(
    int render_process_id,
    int render_view_id,
    const content::MediaStreamDevices& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

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

  EnsureStatusTrayIconResources();
}

void MediaStreamCaptureIndicator::EnsureStatusTrayIconResources() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!mic_image_) {
    mic_image_ = ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_INFOBAR_MEDIA_STREAM_MIC);
  }
  if (!camera_image_) {
    camera_image_ = ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_INFOBAR_MEDIA_STREAM_CAMERA);
  }
  if (!balloon_image_) {
    balloon_image_ = ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_PRODUCT_LOGO_32);
  }
  DCHECK(mic_image_);
  DCHECK(camera_image_);
  DCHECK(balloon_image_);
}

void MediaStreamCaptureIndicator::ShowBalloon(
    int render_process_id,
    int render_view_id,
    bool audio,
    bool video) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(audio || video);

  int message_id = IDS_MEDIA_STREAM_STATUS_TRAY_BALLOON_BODY_AUDIO_AND_VIDEO;
  if (audio && !video)
    message_id = IDS_MEDIA_STREAM_STATUS_TRAY_BALLOON_BODY_AUDIO_ONLY;
  else if (!audio && video)
    message_id = IDS_MEDIA_STREAM_STATUS_TRAY_BALLOON_BODY_VIDEO_ONLY;

  // Only show the balloon for extensions.
  const extensions::Extension* extension =
      GetExtension(render_process_id, render_view_id);
  if (!extension) {
    DVLOG(1) << "Balloon is shown only for extensions";
    return;
  }

  string16 message =
      l10n_util::GetStringFUTF16(message_id,
                                 UTF8ToUTF16(extension->name()));

  WebContents* web_contents = tab_util::GetWebContentsByID(
        render_process_id, render_view_id);

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  should_show_balloon_ = true;
  extensions::ImageLoader::Get(profile)->LoadImageAsync(
      extension,
      extension->GetIconResource(32, ExtensionIconSet::MATCH_BIGGER),
      gfx::Size(32, 32),
      base::Bind(&MediaStreamCaptureIndicator::OnImageLoaded,
                 this, message));
}

void MediaStreamCaptureIndicator::OnImageLoaded(
    const string16& message,
    const gfx::Image& image) {
  if (!should_show_balloon_)
    return;

  const gfx::ImageSkia* image_skia = !image.IsEmpty() ? image.ToImageSkia() :
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          IDR_APP_DEFAULT_ICON);
  status_icon_->DisplayBalloon(*image_skia, string16(), message);
}

void MediaStreamCaptureIndicator::Hide() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Make sure images that finish loading don't cause a balloon to be shown.
  should_show_balloon_ = false;

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
       iter != tabs_.end();) {
    string16 tab_title = GetTitle(iter->render_process_id,
                                  iter->render_view_id);
    if (tab_title.empty()) {
      // Delete the entry since the tab has gone away.
      iter = tabs_.erase(iter);
      continue;
    }

    // Check if any audio and video devices have been used.
    audio = audio || iter->audio_ref_count > 0;
    video = video || iter->video_ref_count > 0;

    menu->AddItem(command_id, tab_title);

    // If reaching the maximum number, no more item will be added to the menu.
    if (command_id == IDC_MEDIA_CONTEXT_MEDIA_STREAM_CAPTURE_LIST_LAST)
      break;

    ++command_id;
    ++iter;
  }

  if (!audio && !video) {
    Hide();
    return;
  }

  // The icon will take the ownership of the passed context menu.
  status_icon_->SetContextMenu(menu.release());
  UpdateStatusTrayIconDisplay(audio, video);
}

void MediaStreamCaptureIndicator::UpdateStatusTrayIconDisplay(
    bool audio, bool video) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(audio || video);
  int message_id = 0;
  if (audio && video) {
    message_id = IDS_MEDIA_STREAM_STATUS_TRAY_TEXT_AUDIO_AND_VIDEO;
    status_icon_->SetImage(*camera_image_);
  } else if (audio && !video) {
    message_id = IDS_MEDIA_STREAM_STATUS_TRAY_TEXT_AUDIO_ONLY;
    status_icon_->SetImage(*mic_image_);
  } else if (!audio && video) {
    message_id = IDS_MEDIA_STREAM_STATUS_TRAY_TEXT_VIDEO_ONLY;
    status_icon_->SetImage(*camera_image_);
  }

  status_icon_->SetToolTip(l10n_util::GetStringFUTF16(
      message_id, l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
}

void MediaStreamCaptureIndicator::AddCaptureDeviceTab(
    int render_process_id,
    int render_view_id,
    const content::MediaStreamDevices& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  WebContents* web_contents = tab_util::GetWebContentsByID(render_process_id,
                                                           render_view_id);
  if (!web_contents)
    return;

  CaptureDeviceTabs::iterator iter = std::find_if(
      tabs_.begin(), tabs_.end(), TabEquals(web_contents,
                                            render_process_id,
                                            render_view_id));
  if (iter == tabs_.end()) {
    tabs_.push_back(CaptureDeviceTab(web_contents,
                                     render_process_id,
                                     render_view_id));
    iter = tabs_.end() - 1;
  }

  bool audio = false;
  bool video = false;
  bool tab_capture = false;
  content::MediaStreamDevices::const_iterator dev = devices.begin();
  for (; dev != devices.end(); ++dev) {
    if (dev->type == content::MEDIA_TAB_AUDIO_CAPTURE ||
        dev->type == content::MEDIA_TAB_VIDEO_CAPTURE) {
      ++iter->tab_capture_ref_count;
      tab_capture = true;
    } else if (content::IsAudioMediaType(dev->type)) {
      ++iter->audio_ref_count;
      audio = true;
    } else if (content::IsVideoMediaType(dev->type)) {
      ++iter->video_ref_count;
      video = true;
    } else {
      NOTIMPLEMENTED();
    }
  }

  DCHECK(web_contents);
  web_contents->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);

  // Don't use desktop notifications for tab capture. We use a favicon
  // glow notification instead.
  if (!status_icon_ || tab_capture)
    return;

  UpdateStatusTrayIconContextMenu();
  ShowBalloon(render_process_id, render_view_id, audio, video);
}

void MediaStreamCaptureIndicator::RemoveCaptureDeviceTab(
    int render_process_id,
    int render_view_id,
    const content::MediaStreamDevices& devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  WebContents* web_contents = tab_util::GetWebContentsByID(render_process_id,
                                                           render_view_id);
  CaptureDeviceTabs::iterator iter = std::find_if(
      tabs_.begin(), tabs_.end(), TabEquals(web_contents,
                                            render_process_id,
                                            render_view_id));

  if (iter != tabs_.end()) {
    content::MediaStreamDevices::const_iterator dev = devices.begin();
    for (; dev != devices.end(); ++dev) {
      if (dev->type == content::MEDIA_TAB_AUDIO_CAPTURE ||
          dev->type == content::MEDIA_TAB_VIDEO_CAPTURE) {
        --iter->tab_capture_ref_count;
      } else if (content::IsAudioMediaType(dev->type)) {
        --iter->audio_ref_count;
      } else if (content::IsVideoMediaType(dev->type)) {
        --iter->video_ref_count;
      } else {
        NOTIMPLEMENTED();
      }

      DCHECK_GE(iter->audio_ref_count, 0);
      DCHECK_GE(iter->video_ref_count, 0);
    }

    // Remove the tab if all the devices have been closed.
    if (iter->audio_ref_count == 0 && iter->video_ref_count == 0 &&
        iter->tab_capture_ref_count == 0)
      tabs_.erase(iter);
  }

  if (web_contents)
    web_contents->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);

  if (!status_icon_)
    return;

  UpdateStatusTrayIconContextMenu();
}

bool MediaStreamCaptureIndicator::IsProcessCapturing(int render_process_id,
                                                     int render_view_id) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  WebContents* web_contents = tab_util::GetWebContentsByID(render_process_id,
                                                           render_view_id);
  if (!web_contents)
    return false;

  CaptureDeviceTabs::const_iterator iter = std::find_if(
      tabs_.begin(), tabs_.end(), TabEquals(web_contents,
                                            render_process_id,
                                            render_view_id));
  if (iter == tabs_.end())
    return false;
  return (iter->audio_ref_count > 0 || iter->video_ref_count > 0);
}

bool MediaStreamCaptureIndicator::IsProcessCapturingTab(
    int render_process_id, int render_view_id) const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  WebContents* web_contents = tab_util::GetWebContentsByID(render_process_id,
                                                           render_view_id);
  if (!web_contents)
    return false;

  CaptureDeviceTabs::const_iterator iter = std::find_if(
      tabs_.begin(), tabs_.end(), TabEquals(web_contents,
                                            render_process_id,
                                            render_view_id));
  if (iter == tabs_.end())
    return false;
  return (iter->tab_capture_ref_count > 0);
}
