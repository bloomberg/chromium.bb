// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/blink_platform_impl.h"

#include <math.h>

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/metrics/user_metrics_action.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/sys_info.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "blink/public/resources/grit/blink_image_resources.h"
#include "blink/public/resources/grit/blink_resources.h"
#include "build/build_config.h"
#include "content/app/resources/grit/content_resources.h"
#include "content/app/strings/grit/content_strings.h"
#include "content/child/child_thread_impl.h"
#include "content/child/content_child_helpers.h"
#include "content/child/feature_policy/feature_policy_platform.h"
#include "content/child/notifications/notification_dispatcher.h"
#include "content/child/notifications/notification_manager.h"
#include "content/child/push_messaging/push_provider.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/web_data_consumer_handle_impl.h"
#include "content/child/web_url_loader_impl.h"
#include "content/child/web_url_request_util.h"
#include "content/child/worker_thread_registry.h"
#include "content/public/common/content_client.h"
#include "content/public/common/service_manager_connection.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebGestureCurve.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/scheduler/child/webthread_base.h"
#include "third_party/zlib/google/compression_utils.h"
#include "ui/base/layout.h"
#include "ui/events/gestures/blink/web_gesture_curve_impl.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

using blink::WebData;
using blink::WebFallbackThemeEngine;
using blink::WebLocalizedString;
using blink::WebString;
using blink::WebThemeEngine;
using blink::WebURL;
using blink::WebURLError;
using blink::WebURLLoader;

