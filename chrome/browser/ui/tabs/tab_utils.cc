// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_utils.h"

#include <utility>

#include "base/command_line.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/usb/usb_tab_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/animation/multi_animation.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/common_theme.h"
#include "ui/native_theme/native_theme.h"

#if defined(OS_MACOSX)
#include "chrome/grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#endif

struct LastMuteMetadata
    : public content::WebContentsUserData<LastMuteMetadata> {
  TabMutedReason reason = TabMutedReason::NONE;
  std::string extension_id;

 private:
  explicit LastMuteMetadata(content::WebContents* contents) {}
  friend class content::WebContentsUserData<LastMuteMetadata>;
};

DEFINE_WEB_CONTENTS_USER_DATA_KEY(LastMuteMetadata);

namespace chrome {

namespace {

// Interval between frame updates of the tab indicator animations.  This is not
// the usual 60 FPS because a trade-off must be made between tab UI animation
// smoothness and media recording/playback performance on low-end hardware.
const int kIndicatorFrameIntervalMs = 50;  // 20 FPS

// Fade-in/out duration for the tab indicator animations.  Fade-in is quick to
// immediately notify the user.  Fade-out is more gradual, so that the user has
// a chance of finding a tab that has quickly "blipped" on and off.
const int kIndicatorFadeInDurationMs = 200;
const int kIndicatorFadeOutDurationMs = 1000;

// Animation that throbs in (towards 1.0) and out (towards 0.0), and ends in the
// "in" state.
class TabRecordingIndicatorAnimation : public gfx::MultiAnimation {
 public:
  ~TabRecordingIndicatorAnimation() override {}

  // Overridden to provide alternating "towards in" and "towards out" behavior.
  double GetCurrentValue() const override;

  static std::unique_ptr<TabRecordingIndicatorAnimation> Create();

 private:
  TabRecordingIndicatorAnimation(const gfx::MultiAnimation::Parts& parts,
                                 const base::TimeDelta interval)
      : MultiAnimation(parts, interval) {}

