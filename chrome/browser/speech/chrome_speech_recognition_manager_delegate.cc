// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/chrome_speech_recognition_manager_delegate.h"

#include <string>

#include "base/bind.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/chrome_speech_recognition_preferences.h"
#include "chrome/browser/speech/speech_recognition_tray_icon_controller.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/view_type_utils.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/speech_recognition_manager.h"
#include "content/public/browser/speech_recognition_session_config.h"
#include "content/public/browser/speech_recognition_session_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/speech_recognition_error.h"
#include "content/public/common/speech_recognition_result.h"
#include "grit/generated_resources.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)
#include "chrome/installer/util/wmi.h"
#endif

using content::BrowserThread;
using content::SpeechRecognitionManager;
using content::SpeechRecognitionSessionContext;
using content::WebContents;

namespace {
const int kNoActiveBubble =
    content::SpeechRecognitionManager::kSessionIDInvalid;

const char kExtensionPrefix[] = "chrome-extension://";

bool RequiresBubble(int session_id) {
  return SpeechRecognitionManager::GetInstance()->
      GetSessionContext(session_id).requested_by_page_element;
}

bool RequiresTrayIcon(int session_id) {
  return !RequiresBubble(session_id);
}
}  // namespace

namespace speech {

// Asynchronously fetches the PC and audio hardware/driver info if
// the user has opted into UMA. This information is sent with speech input
// requests to the server for identifying and improving quality issues with
// specific device configurations.
class ChromeSpeechRecognitionManagerDelegate::OptionalRequestInfo
    : public base::RefCountedThreadSafe<OptionalRequestInfo> {
 public:
  OptionalRequestInfo() : can_report_metrics_(false) {
  }

  void Refresh() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    // UMA opt-in can be checked only from the UI thread, so switch to that.
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&OptionalRequestInfo::CheckUMAAndGetHardwareInfo, this));
  }

  void CheckUMAAndGetHardwareInfo() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (g_browser_process->local_state()->GetBoolean(
        prefs::kMetricsReportingEnabled)) {
      // Access potentially slow OS calls from the FILE thread.
      BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
          base::Bind(&OptionalRequestInfo::GetHardwareInfo, this));
    }
  }

  void GetHardwareInfo() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    base::AutoLock lock(lock_);
    can_report_metrics_ = true;
    string16 device_model =
        SpeechRecognitionManager::GetInstance()->GetAudioInputDeviceModel();
#if defined(OS_WIN)
    value_ = UTF16ToUTF8(
        installer::WMIComputerSystem::GetModel() + L"|" + device_model);
#else  // defined(OS_WIN)
    value_ = UTF16ToUTF8(device_model);
#endif  // defined(OS_WIN)
  }

  std::string value() {
    base::AutoLock lock(lock_);
    return value_;
  }

  bool can_report_metrics() {
    base::AutoLock lock(lock_);
    return can_report_metrics_;
  }

 private:
  friend class base::RefCountedThreadSafe<OptionalRequestInfo>;

  ~OptionalRequestInfo() {}

  base::Lock lock_;
  std::string value_;
  bool can_report_metrics_;

  DISALLOW_COPY_AND_ASSIGN(OptionalRequestInfo);
};

ChromeSpeechRecognitionManagerDelegate::ChromeSpeechRecognitionManagerDelegate()
    : active_bubble_session_id_(kNoActiveBubble) {
}

ChromeSpeechRecognitionManagerDelegate::
    ~ChromeSpeechRecognitionManagerDelegate() {
  if (tray_icon_controller_.get())
    tray_icon_controller_->Hide();
  if (active_bubble_session_id_ != kNoActiveBubble) {
    DCHECK(bubble_controller_.get());
    bubble_controller_->CloseBubble(active_bubble_session_id_);
  }
}

void ChromeSpeechRecognitionManagerDelegate::InfoBubbleButtonClicked(
    int session_id, SpeechRecognitionBubble::Button button) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(active_bubble_session_id_, session_id);

  // Note, the session might have been destroyed, therefore avoid calls to the
  // manager which imply its existance (e.g., GetSessionContext()).

  if (button == SpeechRecognitionBubble::BUTTON_CANCEL) {
    GetBubbleController()->CloseBubble(session_id);
    last_session_config_.reset();
    active_bubble_session_id_ = kNoActiveBubble;

    // We can safely call AbortSession even if the session has already ended,
    // the manager's public methods are reliable and will handle it properly.
    SpeechRecognitionManager::GetInstance()->AbortSession(session_id);
  } else if (button == SpeechRecognitionBubble::BUTTON_TRY_AGAIN) {
    GetBubbleController()->CloseBubble(session_id);
    active_bubble_session_id_ = kNoActiveBubble;
    RestartLastSession();
  }
}