namespace content {

static int ToMessageID(WebLocalizedString::Name name) {
  switch (name) {
    case WebLocalizedString::kAXAMPMFieldText:
      return IDS_AX_AM_PM_FIELD_TEXT;
    case WebLocalizedString::kAXCalendarShowMonthSelector:
      return IDS_AX_CALENDAR_SHOW_MONTH_SELECTOR;
    case WebLocalizedString::kAXCalendarShowNextMonth:
      return IDS_AX_CALENDAR_SHOW_NEXT_MONTH;
    case WebLocalizedString::kAXCalendarShowPreviousMonth:
      return IDS_AX_CALENDAR_SHOW_PREVIOUS_MONTH;
    case WebLocalizedString::kAXCalendarWeekDescription:
      return IDS_AX_CALENDAR_WEEK_DESCRIPTION;
    case WebLocalizedString::kAXDayOfMonthFieldText:
      return IDS_AX_DAY_OF_MONTH_FIELD_TEXT;
    case WebLocalizedString::kAXHourFieldText:
      return IDS_AX_HOUR_FIELD_TEXT;
    case WebLocalizedString::kAXMediaDefault:
      return IDS_AX_MEDIA_DEFAULT;
    case WebLocalizedString::kAXMediaAudioElement:
      return IDS_AX_MEDIA_AUDIO_ELEMENT;
    case WebLocalizedString::kAXMediaVideoElement:
      return IDS_AX_MEDIA_VIDEO_ELEMENT;
    case WebLocalizedString::kAXMediaMuteButton:
      return IDS_AX_MEDIA_MUTE_BUTTON;
    case WebLocalizedString::kAXMediaUnMuteButton:
      return IDS_AX_MEDIA_UNMUTE_BUTTON;
    case WebLocalizedString::kAXMediaPlayButton:
      return IDS_AX_MEDIA_PLAY_BUTTON;
    case WebLocalizedString::kAXMediaPauseButton:
      return IDS_AX_MEDIA_PAUSE_BUTTON;
    case WebLocalizedString::kAXMediaCurrentTimeDisplay:
      return IDS_AX_MEDIA_CURRENT_TIME_DISPLAY;
    case WebLocalizedString::kAXMediaTimeRemainingDisplay:
      return IDS_AX_MEDIA_TIME_REMAINING_DISPLAY;
    case WebLocalizedString::kAXMediaEnterFullscreenButton:
      return IDS_AX_MEDIA_ENTER_FULL_SCREEN_BUTTON;
    case WebLocalizedString::kAXMediaExitFullscreenButton:
      return IDS_AX_MEDIA_EXIT_FULL_SCREEN_BUTTON;
    case WebLocalizedString::kAXMediaShowClosedCaptionsButton:
      return IDS_AX_MEDIA_SHOW_CLOSED_CAPTIONS_BUTTON;
    case WebLocalizedString::kAXMediaHideClosedCaptionsButton:
      return IDS_AX_MEDIA_HIDE_CLOSED_CAPTIONS_BUTTON;
    case WebLocalizedString::kAXMediaCastOffButton:
      return IDS_AX_MEDIA_CAST_OFF_BUTTON;
    case WebLocalizedString::kAXMediaCastOnButton:
      return IDS_AX_MEDIA_CAST_ON_BUTTON;
    case WebLocalizedString::kAXMediaDownloadButton:
      return IDS_AX_MEDIA_DOWNLOAD_BUTTON;
    case WebLocalizedString::kAXMediaOverflowButton:
      return IDS_AX_MEDIA_OVERFLOW_BUTTON;
    case WebLocalizedString::kAXMediaAudioElementHelp:
      return IDS_AX_MEDIA_AUDIO_ELEMENT_HELP;
    case WebLocalizedString::kAXMediaVideoElementHelp:
      return IDS_AX_MEDIA_VIDEO_ELEMENT_HELP;
    case WebLocalizedString::kAXMediaMuteButtonHelp:
      return IDS_AX_MEDIA_MUTE_BUTTON_HELP;
    case WebLocalizedString::kAXMediaUnMuteButtonHelp:
      return IDS_AX_MEDIA_UNMUTE_BUTTON_HELP;
    case WebLocalizedString::kAXMediaPlayButtonHelp:
      return IDS_AX_MEDIA_PLAY_BUTTON_HELP;
    case WebLocalizedString::kAXMediaPauseButtonHelp:
      return IDS_AX_MEDIA_PAUSE_BUTTON_HELP;
    case WebLocalizedString::kAXMediaAudioSliderHelp:
      return IDS_AX_MEDIA_AUDIO_SLIDER_HELP;
    case WebLocalizedString::kAXMediaVideoSliderHelp:
      return IDS_AX_MEDIA_VIDEO_SLIDER_HELP;
    case WebLocalizedString::kAXMediaCurrentTimeDisplayHelp:
      return IDS_AX_MEDIA_CURRENT_TIME_DISPLAY_HELP;
    case WebLocalizedString::kAXMediaTimeRemainingDisplayHelp:
      return IDS_AX_MEDIA_TIME_REMAINING_DISPLAY_HELP;
    case WebLocalizedString::kAXMediaEnterFullscreenButtonHelp:
      return IDS_AX_MEDIA_ENTER_FULL_SCREEN_BUTTON_HELP;
    case WebLocalizedString::kAXMediaExitFullscreenButtonHelp:
      return IDS_AX_MEDIA_EXIT_FULL_SCREEN_BUTTON_HELP;
    case WebLocalizedString::kAXMediaShowClosedCaptionsButtonHelp:
      return IDS_AX_MEDIA_SHOW_CLOSED_CAPTIONS_BUTTON_HELP;
    case WebLocalizedString::kAXMediaHideClosedCaptionsButtonHelp:
      return IDS_AX_MEDIA_HIDE_CLOSED_CAPTIONS_BUTTON_HELP;
    case WebLocalizedString::kAXMediaCastOffButtonHelp:
      return IDS_AX_MEDIA_CAST_OFF_BUTTON_HELP;
    case WebLocalizedString::kAXMediaCastOnButtonHelp:
      return IDS_AX_MEDIA_CAST_ON_BUTTON_HELP;
    case WebLocalizedString::kAXMediaOverflowButtonHelp:
      return IDS_AX_MEDIA_OVERFLOW_BUTTON_HELP;
    case WebLocalizedString::kAXMillisecondFieldText:
      return IDS_AX_MILLISECOND_FIELD_TEXT;
    case WebLocalizedString::kAXMinuteFieldText:
      return IDS_AX_MINUTE_FIELD_TEXT;
    case WebLocalizedString::kAXMonthFieldText:
      return IDS_AX_MONTH_FIELD_TEXT;
    case WebLocalizedString::kAXSecondFieldText:
      return IDS_AX_SECOND_FIELD_TEXT;
    case WebLocalizedString::kAXWeekOfYearFieldText:
      return IDS_AX_WEEK_OF_YEAR_FIELD_TEXT;
    case WebLocalizedString::kAXYearFieldText:
      return IDS_AX_YEAR_FIELD_TEXT;
    case WebLocalizedString::kCalendarClear:
      return IDS_FORM_CALENDAR_CLEAR;
    case WebLocalizedString::kCalendarToday:
      return IDS_FORM_CALENDAR_TODAY;
    case WebLocalizedString::kDetailsLabel:
      return IDS_DETAILS_WITHOUT_SUMMARY_LABEL;
    case WebLocalizedString::kDownloadButtonLabel:
      return IDS_DOWNLOAD_BUTTON_LABEL;
    case WebLocalizedString::kFileButtonChooseFileLabel:
      return IDS_FORM_FILE_BUTTON_LABEL;
    case WebLocalizedString::kFileButtonChooseMultipleFilesLabel:
      return IDS_FORM_MULTIPLE_FILES_BUTTON_LABEL;
    case WebLocalizedString::kFileButtonNoFileSelectedLabel:
      return IDS_FORM_FILE_NO_FILE_LABEL;
    case WebLocalizedString::kInputElementAltText:
      return IDS_FORM_INPUT_ALT;
    case WebLocalizedString::kMissingPluginText:
      return IDS_PLUGIN_INITIALIZATION_ERROR;
    case WebLocalizedString::kMediaRemotingDisableText:
      return IDS_MEDIA_REMOTING_DISABLE_TEXT;
    case WebLocalizedString::kMediaRemotingCastText:
      return IDS_MEDIA_REMOTING_CAST_TEXT;
    case WebLocalizedString::kMultipleFileUploadText:
      return IDS_FORM_FILE_MULTIPLE_UPLOAD;
    case WebLocalizedString::kOtherColorLabel:
      return IDS_FORM_OTHER_COLOR_LABEL;
    case WebLocalizedString::kOtherDateLabel:
      return IDS_FORM_OTHER_DATE_LABEL;
    case WebLocalizedString::kOtherMonthLabel:
      return IDS_FORM_OTHER_MONTH_LABEL;
    case WebLocalizedString::kOtherWeekLabel:
      return IDS_FORM_OTHER_WEEK_LABEL;
    case WebLocalizedString::kOverflowMenuCaptions:
      return IDS_MEDIA_OVERFLOW_MENU_CLOSED_CAPTIONS;
    case WebLocalizedString::kOverflowMenuCast:
      return IDS_MEDIA_OVERFLOW_MENU_CAST;
    case WebLocalizedString::kOverflowMenuEnterFullscreen:
      return IDS_MEDIA_OVERFLOW_MENU_ENTER_FULLSCREEN;
    case WebLocalizedString::kOverflowMenuExitFullscreen:
      return IDS_MEDIA_OVERFLOW_MENU_EXIT_FULLSCREEN;
    case WebLocalizedString::kOverflowMenuStopCast:
      return IDS_MEDIA_OVERFLOW_MENU_STOP_CAST;
    case WebLocalizedString::kOverflowMenuMute:
      return IDS_MEDIA_OVERFLOW_MENU_MUTE;
    case WebLocalizedString::kOverflowMenuUnmute:
      return IDS_MEDIA_OVERFLOW_MENU_UNMUTE;
    case WebLocalizedString::kOverflowMenuPlay:
      return IDS_MEDIA_OVERFLOW_MENU_PLAY;
    case WebLocalizedString::kOverflowMenuPause:
      return IDS_MEDIA_OVERFLOW_MENU_PAUSE;
    case WebLocalizedString::kOverflowMenuDownload:
      return IDS_MEDIA_OVERFLOW_MENU_DOWNLOAD;
    case WebLocalizedString::kPlaceholderForDayOfMonthField:
      return IDS_FORM_PLACEHOLDER_FOR_DAY_OF_MONTH_FIELD;
    case WebLocalizedString::kPlaceholderForMonthField:
      return IDS_FORM_PLACEHOLDER_FOR_MONTH_FIELD;
    case WebLocalizedString::kPlaceholderForYearField:
      return IDS_FORM_PLACEHOLDER_FOR_YEAR_FIELD;
    case WebLocalizedString::kResetButtonDefaultLabel:
      return IDS_FORM_RESET_LABEL;
    case WebLocalizedString::kSelectMenuListText:
      return IDS_FORM_SELECT_MENU_LIST_TEXT;
    case WebLocalizedString::kSubmitButtonDefaultLabel:
      return IDS_FORM_SUBMIT_LABEL;
    case WebLocalizedString::kThisMonthButtonLabel:
      return IDS_FORM_THIS_MONTH_LABEL;
    case WebLocalizedString::kThisWeekButtonLabel:
      return IDS_FORM_THIS_WEEK_LABEL;
    case WebLocalizedString::kValidationBadInputForDateTime:
      return IDS_FORM_VALIDATION_BAD_INPUT_DATETIME;
    case WebLocalizedString::kValidationBadInputForNumber:
      return IDS_FORM_VALIDATION_BAD_INPUT_NUMBER;
    case WebLocalizedString::kValidationPatternMismatch:
      return IDS_FORM_VALIDATION_PATTERN_MISMATCH;
    case WebLocalizedString::kValidationRangeOverflow:
      return IDS_FORM_VALIDATION_RANGE_OVERFLOW;
    case WebLocalizedString::kValidationRangeOverflowDateTime:
      return IDS_FORM_VALIDATION_RANGE_OVERFLOW_DATETIME;
    case WebLocalizedString::kValidationRangeUnderflow:
      return IDS_FORM_VALIDATION_RANGE_UNDERFLOW;
    case WebLocalizedString::kValidationRangeUnderflowDateTime:
      return IDS_FORM_VALIDATION_RANGE_UNDERFLOW_DATETIME;
    case WebLocalizedString::kValidationStepMismatch:
      return IDS_FORM_VALIDATION_STEP_MISMATCH;
    case WebLocalizedString::kValidationStepMismatchCloseToLimit:
      return IDS_FORM_VALIDATION_STEP_MISMATCH_CLOSE_TO_LIMIT;
    case WebLocalizedString::kValidationTooLong:
      return IDS_FORM_VALIDATION_TOO_LONG;
    case WebLocalizedString::kValidationTooShort:
      return IDS_FORM_VALIDATION_TOO_SHORT;
    case WebLocalizedString::kValidationTooShortPlural:
      return IDS_FORM_VALIDATION_TOO_SHORT_PLURAL;
    case WebLocalizedString::kValidationTypeMismatch:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH;
    case WebLocalizedString::kValidationTypeMismatchForEmail:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL;
    case WebLocalizedString::kValidationTypeMismatchForEmailEmpty:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_EMPTY;
    case WebLocalizedString::kValidationTypeMismatchForEmailEmptyDomain:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_EMPTY_DOMAIN;
    case WebLocalizedString::kValidationTypeMismatchForEmailEmptyLocal:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_EMPTY_LOCAL;
    case WebLocalizedString::kValidationTypeMismatchForEmailInvalidDomain:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_INVALID_DOMAIN;
    case WebLocalizedString::kValidationTypeMismatchForEmailInvalidDots:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_INVALID_DOTS;
    case WebLocalizedString::kValidationTypeMismatchForEmailInvalidLocal:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_INVALID_LOCAL;
    case WebLocalizedString::kValidationTypeMismatchForEmailNoAtSign:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_NO_AT_SIGN;
    case WebLocalizedString::kValidationTypeMismatchForMultipleEmail:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_MULTIPLE_EMAIL;
    case WebLocalizedString::kValidationTypeMismatchForURL:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_URL;
    case WebLocalizedString::kValidationValueMissing:
      return IDS_FORM_VALIDATION_VALUE_MISSING;
    case WebLocalizedString::kValidationValueMissingForCheckbox:
      return IDS_FORM_VALIDATION_VALUE_MISSING_CHECKBOX;
    case WebLocalizedString::kValidationValueMissingForFile:
      return IDS_FORM_VALIDATION_VALUE_MISSING_FILE;
    case WebLocalizedString::kValidationValueMissingForMultipleFile:
      return IDS_FORM_VALIDATION_VALUE_MISSING_MULTIPLE_FILE;
    case WebLocalizedString::kValidationValueMissingForRadio:
      return IDS_FORM_VALIDATION_VALUE_MISSING_RADIO;
    case WebLocalizedString::kValidationValueMissingForSelect:
      return IDS_FORM_VALIDATION_VALUE_MISSING_SELECT;
    case WebLocalizedString::kWeekFormatTemplate:
      return IDS_FORM_INPUT_WEEK_TEMPLATE;
    case WebLocalizedString::kWeekNumberLabel:
      return IDS_FORM_WEEK_NUMBER_LABEL;
    case WebLocalizedString::kTextTracksNoLabel:
      return IDS_MEDIA_TRACKS_NO_LABEL;
    case WebLocalizedString::kTextTracksOff:
      return IDS_MEDIA_TRACKS_OFF;
    // This "default:" line exists to avoid compile warnings about enum
    // coverage when we add a new symbol to WebLocalizedString.h in WebKit.
    // After a planned WebKit patch is landed, we need to add a case statement
    // for the added symbol here.
    default:
      break;
  }
  return -1;
}

// TODO(skyostil): Ensure that we always have an active task runner when
// constructing the platform.
BlinkPlatformImpl::BlinkPlatformImpl()
    : BlinkPlatformImpl(base::ThreadTaskRunnerHandle::IsSet()
                            ? base::ThreadTaskRunnerHandle::Get()
                            : nullptr) {
}

BlinkPlatformImpl::BlinkPlatformImpl(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner)
    : main_thread_task_runner_(main_thread_task_runner),
      compositor_thread_(nullptr) {
  InternalInit();
}

void BlinkPlatformImpl::InternalInit() {
  // ChildThread may not exist in some tests.
  if (ChildThreadImpl::current()) {
    thread_safe_sender_ = ChildThreadImpl::current()->thread_safe_sender();
    notification_dispatcher_ =
        ChildThreadImpl::current()->notification_dispatcher();
  }
}

void BlinkPlatformImpl::WaitUntilWebThreadTLSUpdate(
    blink::scheduler::WebThreadBase* thread) {
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  thread->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&BlinkPlatformImpl::UpdateWebThreadTLS, base::Unretained(this),
                 base::Unretained(thread), base::Unretained(&event)));
  event.Wait();
}

