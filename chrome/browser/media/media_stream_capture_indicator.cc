// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_stream_capture_indicator.h"

#include <stddef.h>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#endif

using content::BrowserThread;
using content::WebContents;

namespace {

#if defined(ENABLE_EXTENSIONS)
const extensions::Extension* GetExtension(WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!web_contents)
    return NULL;

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(web_contents->GetBrowserContext());
  return registry->enabled_extensions().GetExtensionOrAppByURL(
      web_contents->GetURL());
}

bool IsWhitelistedExtension(const extensions::Extension* extension) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  static const char* const kExtensionWhitelist[] = {
    extension_misc::kHotwordNewExtensionId,
  };

  for (size_t i = 0; i < arraysize(kExtensionWhitelist); ++i) {
    if (extension->id() == kExtensionWhitelist[i])
      return true;
  }

  return false;
}
#endif  // defined(ENABLE_EXTENSIONS)

base::string16 GetTitle(WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!web_contents)
    return base::string16();

#if defined(ENABLE_EXTENSIONS)
  const extensions::Extension* const extension = GetExtension(web_contents);
  if (extension)
    return base::UTF8ToUTF16(extension->name());
#endif

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  std::string languages =
      profile->GetPrefs()->GetString(prefs::kAcceptLanguages);
  return url_formatter::FormatUrlForSecurityDisplay(web_contents->GetURL(),
                                                    languages);
}

}  // namespace

// Stores usage counts for all the capture devices associated with a single
// WebContents instance. Instances of this class are owned by
// MediaStreamCaptureIndicator. They also observe for the destruction of their
// corresponding WebContents and trigger their own deletion from their
// MediaStreamCaptureIndicator.
class MediaStreamCaptureIndicator::WebContentsDeviceUsage
    : public content::WebContentsObserver {
 public:
  WebContentsDeviceUsage(scoped_refptr<MediaStreamCaptureIndicator> indicator,
                         WebContents* web_contents)
      : WebContentsObserver(web_contents),
        indicator_(indicator),
        audio_ref_count_(0),
        video_ref_count_(0),
        mirroring_ref_count_(0),
        weak_factory_(this) {
  }

  bool IsCapturingAudio() const { return audio_ref_count_ > 0; }
  bool IsCapturingVideo() const { return video_ref_count_ > 0; }
  bool IsMirroring() const { return mirroring_ref_count_ > 0; }

  scoped_ptr<content::MediaStreamUI> RegisterMediaStream(
      const content::MediaStreamDevices& devices);

  // Increment ref-counts up based on the type of each device provided.
  void AddDevices(const content::MediaStreamDevices& devices);

  // Decrement ref-counts up based on the type of each device provided.
  void RemoveDevices(const content::MediaStreamDevices& devices);

 private:
  // content::WebContentsObserver overrides.
  void WebContentsDestroyed() override {
    indicator_->UnregisterWebContents(web_contents());
  }

  scoped_refptr<MediaStreamCaptureIndicator> indicator_;
  int audio_ref_count_;
  int video_ref_count_;
  int mirroring_ref_count_;

  base::WeakPtrFactory<WebContentsDeviceUsage> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsDeviceUsage);
};

// Implements MediaStreamUI interface. Instances of this class are created for
// each MediaStream and their ownership is passed to MediaStream implementation
// in the content layer. Each UIDelegate keeps a weak pointer to the
// corresponding WebContentsDeviceUsage object to deliver updates about state of
// the stream.
class MediaStreamCaptureIndicator::UIDelegate : public content::MediaStreamUI {
 public:
  UIDelegate(base::WeakPtr<WebContentsDeviceUsage> device_usage,
             const content::MediaStreamDevices& devices)
      : device_usage_(device_usage),
        devices_(devices),
        started_(false) {
    DCHECK(!devices_.empty());
  }

  ~UIDelegate() override {
    if (started_ && device_usage_.get())
      device_usage_->RemoveDevices(devices_);
  }

 private:
  // content::MediaStreamUI interface.
  gfx::NativeViewId OnStarted(const base::Closure& close_callback) override {
    DCHECK(!started_);
    started_ = true;
    if (device_usage_.get())
      device_usage_->AddDevices(devices_);
    return 0;
  }

  base::WeakPtr<WebContentsDeviceUsage> device_usage_;
  content::MediaStreamDevices devices_;
  bool started_;

  DISALLOW_COPY_AND_ASSIGN(UIDelegate);
};