void ChromeSpeechRecognitionManagerDelegate::InfoBubbleFocusChanged(
    int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(active_bubble_session_id_, session_id);

  // Note, the session might have been destroyed, therefore avoid calls to the
  // manager which imply its existance (e.g., GetSessionContext()).

  GetBubbleController()->CloseBubble(session_id);
  last_session_config_.reset();
  active_bubble_session_id_ = kNoActiveBubble;

  // If the user clicks outside the bubble while capturing audio we abort the
  // session. Otherwise, i.e. audio capture is ended and we are just waiting for
  // results, this activity is carried silently in background.
  if (SpeechRecognitionManager::GetInstance()->IsCapturingAudio())
    SpeechRecognitionManager::GetInstance()->AbortSession(session_id);
}

void ChromeSpeechRecognitionManagerDelegate::RestartLastSession() {
  DCHECK(last_session_config_.get());
  SpeechRecognitionManager* manager = SpeechRecognitionManager::GetInstance();
  const int new_session_id = manager->CreateSession(*last_session_config_);
  DCHECK_NE(new_session_id, kNoActiveBubble);
  last_session_config_.reset();
  manager->StartSession(new_session_id);
}

void ChromeSpeechRecognitionManagerDelegate::OnRecognitionStart(
    int session_id) {
  const content::SpeechRecognitionSessionContext& context =
      SpeechRecognitionManager::GetInstance()->GetSessionContext(session_id);

  if (RequiresBubble(session_id)) {
    // Copy the configuration of the session (for the "try again" button).
    last_session_config_.reset(new content::SpeechRecognitionSessionConfig(
        SpeechRecognitionManager::GetInstance()->GetSessionConfig(session_id)));

    // Create and show the bubble.
    DCHECK_EQ(active_bubble_session_id_, kNoActiveBubble);
    active_bubble_session_id_ = session_id;
    GetBubbleController()->CreateBubble(session_id,
                                        context.render_process_id,
                                        context.render_view_id,
                                        context.element_rect);

    // TODO(primiano) Why not creating directly the bubble in warmup mode?
    GetBubbleController()->SetBubbleWarmUpMode(session_id);
  }
}

void ChromeSpeechRecognitionManagerDelegate::OnAudioStart(int session_id) {
  if (RequiresBubble(session_id)) {
    GetBubbleController()->SetBubbleRecordingMode(session_id);
  } else if (RequiresTrayIcon(session_id)) {
    // We post the action to the UI thread for sessions requiring a tray icon,
    // since ChromeSpeechRecognitionPreferences (which requires UI thread) is
    // involved for determining whether a security alert balloon is required.
    const content::SpeechRecognitionSessionContext& context =
        SpeechRecognitionManager::GetInstance()->GetSessionContext(session_id);
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
        &ChromeSpeechRecognitionManagerDelegate::ShowTrayIconOnUIThread,
        context.context_name,
        context.render_process_id,
        scoped_refptr<SpeechRecognitionTrayIconController>(
            GetTrayIconController())));
  }
}

void ChromeSpeechRecognitionManagerDelegate::OnEnvironmentEstimationComplete(
    int session_id) {
}

void ChromeSpeechRecognitionManagerDelegate::OnSoundStart(int session_id) {
}

void ChromeSpeechRecognitionManagerDelegate::OnSoundEnd(int session_id) {
}

void ChromeSpeechRecognitionManagerDelegate::OnAudioEnd(int session_id) {
  // OnAudioEnd can be also raised after an abort, when the bubble has already
  // been closed.
  if (RequiresBubble(session_id) && active_bubble_session_id_ == session_id) {
    GetBubbleController()->SetBubbleRecognizingMode(session_id);
  } else if (RequiresTrayIcon(session_id)) {
    GetTrayIconController()->Hide();
  }
}