void BlinkPlatformImpl::UpdateWebThreadTLS(blink::WebThread* thread,
                                           base::WaitableEvent* event) {
  DCHECK(!current_thread_slot_.Get());
  current_thread_slot_.Set(thread);
  event->Signal();
}

BlinkPlatformImpl::~BlinkPlatformImpl() {
}

std::unique_ptr<blink::WebDataConsumerHandle>
BlinkPlatformImpl::CreateDataConsumerHandle(
    mojo::ScopedDataPipeConsumerHandle handle) {
  return base::MakeUnique<WebDataConsumerHandleImpl>(std::move(handle));
}

WebString BlinkPlatformImpl::UserAgent() {
  return blink::WebString::FromUTF8(GetContentClient()->GetUserAgent());
}

std::unique_ptr<blink::WebThread> BlinkPlatformImpl::CreateThread(
    const char* name) {
  std::unique_ptr<blink::scheduler::WebThreadBase> thread =
      blink::scheduler::WebThreadBase::CreateWorkerThread(
          name, base::Thread::Options());
  thread->Init();
  WaitUntilWebThreadTLSUpdate(thread.get());
  return std::move(thread);
}

void BlinkPlatformImpl::SetCompositorThread(
    blink::scheduler::WebThreadBase* compositor_thread) {
  compositor_thread_ = compositor_thread;
  if (compositor_thread_)
    WaitUntilWebThreadTLSUpdate(compositor_thread_);
}