  // Number of times to "toggle throb" the recording and tab capture indicators
  // when they first appear.
  static const int kCaptureIndicatorThrobCycles = 5;
};

double TabRecordingIndicatorAnimation::GetCurrentValue() const {
  return current_part_index() % 2 ?
      1.0 - MultiAnimation::GetCurrentValue() :
      MultiAnimation::GetCurrentValue();
}

std::unique_ptr<TabRecordingIndicatorAnimation>
TabRecordingIndicatorAnimation::Create() {
  MultiAnimation::Parts parts;
  static_assert(kCaptureIndicatorThrobCycles % 2 != 0,
        "odd number of cycles required so animation finishes in showing state");
  for (int i = 0; i < kCaptureIndicatorThrobCycles; ++i) {
    parts.push_back(MultiAnimation::Part(
        i % 2 ? kIndicatorFadeOutDurationMs : kIndicatorFadeInDurationMs,
        gfx::Tween::EASE_IN));
  }
  const base::TimeDelta interval =
      base::TimeDelta::FromMilliseconds(kIndicatorFrameIntervalMs);
  std::unique_ptr<TabRecordingIndicatorAnimation> animation(
      new TabRecordingIndicatorAnimation(parts, interval));
  animation->set_continuous(false);
  return animation;
}

}  // namespace

bool ShouldTabShowFavicon(int capacity,
                          bool is_pinned_tab,
                          bool is_active_tab,
                          bool has_favicon,
                          TabAlertState alert_state) {
  if (!has_favicon)
    return false;
  int required_capacity = 1;
  if (ShouldTabShowCloseButton(capacity, is_pinned_tab, is_active_tab))
    ++required_capacity;
  if (ShouldTabShowAlertIndicator(capacity, is_pinned_tab, is_active_tab,
                                  has_favicon, alert_state)) {
    ++required_capacity;
  }
  return capacity >= required_capacity;
}

bool ShouldTabShowAlertIndicator(int capacity,
                                 bool is_pinned_tab,
                                 bool is_active_tab,
                                 bool has_favicon,
                                 TabAlertState alert_state) {
  if (alert_state == TabAlertState::NONE)
    return false;
  if (ShouldTabShowCloseButton(capacity, is_pinned_tab, is_active_tab))
    return capacity >= 2;
  return capacity >= 1;
}

bool ShouldTabShowCloseButton(int capacity,
                              bool is_pinned_tab,
                              bool is_active_tab) {
  if (is_pinned_tab)
    return false;
  else if (is_active_tab)
    return true;
  else
    return capacity >= 3;
}

TabAlertState GetTabAlertStateForContents(content::WebContents* contents) {
  if (!contents)
    return TabAlertState::NONE;

  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()->
          GetMediaStreamCaptureIndicator();
  if (indicator.get()) {
    if (indicator->IsBeingMirrored(contents))
      return TabAlertState::TAB_CAPTURING;
    if (indicator->IsCapturingUserMedia(contents))
      return TabAlertState::MEDIA_RECORDING;
  }

  if (contents->IsConnectedToBluetoothDevice())
    return TabAlertState::BLUETOOTH_CONNECTED;

  UsbTabHelper* usb_tab_helper = UsbTabHelper::FromWebContents(contents);
  if (usb_tab_helper && usb_tab_helper->IsDeviceConnected())
    return TabAlertState::USB_CONNECTED;

  if (contents->IsAudioMuted())
    return TabAlertState::AUDIO_MUTING;
  if (contents->WasRecentlyAudible())
    return TabAlertState::AUDIO_PLAYING;

  return TabAlertState::NONE;
}

gfx::Image GetTabAlertIndicatorImage(TabAlertState alert_state,
                                     SkColor button_color) {
  const gfx::VectorIcon* icon = nullptr;
  switch (alert_state) {
    case TabAlertState::AUDIO_PLAYING:
      icon = &kTabAudioIcon;
      break;
    case TabAlertState::AUDIO_MUTING:
      icon = &kTabAudioMutingIcon;
      break;
    case TabAlertState::MEDIA_RECORDING:
      icon = &kTabMediaRecordingIcon;
      break;
    case TabAlertState::TAB_CAPTURING:
      icon = &kTabMediaCapturingIcon;
      break;
    case TabAlertState::BLUETOOTH_CONNECTED:
      icon = &kTabBluetoothConnectedIcon;
      break;
    case TabAlertState::USB_CONNECTED:
      icon = &kTabUsbConnectedIcon;
      break;
    case TabAlertState::NONE:
      return gfx::Image();
  }
  DCHECK(icon);
  return gfx::Image(gfx::CreateVectorIcon(*icon, 16, button_color));
}

gfx::Image GetTabAlertIndicatorAffordanceImage(TabAlertState alert_state,
                                               SkColor button_color) {
  switch (alert_state) {
    case TabAlertState::AUDIO_PLAYING:
      return GetTabAlertIndicatorImage(TabAlertState::AUDIO_MUTING,
                                       button_color);
    case TabAlertState::AUDIO_MUTING:
      return GetTabAlertIndicatorImage(TabAlertState::AUDIO_PLAYING,
                                       button_color);
    case TabAlertState::NONE:
    case TabAlertState::MEDIA_RECORDING:
    case TabAlertState::TAB_CAPTURING:
    case TabAlertState::BLUETOOTH_CONNECTED:
    case TabAlertState::USB_CONNECTED:
      return GetTabAlertIndicatorImage(alert_state, button_color);
  }
  NOTREACHED();
  return GetTabAlertIndicatorImage(alert_state, button_color);
}

std::unique_ptr<gfx::Animation> CreateTabAlertIndicatorFadeAnimation(
    TabAlertState alert_state) {
  if (alert_state == TabAlertState::MEDIA_RECORDING ||
      alert_state == TabAlertState::TAB_CAPTURING) {
    return TabRecordingIndicatorAnimation::Create();
  }

  // Note: While it seems silly to use a one-part MultiAnimation, it's the only
  // gfx::Animation implementation that lets us control the frame interval.
  gfx::MultiAnimation::Parts parts;
  const bool is_for_fade_in = (alert_state != TabAlertState::NONE);
  parts.push_back(gfx::MultiAnimation::Part(
      is_for_fade_in ? kIndicatorFadeInDurationMs : kIndicatorFadeOutDurationMs,
      gfx::Tween::EASE_IN));
  const base::TimeDelta interval =
      base::TimeDelta::FromMilliseconds(kIndicatorFrameIntervalMs);
  std::unique_ptr<gfx::MultiAnimation> animation(
      new gfx::MultiAnimation(parts, interval));
  animation->set_continuous(false);
  return std::move(animation);
}

base::string16 AssembleTabTooltipText(const base::string16& title,
                                      TabAlertState alert_state) {
  if (alert_state == TabAlertState::NONE)
    return title;

  base::string16 result = title;
  if (!result.empty())
    result.append(1, '\n');
  switch (alert_state) {
    case TabAlertState::AUDIO_PLAYING:
      result.append(
          l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_ALERT_STATE_AUDIO_PLAYING));
      break;
    case TabAlertState::AUDIO_MUTING:
      result.append(
          l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_ALERT_STATE_AUDIO_MUTING));
      break;
    case TabAlertState::MEDIA_RECORDING:
      result.append(l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_MEDIA_RECORDING));
      break;
    case TabAlertState::TAB_CAPTURING:
      result.append(
          l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_ALERT_STATE_TAB_CAPTURING));
      break;
    case TabAlertState::BLUETOOTH_CONNECTED:
      result.append(l10n_util::GetStringUTF16(
          IDS_TOOLTIP_TAB_ALERT_STATE_BLUETOOTH_CONNECTED));
      break;
    case TabAlertState::USB_CONNECTED:
      result.append(
          l10n_util::GetStringUTF16(IDS_TOOLTIP_TAB_ALERT_STATE_USB_CONNECTED));
      break;
    case TabAlertState::NONE:
      NOTREACHED();
      break;
  }
  return result;
}

bool AreExperimentalMuteControlsEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableTabAudioMuting);
}

bool CanToggleAudioMute(content::WebContents* contents) {
  switch (GetTabAlertStateForContents(contents)) {
    case TabAlertState::NONE:
    case TabAlertState::AUDIO_PLAYING:
    case TabAlertState::AUDIO_MUTING:
      return true;
    case TabAlertState::MEDIA_RECORDING:
    case TabAlertState::TAB_CAPTURING:
    case TabAlertState::BLUETOOTH_CONNECTED:
    case TabAlertState::USB_CONNECTED:
      return false;
  }
  NOTREACHED();
  return false;
}

TabMutedReason GetTabAudioMutedReason(content::WebContents* contents) {
  LastMuteMetadata::CreateForWebContents(contents);  // Ensures metadata exists.
  LastMuteMetadata* const metadata =
      LastMuteMetadata::FromWebContents(contents);
  if (GetTabAlertStateForContents(contents) == TabAlertState::TAB_CAPTURING) {
    // For tab capture, libcontent forces muting off.
    metadata->reason = TabMutedReason::MEDIA_CAPTURE;
    metadata->extension_id.clear();
  }
  return metadata->reason;
}

const std::string& GetExtensionIdForMutedTab(content::WebContents* contents) {
  DCHECK_EQ(GetTabAudioMutedReason(contents) != TabMutedReason::EXTENSION,
            LastMuteMetadata::FromWebContents(contents)->extension_id.empty());
  return LastMuteMetadata::FromWebContents(contents)->extension_id;
}

TabMutedResult SetTabAudioMuted(content::WebContents* contents,
                                bool mute,
                                TabMutedReason reason,
                                const std::string& extension_id) {
  DCHECK(contents);
  DCHECK(TabMutedReason::NONE != reason);

  if (reason == TabMutedReason::AUDIO_INDICATOR &&
      !AreExperimentalMuteControlsEnabled()) {
    return TabMutedResult::FAIL_NOT_ENABLED;
  }

  if (!chrome::CanToggleAudioMute(contents))
    return TabMutedResult::FAIL_TABCAPTURE;

  contents->SetAudioMuted(mute);

  LastMuteMetadata::CreateForWebContents(contents);  // Ensures metadata exists.
  LastMuteMetadata* const metadata =
      LastMuteMetadata::FromWebContents(contents);
  metadata->reason = reason;
  if (reason == TabMutedReason::EXTENSION) {
    DCHECK(!extension_id.empty());
    metadata->extension_id = extension_id;
  } else {
    metadata->extension_id.clear();
  }

  return TabMutedResult::SUCCESS;
}

void UnmuteIfMutedByExtension(content::WebContents* contents,
                              const std::string& extension_id) {
  LastMuteMetadata::CreateForWebContents(contents);  // Ensures metadata exists.
  LastMuteMetadata* const metadata =
      LastMuteMetadata::FromWebContents(contents);
  if (metadata->reason == TabMutedReason::EXTENSION &&
      metadata->extension_id == extension_id) {
    SetTabAudioMuted(contents, false, TabMutedReason::EXTENSION, extension_id);
  }
}

bool AreAllTabsMuted(const TabStripModel& tab_strip,
                     const std::vector<int>& indices) {
  for (std::vector<int>::const_iterator i = indices.begin(); i != indices.end();
       ++i) {
    if (!tab_strip.GetWebContentsAt(*i)->IsAudioMuted())
      return false;
  }
  return true;
}

}  // namespace chrome