void ChromeSpeechRecognitionManagerDelegate::OnRecognitionResult(
    int session_id, const content::SpeechRecognitionResult& result) {
  // A result can be dispatched when the bubble is not visible anymore (e.g.,
  // lost focus while waiting for a result, thus continuing in background).
  if (RequiresBubble(session_id) && active_bubble_session_id_ == session_id) {
    GetBubbleController()->CloseBubble(session_id);
    last_session_config_.reset();
    active_bubble_session_id_ = kNoActiveBubble;
  }
}

void ChromeSpeechRecognitionManagerDelegate::OnRecognitionError(
    int session_id, const content::SpeechRecognitionError& error) {
  // An error can be dispatched when the bubble is not visible anymore.
  if (active_bubble_session_id_ != session_id)
    return;
  DCHECK(RequiresBubble(session_id));

  int error_message_id = 0;
  switch (error.code) {
    case content::SPEECH_RECOGNITION_ERROR_AUDIO:
      switch (error.details) {
        case content::SPEECH_AUDIO_ERROR_DETAILS_NO_MIC:
          error_message_id = IDS_SPEECH_INPUT_NO_MIC;
          break;
        case content::SPEECH_AUDIO_ERROR_DETAILS_IN_USE:
          error_message_id = IDS_SPEECH_INPUT_MIC_IN_USE;
          break;
        default:
          error_message_id = IDS_SPEECH_INPUT_MIC_ERROR;
          break;
      }
      break;
    case content::SPEECH_RECOGNITION_ERROR_ABORTED:
      error_message_id = IDS_SPEECH_INPUT_ABORTED;
      break;
    case content::SPEECH_RECOGNITION_ERROR_NO_SPEECH:
      error_message_id = IDS_SPEECH_INPUT_NO_SPEECH;
      break;
    case content::SPEECH_RECOGNITION_ERROR_NO_MATCH:
      error_message_id = IDS_SPEECH_INPUT_NO_RESULTS;
      break;
    case content::SPEECH_RECOGNITION_ERROR_NETWORK:
      error_message_id = IDS_SPEECH_INPUT_NET_ERROR;
      break;
    default:
      NOTREACHED() << "unknown error " << error.code;
      return;
  }
  GetBubbleController()->SetBubbleMessage(
      session_id, l10n_util::GetStringUTF16(error_message_id));
}

void ChromeSpeechRecognitionManagerDelegate::OnAudioLevelsChange(
    int session_id, float volume, float noise_volume) {
  if (active_bubble_session_id_ == session_id) {
    DCHECK(RequiresBubble(session_id));
    GetBubbleController()->SetBubbleInputVolume(session_id,
                                                volume, noise_volume);
  } else if (RequiresTrayIcon(session_id)) {
    GetTrayIconController()->SetVUMeterVolume(volume);
  }
}

void ChromeSpeechRecognitionManagerDelegate::OnRecognitionEnd(int session_id) {
  // No need to remove the bubble here, since either one of the following events
  // must have happened prior to this callback:
  // - A previous OnRecognitionResult event already closed the bubble.
  // - An error occurred, so the bubble is showing the error and will be closed
  //   when it will lose focus (by InfoBubbleFocusChanged()).
  // - The bubble lost focus or the user pressed the Cancel button, thus it has
  //   been closed by InfoBubbleFocusChanged(), which triggered an AbortSession.
}

void ChromeSpeechRecognitionManagerDelegate::GetDiagnosticInformation(
    bool* can_report_metrics,
    std::string* hardware_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!optional_request_info_.get()) {
    optional_request_info_ = new OptionalRequestInfo();
    // Since hardware info is optional with speech input requests, we start an
    // asynchronous fetch here and move on with recording audio. This first
    // speech input request would send an empty string for hardware info and
    // subsequent requests may have the hardware info available if the fetch
    // completed before them. This way we don't end up stalling the user with
    // a long wait and disk seeks when they click on a UI element and start
    // speaking.
    optional_request_info_->Refresh();
  }
  *can_report_metrics = optional_request_info_->can_report_metrics();
  *hardware_info = optional_request_info_->value();
}