blink::WebThread* BlinkPlatformImpl::CurrentThread() {
  return static_cast<blink::WebThread*>(current_thread_slot_.Get());
}

void BlinkPlatformImpl::RecordAction(const blink::UserMetricsAction& name) {
  if (ChildThread* child_thread = ChildThread::Get())
    child_thread->RecordComputedAction(name.Action());
}

namespace {

WebData loadAudioSpatializationResource(const char* name) {
#ifdef IDR_AUDIO_SPATIALIZATION_COMPOSITE
  if (!strcmp(name, "Composite")) {
    base::StringPiece resource = GetContentClient()->GetDataResource(
        IDR_AUDIO_SPATIALIZATION_COMPOSITE, ui::SCALE_FACTOR_NONE);
    return WebData(resource.data(), resource.size());
  }
#endif

#ifdef IDR_AUDIO_SPATIALIZATION_T000_P000
  const size_t kExpectedSpatializationNameLength = 31;
  if (strlen(name) != kExpectedSpatializationNameLength) {
    return WebData();
  }

  // Extract the azimuth and elevation from the resource name.
  int azimuth = 0;
  int elevation = 0;
  int values_parsed =
      sscanf(name, "IRC_Composite_C_R0195_T%3d_P%3d", &azimuth, &elevation);
  if (values_parsed != 2) {
    return WebData();
  }

  // The resource index values go through the elevations first, then azimuths.
  const int kAngleSpacing = 15;

  // 0 <= elevation <= 90 (or 315 <= elevation <= 345)
  // in increments of 15 degrees.
  int elevation_index =
      elevation <= 90 ? elevation / kAngleSpacing :
      7 + (elevation - 315) / kAngleSpacing;
  bool is_elevation_index_good = 0 <= elevation_index && elevation_index < 10;

  // 0 <= azimuth < 360 in increments of 15 degrees.
  int azimuth_index = azimuth / kAngleSpacing;
  bool is_azimuth_index_good = 0 <= azimuth_index && azimuth_index < 24;

  const int kNumberOfElevations = 10;
  const int kNumberOfAudioResources = 240;
  int resource_index = kNumberOfElevations * azimuth_index + elevation_index;
  bool is_resource_index_good = 0 <= resource_index &&
      resource_index < kNumberOfAudioResources;

  if (is_azimuth_index_good && is_elevation_index_good &&
      is_resource_index_good) {
    const int kFirstAudioResourceIndex = IDR_AUDIO_SPATIALIZATION_T000_P000;
    base::StringPiece resource = GetContentClient()->GetDataResource(
        kFirstAudioResourceIndex + resource_index, ui::SCALE_FACTOR_NONE);
    return WebData(resource.data(), resource.size());
  }
#endif  // IDR_AUDIO_SPATIALIZATION_T000_P000

  NOTREACHED();
  return WebData();
}

// This must match third_party/WebKit/public/blink_resources.grd.
// In particular, |is_gzipped| corresponds to compress="gzip".
struct DataResource {
  const char* name;
  int id;
  ui::ScaleFactor scale_factor;
  bool is_gzipped;
};

const DataResource kDataResources[] = {
    {"missingImage", IDR_BROKENIMAGE, ui::SCALE_FACTOR_100P, false},
    {"missingImage@2x", IDR_BROKENIMAGE, ui::SCALE_FACTOR_200P, false},
    {"mediaplayerPause", IDR_MEDIAPLAYER_PAUSE_BUTTON, ui::SCALE_FACTOR_100P,
     false},
    {"mediaplayerPlay", IDR_MEDIAPLAYER_PLAY_BUTTON, ui::SCALE_FACTOR_100P,
     false},
    {"mediaplayerSoundNotMuted", IDR_MEDIAPLAYER_SOUND_NOT_MUTED_BUTTON,
     ui::SCALE_FACTOR_100P, false},
    {"mediaplayerSoundMuted", IDR_MEDIAPLAYER_SOUND_MUTED_BUTTON,
     ui::SCALE_FACTOR_100P, false},
    {"mediaplayerSliderThumb", IDR_MEDIAPLAYER_SLIDER_THUMB,
     ui::SCALE_FACTOR_100P, false},
    {"mediaplayerVolumeSliderThumb", IDR_MEDIAPLAYER_VOLUME_SLIDER_THUMB,
     ui::SCALE_FACTOR_100P, false},
    {"mediaplayerClosedCaption", IDR_MEDIAPLAYER_CLOSEDCAPTION_BUTTON,
     ui::SCALE_FACTOR_100P, false},
    {"mediaplayerClosedCaptionDisabled",
     IDR_MEDIAPLAYER_CLOSEDCAPTION_BUTTON_DISABLED, ui::SCALE_FACTOR_100P,
     false},
    {"mediaplayerEnterFullscreen", IDR_MEDIAPLAYER_ENTER_FULLSCREEN_BUTTON,
     ui::SCALE_FACTOR_100P, false},
    {"mediaplayerExitFullscreen", IDR_MEDIAPLAYER_EXIT_FULLSCREEN_BUTTON,
     ui::SCALE_FACTOR_100P, false},
    {"mediaplayerCastOff", IDR_MEDIAPLAYER_CAST_BUTTON_OFF,
     ui::SCALE_FACTOR_100P, false},
    {"mediaplayerCastOn", IDR_MEDIAPLAYER_CAST_BUTTON_ON, ui::SCALE_FACTOR_100P,
     false},
    {"mediaplayerOverlayCastOff", IDR_MEDIAPLAYER_OVERLAY_CAST_BUTTON_OFF,
     ui::SCALE_FACTOR_100P, false},
    {"mediaplayerOverlayPlay", IDR_MEDIAPLAYER_OVERLAY_PLAY_BUTTON,
     ui::SCALE_FACTOR_100P, false},
    {"mediaRemotingCastIcon", IDR_MEDIA_REMOTING_CAST_ICON,
     ui::SCALE_FACTOR_100P, false},
    {"mediaplayerTrackSelectionCheckmark",
     IDR_MEDIAPLAYER_TRACKSELECTION_CHECKMARK, ui::SCALE_FACTOR_100P, false},
    {"mediaplayerClosedCaptionsIcon", IDR_MEDIAPLAYER_CLOSEDCAPTIONS_ICON,
     ui::SCALE_FACTOR_100P, false},
    {"mediaplayerSubtitlesIcon", IDR_MEDIAPLAYER_SUBTITLES_ICON,
     ui::SCALE_FACTOR_100P, false},
    {"mediaplayerOverflowMenu", IDR_MEDIAPLAYER_OVERFLOW_MENU_ICON,
     ui::SCALE_FACTOR_100P, false},
    {"mediaplayerDownloadIcon", IDR_MEDIAPLAYER_DOWNLOAD_ICON,
     ui::SCALE_FACTOR_100P, false},
    {"searchCancel", IDR_SEARCH_CANCEL, ui::SCALE_FACTOR_100P, false},
    {"searchCancelPressed", IDR_SEARCH_CANCEL_PRESSED, ui::SCALE_FACTOR_100P,
     false},
    {"generatePassword", IDR_PASSWORD_GENERATION_ICON, ui::SCALE_FACTOR_100P,
     false},
    {"generatePasswordHover", IDR_PASSWORD_GENERATION_ICON_HOVER,
     ui::SCALE_FACTOR_100P, false},
    {"html.css", IDR_UASTYLE_HTML_CSS, ui::SCALE_FACTOR_NONE, true},
    {"quirks.css", IDR_UASTYLE_QUIRKS_CSS, ui::SCALE_FACTOR_NONE, true},
    {"view-source.css", IDR_UASTYLE_VIEW_SOURCE_CSS, ui::SCALE_FACTOR_NONE,
     true},
    // Not limited to Android since it's used for mobile layouts in inspector.
    {"themeChromiumAndroid.css", IDR_UASTYLE_THEME_CHROMIUM_ANDROID_CSS,
     ui::SCALE_FACTOR_NONE, true},
    // Not limited to Android since it's used for mobile layouts in inspector.
    {"fullscreenAndroid.css", IDR_UASTYLE_FULLSCREEN_ANDROID_CSS,
     ui::SCALE_FACTOR_NONE, true},
    // Not limited to Android since it's used for mobile layouts in inspector.
    {"mediaControlsAndroid.css", IDR_UASTYLE_MEDIA_CONTROLS_ANDROID_CSS,
     ui::SCALE_FACTOR_NONE, true},
    // Not limited to Linux since it's used for mobile layouts in inspector.
    {"themeChromiumLinux.css", IDR_UASTYLE_THEME_CHROMIUM_LINUX_CSS,
     ui::SCALE_FACTOR_NONE, true},
    {"themeInputMultipleFields.css",
     IDR_UASTYLE_THEME_INPUT_MULTIPLE_FIELDS_CSS, ui::SCALE_FACTOR_NONE, true},
#if defined(OS_MACOSX)
    {"themeMac.css", IDR_UASTYLE_THEME_MAC_CSS, ui::SCALE_FACTOR_NONE, true},
#endif
    {"themeWin.css", IDR_UASTYLE_THEME_WIN_CSS, ui::SCALE_FACTOR_NONE, true},
    {"themeWinQuirks.css", IDR_UASTYLE_THEME_WIN_QUIRKS_CSS,
     ui::SCALE_FACTOR_NONE, true},
    {"svg.css", IDR_UASTYLE_SVG_CSS, ui::SCALE_FACTOR_NONE, true},
    {"mathml.css", IDR_UASTYLE_MATHML_CSS, ui::SCALE_FACTOR_NONE, true},
    {"mediaControls.css", IDR_UASTYLE_MEDIA_CONTROLS_CSS, ui::SCALE_FACTOR_NONE,
     true},
    {"fullscreen.css", IDR_UASTYLE_FULLSCREEN_CSS, ui::SCALE_FACTOR_NONE, true},
    {"xhtmlmp.css", IDR_UASTYLE_XHTMLMP_CSS, ui::SCALE_FACTOR_NONE, true},
    {"viewportAndroid.css", IDR_UASTYLE_VIEWPORT_ANDROID_CSS,
     ui::SCALE_FACTOR_NONE, true},
    {"viewportTelevision.css", IDR_UASTYLE_VIEWPORT_TELEVISION_CSS,
     ui::SCALE_FACTOR_NONE, true},
    {"InspectorOverlayPage.html", IDR_INSPECTOR_OVERLAY_PAGE_HTML,
     ui::SCALE_FACTOR_NONE, true},
    {"DocumentXMLTreeViewer.css", IDR_DOCUMENTXMLTREEVIEWER_CSS,
     ui::SCALE_FACTOR_NONE, true},
    {"DocumentXMLTreeViewer.js", IDR_DOCUMENTXMLTREEVIEWER_JS,
     ui::SCALE_FACTOR_NONE, true},
#ifdef IDR_PICKER_COMMON_JS
    {"pickerCommon.js", IDR_PICKER_COMMON_JS, ui::SCALE_FACTOR_NONE, true},
    {"pickerCommon.css", IDR_PICKER_COMMON_CSS, ui::SCALE_FACTOR_NONE, true},
    {"calendarPicker.js", IDR_CALENDAR_PICKER_JS, ui::SCALE_FACTOR_NONE, true},
    {"calendarPicker.css", IDR_CALENDAR_PICKER_CSS, ui::SCALE_FACTOR_NONE,
     true},
    {"listPicker.js", IDR_LIST_PICKER_JS, ui::SCALE_FACTOR_NONE, true},
    {"listPicker.css", IDR_LIST_PICKER_CSS, ui::SCALE_FACTOR_NONE, true},
    {"pickerButton.css", IDR_PICKER_BUTTON_CSS, ui::SCALE_FACTOR_NONE, true},
    {"suggestionPicker.js", IDR_SUGGESTION_PICKER_JS, ui::SCALE_FACTOR_NONE,
     true},
    {"suggestionPicker.css", IDR_SUGGESTION_PICKER_CSS, ui::SCALE_FACTOR_NONE,
     true},
    {"colorSuggestionPicker.js", IDR_COLOR_SUGGESTION_PICKER_JS,
     ui::SCALE_FACTOR_NONE, true},
    {"colorSuggestionPicker.css", IDR_COLOR_SUGGESTION_PICKER_CSS,
     ui::SCALE_FACTOR_NONE, true},
#endif
    {"input_alert.svg", IDR_VALIDATION_BUBBLE_ICON, ui::SCALE_FACTOR_NONE,
     true},
    {"validation_bubble.css", IDR_VALIDATION_BUBBLE_CSS, ui::SCALE_FACTOR_NONE,
     true},
    {"placeholderIcon", IDR_PLACEHOLDER_ICON, ui::SCALE_FACTOR_100P, false},
};

}  // namespace