scoped_ptr<content::MediaStreamUI>
MediaStreamCaptureIndicator::WebContentsDeviceUsage::RegisterMediaStream(
    const content::MediaStreamDevices& devices) {
  return make_scoped_ptr(new UIDelegate( weak_factory_.GetWeakPtr(), devices));
}

void MediaStreamCaptureIndicator::WebContentsDeviceUsage::AddDevices(
    const content::MediaStreamDevices& devices) {
  for (content::MediaStreamDevices::const_iterator it = devices.begin();
       it != devices.end(); ++it) {
    if (it->type == content::MEDIA_TAB_AUDIO_CAPTURE ||
        it->type == content::MEDIA_TAB_VIDEO_CAPTURE) {
      ++mirroring_ref_count_;
    } else if (content::IsAudioInputMediaType(it->type)) {
      ++audio_ref_count_;
    } else if (content::IsVideoMediaType(it->type)) {
      ++video_ref_count_;
    } else {
      NOTIMPLEMENTED();
    }
  }

  if (web_contents())
    web_contents()->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);

  indicator_->UpdateNotificationUserInterface();
}

void MediaStreamCaptureIndicator::WebContentsDeviceUsage::RemoveDevices(
    const content::MediaStreamDevices& devices) {
  for (content::MediaStreamDevices::const_iterator it = devices.begin();
       it != devices.end(); ++it) {
    if (it->type == content::MEDIA_TAB_AUDIO_CAPTURE ||
        it->type == content::MEDIA_TAB_VIDEO_CAPTURE) {
      --mirroring_ref_count_;
    } else if (content::IsAudioInputMediaType(it->type)) {
      --audio_ref_count_;
    } else if (content::IsVideoMediaType(it->type)) {
      --video_ref_count_;
    } else {
      NOTIMPLEMENTED();
    }
  }

  DCHECK_GE(audio_ref_count_, 0);
  DCHECK_GE(video_ref_count_, 0);
  DCHECK_GE(mirroring_ref_count_, 0);

  web_contents()->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);
  indicator_->UpdateNotificationUserInterface();
}

MediaStreamCaptureIndicator::MediaStreamCaptureIndicator()
    : status_icon_(NULL),
      mic_image_(NULL),
      camera_image_(NULL) {
}

MediaStreamCaptureIndicator::~MediaStreamCaptureIndicator() {
  // The user is responsible for cleaning up by reporting the closure of any
  // opened devices.  However, there exists a race condition at shutdown: The UI
  // thread may be stopped before CaptureDevicesClosed() posts the task to
  // invoke DoDevicesClosedOnUIThread().  In this case, usage_map_ won't be
  // empty like it should.
  DCHECK(usage_map_.empty() ||
         !BrowserThread::IsMessageLoopValid(BrowserThread::UI));
}

scoped_ptr<content::MediaStreamUI>
MediaStreamCaptureIndicator::RegisterMediaStream(
    content::WebContents* web_contents,
    const content::MediaStreamDevices& devices) {
  WebContentsDeviceUsage* usage = usage_map_.get(web_contents);
  if (!usage) {
    usage = new WebContentsDeviceUsage(this, web_contents);
    usage_map_.add(web_contents, make_scoped_ptr(usage));
  }
  return usage->RegisterMediaStream(devices);
}

void MediaStreamCaptureIndicator::ExecuteCommand(int command_id,
                                                 int event_flags) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const int index =
      command_id - IDC_MEDIA_CONTEXT_MEDIA_STREAM_CAPTURE_LIST_FIRST;
  DCHECK_LE(0, index);
  DCHECK_GT(static_cast<int>(command_targets_.size()), index);
  WebContents* web_contents = command_targets_[index];
  if (ContainsKey(usage_map_, web_contents))
    web_contents->GetDelegate()->ActivateContents(web_contents);
}

bool MediaStreamCaptureIndicator::IsCapturingUserMedia(
    content::WebContents* web_contents) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContentsDeviceUsage* usage = usage_map_.get(web_contents);
  return usage && (usage->IsCapturingAudio() || usage->IsCapturingVideo());
}

bool MediaStreamCaptureIndicator::IsCapturingVideo(
    content::WebContents* web_contents) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContentsDeviceUsage* usage = usage_map_.get(web_contents);
  return usage && usage->IsCapturingVideo();
}

bool MediaStreamCaptureIndicator::IsCapturingAudio(
    content::WebContents* web_contents) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContentsDeviceUsage* usage = usage_map_.get(web_contents);
  return usage && usage->IsCapturingAudio();
}