void ChromeSpeechRecognitionManagerDelegate::CheckRecognitionIsAllowed(
    int session_id,
    base::Callback<void(int session_id, bool is_allowed)> callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  const content::SpeechRecognitionSessionContext& context =
      SpeechRecognitionManager::GetInstance()->GetSessionContext(session_id);

  // Make sure that initiators (extensions/web pages) properly set the
  // |render_process_id| field, which is needed later to retrieve the
  // ChromeSpeechRecognitionPreferences associated to their profile.
  DCHECK_NE(context.render_process_id, 0);

  // We don't need any particular check for sessions not using a bubble. In such
  // cases, we just notify it to the manager (calling-back synchronously, since
  // we remain in the IO thread).
  if (RequiresTrayIcon(session_id)) {
    callback.Run(session_id, true /* is_allowed */);
    return;
  }

  // Sessions using bubbles, conversely, need a check on the renderer view type.
  // The check must be performed in the UI thread. We defer it posting to
  // CheckRenderViewType, which will issue the callback on our behalf.
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&CheckRenderViewType,
                                     session_id,
                                     callback,
                                     context.render_process_id,
                                     context.render_view_id));
}

content::SpeechRecognitionEventListener*
ChromeSpeechRecognitionManagerDelegate::GetEventListener() {
  return this;
}

void ChromeSpeechRecognitionManagerDelegate::ShowTrayIconOnUIThread(
    const std::string& context_name,
    int render_process_id,
    scoped_refptr<SpeechRecognitionTrayIconController> tray_icon_controller) {
  content::RenderProcessHost* render_process_host =
      content::RenderProcessHost::FromID(render_process_id);
  DCHECK(render_process_host);
  content::BrowserContext* browser_context =
      render_process_host->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);
  scoped_refptr<ChromeSpeechRecognitionPreferences> pref =
      ChromeSpeechRecognitionPreferences::GetForProfile(profile);
  bool show_notification = pref->ShouldShowSecurityNotification(context_name);
  if (show_notification)
    pref->SetHasShownSecurityNotification(context_name);

  // Speech recognitions initiated by JS APIs within an extension (so NOT by
  // extension API) will come with a context_name like "chrome-extension://id"
  // (that is, their origin as injected by WebKit). In such cases we try to
  // lookup the extension name, in order to show a more user-friendly balloon.
  string16 initiator_name = UTF8ToUTF16(context_name);
  if (context_name.find(kExtensionPrefix) == 0) {
    const std::string extension_id =
        context_name.substr(sizeof(kExtensionPrefix) - 1);
    const extensions::Extension* extension =
          profile->GetExtensionService()->GetExtensionById(extension_id, true);
    DCHECK(extension);
    initiator_name = UTF8ToUTF16(extension->name());
  }

  tray_icon_controller->Show(initiator_name, show_notification);
}

void ChromeSpeechRecognitionManagerDelegate::CheckRenderViewType(
    int session_id,
    base::Callback<void(int session_id, bool is_allowed)> callback,
    int render_process_id,
    int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const content::RenderViewHost* render_view_host =
      content::RenderViewHost::FromID(render_process_id, render_view_id);
  bool allowed = false;
  if (render_view_host) {
    // For host delegates other than VIEW_TYPE_TAB_CONTENTS we can't reliably
    // show a popup, including the speech input bubble. In these cases for
    // privacy reasons we don't want to start recording if the user can't be
    // properly notified. An example of this is trying to show the speech input
    // bubble within an extension popup: http://crbug.com/92083. In these
    // situations the speech input extension API should be used instead.
    WebContents* web_contents =
        WebContents::FromRenderViewHost(render_view_host);
    chrome::ViewType view_type = chrome::GetViewType(web_contents);
    if (view_type == chrome::VIEW_TYPE_TAB_CONTENTS)
      allowed = true;
  }
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(callback, session_id, allowed));
}

SpeechRecognitionBubbleController*
ChromeSpeechRecognitionManagerDelegate::GetBubbleController() {
  if (!bubble_controller_.get())
    bubble_controller_ = new SpeechRecognitionBubbleController(this);
  return bubble_controller_.get();
}

SpeechRecognitionTrayIconController*
ChromeSpeechRecognitionManagerDelegate::GetTrayIconController() {
  if (!tray_icon_controller_.get())
    tray_icon_controller_ = new SpeechRecognitionTrayIconController();
  return tray_icon_controller_.get();
}


}  // namespace speech