WebData BlinkPlatformImpl::GetDataResource(const char* name) {
  // Some clients will call into this method with an empty |name| when they have
  // optional resources.  For example, the PopupMenuChromium code can have icons
  // for some Autofill items but not for others.
  if (!strlen(name))
    return WebData();

  // Check the name prefix to see if it's an audio resource.
  if (base::StartsWith(name, "IRC_Composite", base::CompareCase::SENSITIVE) ||
      base::StartsWith(name, "Composite", base::CompareCase::SENSITIVE))
    return loadAudioSpatializationResource(name);

  // TODO(flackr): We should use a better than linear search here, a trie would
  // be ideal.
  for (size_t i = 0; i < arraysize(kDataResources); ++i) {
    if (!strcmp(name, kDataResources[i].name)) {
      base::StringPiece resource = GetContentClient()->GetDataResource(
          kDataResources[i].id, kDataResources[i].scale_factor);
      if (kDataResources[i].is_gzipped) {
        std::string uncompressed;
        CHECK(compression::GzipUncompress(resource.as_string(), &uncompressed));
        return WebData(uncompressed.data(), uncompressed.size());
      }
      return WebData(resource.data(), resource.size());
    }
  }

  NOTREACHED() << "Unknown image resource " << name;
  return WebData();
}