bool MediaStreamCaptureIndicator::IsBeingMirrored(
    content::WebContents* web_contents) const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContentsDeviceUsage* usage = usage_map_.get(web_contents);
  return usage && usage->IsMirroring();
}

void MediaStreamCaptureIndicator::UnregisterWebContents(
    WebContents* web_contents) {
  usage_map_.erase(web_contents);
  UpdateNotificationUserInterface();
}

void MediaStreamCaptureIndicator::MaybeCreateStatusTrayIcon(bool audio,
                                                            bool video) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (status_icon_)
    return;

  // If there is no browser process, we should not create the status tray.
  if (!g_browser_process)
    return;

  StatusTray* status_tray = g_browser_process->status_tray();
  if (!status_tray)
    return;

  EnsureStatusTrayIconResources();

  gfx::ImageSkia image;
  base::string16 tool_tip;
  GetStatusTrayIconInfo(audio, video, &image, &tool_tip);
  DCHECK(!image.isNull());
  DCHECK(!tool_tip.empty());

  status_icon_ = status_tray->CreateStatusIcon(
      StatusTray::MEDIA_STREAM_CAPTURE_ICON, image, tool_tip);
}

void MediaStreamCaptureIndicator::EnsureStatusTrayIconResources() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!mic_image_) {
    mic_image_ = ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_INFOBAR_MEDIA_STREAM_MIC);
  }
  if (!camera_image_) {
    camera_image_ = ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_INFOBAR_MEDIA_STREAM_CAMERA);
  }
  DCHECK(mic_image_);
  DCHECK(camera_image_);
}

void MediaStreamCaptureIndicator::MaybeDestroyStatusTrayIcon() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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

void MediaStreamCaptureIndicator::UpdateNotificationUserInterface() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  scoped_ptr<StatusIconMenuModel> menu(new StatusIconMenuModel(this));
  bool audio = false;
  bool video = false;
  int command_id = IDC_MEDIA_CONTEXT_MEDIA_STREAM_CAPTURE_LIST_FIRST;
  command_targets_.clear();

  for (const auto& it : usage_map_) {
    // Check if any audio and video devices have been used.
    const WebContentsDeviceUsage& usage = *it.second;
    if (!usage.IsCapturingAudio() && !usage.IsCapturingVideo())
      continue;

    WebContents* const web_contents = it.first;

    // The audio/video icon is shown only for non-whitelisted extensions or on
    // Android. For regular tabs on desktop, we show an indicator in the tab
    // icon.
#if defined(ENABLE_EXTENSIONS)
    const extensions::Extension* extension = GetExtension(web_contents);
    if (!extension || IsWhitelistedExtension(extension))
      continue;
#endif

    audio = audio || usage.IsCapturingAudio();
    video = video || usage.IsCapturingVideo();

    command_targets_.push_back(web_contents);
    menu->AddItem(command_id, GetTitle(web_contents));

    // If the menu item is not a label, enable it.
    menu->SetCommandIdEnabled(command_id, command_id != IDC_MinimumLabelValue);

    // If reaching the maximum number, no more item will be added to the menu.
    if (command_id == IDC_MEDIA_CONTEXT_MEDIA_STREAM_CAPTURE_LIST_LAST)
      break;
    ++command_id;
  }

  if (command_targets_.empty()) {
    MaybeDestroyStatusTrayIcon();
    return;
  }

  // The icon will take the ownership of the passed context menu.
  MaybeCreateStatusTrayIcon(audio, video);
  if (status_icon_) {
    status_icon_->SetContextMenu(std::move(menu));
  }
}

void MediaStreamCaptureIndicator::GetStatusTrayIconInfo(
    bool audio,
    bool video,
    gfx::ImageSkia* image,
    base::string16* tool_tip) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(audio || video);
  DCHECK(image);
  DCHECK(tool_tip);

  int message_id = 0;
  if (audio && video) {
    message_id = IDS_MEDIA_STREAM_STATUS_TRAY_TEXT_AUDIO_AND_VIDEO;
    *image = *camera_image_;
  } else if (audio && !video) {
    message_id = IDS_MEDIA_STREAM_STATUS_TRAY_TEXT_AUDIO_ONLY;
    *image = *mic_image_;
  } else if (!audio && video) {
    message_id = IDS_MEDIA_STREAM_STATUS_TRAY_TEXT_VIDEO_ONLY;
    *image = *camera_image_;
  }

  *tool_tip = l10n_util::GetStringUTF16(message_id);
}