WebString BlinkPlatformImpl::QueryLocalizedString(
    WebLocalizedString::Name name) {
  int message_id = ToMessageID(name);
  if (message_id < 0)
    return WebString();
  return WebString::FromUTF16(
      GetContentClient()->GetLocalizedString(message_id));
}

WebString BlinkPlatformImpl::queryLocalizedString(
    WebLocalizedString::Name name, int numeric_value) {
  return QueryLocalizedString(
      name, WebString::FromUTF16(base::IntToString16(numeric_value)));
}

WebString BlinkPlatformImpl::QueryLocalizedString(WebLocalizedString::Name name,
                                                  const WebString& value) {
  int message_id = ToMessageID(name);
  if (message_id < 0)
    return WebString();
  return WebString::FromUTF16(base::ReplaceStringPlaceholders(
      GetContentClient()->GetLocalizedString(message_id), value.Utf16(), NULL));
}

WebString BlinkPlatformImpl::QueryLocalizedString(WebLocalizedString::Name name,
                                                  const WebString& value1,
                                                  const WebString& value2) {
  int message_id = ToMessageID(name);
  if (message_id < 0)
    return WebString();
  std::vector<base::string16> values;
  values.reserve(2);
  values.push_back(value1.Utf16());
  values.push_back(value2.Utf16());
  return WebString::FromUTF16(base::ReplaceStringPlaceholders(
      GetContentClient()->GetLocalizedString(message_id), values, NULL));
}

blink::WebThread* BlinkPlatformImpl::CompositorThread() const {
  return compositor_thread_;
}

std::unique_ptr<blink::WebGestureCurve>
BlinkPlatformImpl::CreateFlingAnimationCurve(
    blink::WebGestureDevice device_source,
    const blink::WebFloatPoint& velocity,
    const blink::WebSize& cumulative_scroll) {
  return ui::WebGestureCurveImpl::CreateFromDefaultPlatformCurve(
      device_source, gfx::Vector2dF(velocity.x, velocity.y),
      gfx::Vector2dF(cumulative_scroll.width, cumulative_scroll.height),
      IsMainThread());
}

void BlinkPlatformImpl::DidStartWorkerThread() {
  WorkerThreadRegistry::Instance()->DidStartCurrentWorkerThread();
}

void BlinkPlatformImpl::WillStopWorkerThread() {
  WorkerThreadRegistry::Instance()->WillStopCurrentWorkerThread();
}

bool BlinkPlatformImpl::AllowScriptExtensionForServiceWorker(
    const blink::WebURL& scriptUrl) {
  return GetContentClient()->AllowScriptExtensionForServiceWorker(scriptUrl);
}

blink::WebCrypto* BlinkPlatformImpl::Crypto() {
  return &web_crypto_;
}

blink::WebNotificationManager* BlinkPlatformImpl::GetNotificationManager() {
  if (!thread_safe_sender_.get() || !notification_dispatcher_.get())
    return nullptr;

  return NotificationManager::ThreadSpecificInstance(
      thread_safe_sender_.get(),
      notification_dispatcher_.get());
}

blink::WebPushProvider* BlinkPlatformImpl::PushProvider() {
  return PushProvider::ThreadSpecificInstance(main_thread_task_runner_);
}

blink::WebMediaCapabilitiesClient*
BlinkPlatformImpl::MediaCapabilitiesClient() {
  return &media_capabilities_client_;
}

WebThemeEngine* BlinkPlatformImpl::ThemeEngine() {
  return &native_theme_engine_;
}

WebFallbackThemeEngine* BlinkPlatformImpl::FallbackThemeEngine() {
  return &fallback_theme_engine_;
}

blink::Platform::FileHandle BlinkPlatformImpl::DatabaseOpenFile(
    const blink::WebString& vfs_file_name,
    int desired_flags) {
#if defined(OS_WIN)
  return INVALID_HANDLE_VALUE;
#elif defined(OS_POSIX)
  return -1;
#endif
}

int BlinkPlatformImpl::DatabaseDeleteFile(const blink::WebString& vfs_file_name,
                                          bool sync_dir) {
  return -1;
}

long BlinkPlatformImpl::DatabaseGetFileAttributes(
    const blink::WebString& vfs_file_name) {
  return 0;
}

long long BlinkPlatformImpl::DatabaseGetFileSize(
    const blink::WebString& vfs_file_name) {
  return 0;
}

long long BlinkPlatformImpl::DatabaseGetSpaceAvailableForOrigin(
    const blink::WebSecurityOrigin& origin) {
  return 0;
}

bool BlinkPlatformImpl::DatabaseSetFileSize(
    const blink::WebString& vfs_file_name,
    long long size) {
  return false;
}

size_t BlinkPlatformImpl::ActualMemoryUsageMB() {
  return GetMemoryUsageKB() >> 10;
}

size_t BlinkPlatformImpl::NumberOfProcessors() {
  return static_cast<size_t>(base::SysInfo::NumberOfProcessors());
}

size_t BlinkPlatformImpl::MaxDecodedImageBytes() {
#if defined(OS_ANDROID)
  if (base::SysInfo::IsLowEndDevice()) {
    // Limit image decoded size to 3M pixels on low end devices.
    // 4 is maximum number of bytes per pixel.
    return 3 * 1024 * 1024 * 4;
  }
  // For other devices, limit decoded image size based on the amount of physical
  // memory.
  // In some cases all physical memory is not accessible by Chromium, as it can
  // be reserved for direct use by certain hardware. Thus, we set the limit so
  // that 1.6GB of reported physical memory on a 2GB device is enough to set the
  // limit at 16M pixels, which is a desirable value since 4K*4K is a relatively
  // common texture size.
  return base::SysInfo::AmountOfPhysicalMemory() / 25;
#else
  return kNoDecodedImageByteLimit;
#endif
}

bool BlinkPlatformImpl::IsLowEndDevice() {
  return base::SysInfo::IsLowEndDevice();
}

uint32_t BlinkPlatformImpl::GetUniqueIdForProcess() {
  // TODO(rickyz): Replace this with base::GetUniqueIdForProcess when that's
  // ready.
  return base::trace_event::TraceLog::GetInstance()->process_id();
}

bool BlinkPlatformImpl::IsMainThread() const {
  return main_thread_task_runner_.get() &&
         main_thread_task_runner_->BelongsToCurrentThread();
}

WebString BlinkPlatformImpl::DomCodeStringFromEnum(int dom_code) {
  return WebString::FromUTF8(ui::KeycodeConverter::DomCodeToCodeString(
      static_cast<ui::DomCode>(dom_code)));
}

int BlinkPlatformImpl::DomEnumFromCodeString(const WebString& code) {
  return static_cast<int>(
      ui::KeycodeConverter::CodeStringToDomCode(code.Utf8()));
}

WebString BlinkPlatformImpl::DomKeyStringFromEnum(int dom_key) {
  return WebString::FromUTF8(ui::KeycodeConverter::DomKeyToKeyString(
      static_cast<ui::DomKey>(dom_key)));
}

int BlinkPlatformImpl::DomKeyEnumFromString(const WebString& key_string) {
  return static_cast<int>(
      ui::KeycodeConverter::KeyStringToDomKey(key_string.Utf8()));
}

bool BlinkPlatformImpl::IsDomKeyForModifier(int dom_key) {
  return ui::KeycodeConverter::IsDomKeyForModifier(
      static_cast<ui::DomKey>(dom_key));
}

std::unique_ptr<blink::WebFeaturePolicy> BlinkPlatformImpl::CreateFeaturePolicy(
    const blink::WebFeaturePolicy* parent_policy,
    const blink::WebParsedFeaturePolicy& container_policy,
    const blink::WebParsedFeaturePolicy& policy_header,
    const blink::WebSecurityOrigin& origin) {
  std::unique_ptr<FeaturePolicy> policy = FeaturePolicy::CreateFromParentPolicy(
      static_cast<const FeaturePolicy*>(parent_policy),
      FeaturePolicyHeaderFromWeb(container_policy), url::Origin(origin));
  policy->SetHeaderPolicy(FeaturePolicyHeaderFromWeb(policy_header));
  return std::move(policy);
}

std::unique_ptr<blink::WebFeaturePolicy>
BlinkPlatformImpl::DuplicateFeaturePolicyWithOrigin(
    const blink::WebFeaturePolicy& policy,
    const blink::WebSecurityOrigin& new_origin) {
  return FeaturePolicy::CreateFromPolicyWithOrigin(
      static_cast<const FeaturePolicy&>(policy), url::Origin(new_origin));
}

}  // namespace content
