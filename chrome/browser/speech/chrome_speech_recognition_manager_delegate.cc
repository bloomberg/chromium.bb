// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/chrome_speech_recognition_manager_delegate.h"

#include <set>
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
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
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
using content::WebContents;

namespace {

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

// Simple utility to get notified when a WebContent (a tab or an extension's
// background page) is closed or crashes. Both the callback site and the
// callback thread are passed by the caller in the constructor.
// There is no restriction on the constructor, however this class must be
// destroyed on the UI thread, due to the NotificationRegistrar dependency.
class ChromeSpeechRecognitionManagerDelegate::TabWatcher
    : public base::RefCountedThreadSafe<TabWatcher>,
      public content::NotificationObserver {
 public:
  typedef base::Callback<void(int render_process_id, int render_view_id)>
      TabClosedCallback;

  TabWatcher(TabClosedCallback tab_closed_callback,
             BrowserThread::ID callback_thread)
      : tab_closed_callback_(tab_closed_callback),
        callback_thread_(callback_thread) {
  }

  // Starts monitoring the WebContents corresponding to the given
  // |render_process_id|, |render_view_id| pair, invoking |tab_closed_callback_|
  // if closed/unloaded.
  void Watch(int render_process_id, int render_view_id) {
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
          &TabWatcher::Watch, this, render_process_id, render_view_id));
      return;
    }

    WebContents* web_contents = tab_util::GetWebContentsByID(render_process_id,
                                                             render_view_id);
    // Sessions initiated by speech input extension APIs will end up in a NULL
    // WebContent here, but they are properly managed by the
    // chrome::SpeechInputExtensionManager. However, sessions initiated within a
    // extension using the (new) speech JS APIs, will be properly handled here.
    // TODO(primiano) turn this line into a DCHECK once speech input extension
    // API is deprecated.
    if (!web_contents)
      return;

    // Avoid multiple registrations on |registrar_| for the same |web_contents|.
    if (registered_web_contents_.find(web_contents) !=
        registered_web_contents_.end()) {
      return;
    }
    registered_web_contents_.insert(web_contents);

    // Lazy initialize the registrar.
    if (!registrar_.get())
      registrar_.reset(new content::NotificationRegistrar());

    registrar_->Add(this,
                    content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
                    content::Source<WebContents>(web_contents));
  }

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK_EQ(content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED, type);

    WebContents* web_contents = content::Source<WebContents>(source).ptr();
    int render_process_id = web_contents->GetRenderProcessHost()->GetID();
    int render_view_id = web_contents->GetRenderViewHost()->GetRoutingID();

    registrar_->Remove(this,
                       content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
                       content::Source<WebContents>(web_contents));
    registered_web_contents_.erase(web_contents);

    BrowserThread::PostTask(callback_thread_, FROM_HERE, base::Bind(
        tab_closed_callback_, render_process_id, render_view_id));
  }

 private:
  friend class base::RefCountedThreadSafe<TabWatcher>;

  virtual ~TabWatcher() {
    // Must be destroyed on the UI thread due to |registrar_| non thread-safety.
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  }

  // Lazy-initialized and used on the UI thread to handle web contents
  // notifications (tab closing).
  scoped_ptr<content::NotificationRegistrar> registrar_;

  // Keeps track of which WebContent(s) have been registered, in order to avoid
  // double registrations on |registrar_|
  std::set<content::WebContents*> registered_web_contents_;

  // Callback used to notify, on the thread specified by |callback_thread_| the
  // closure of a registered tab.
  TabClosedCallback tab_closed_callback_;
  content::BrowserThread::ID callback_thread_;

  DISALLOW_COPY_AND_ASSIGN(TabWatcher);
};

ChromeSpeechRecognitionManagerDelegate
::ChromeSpeechRecognitionManagerDelegate() {
}

ChromeSpeechRecognitionManagerDelegate
::~ChromeSpeechRecognitionManagerDelegate() {
  if (tray_icon_controller_.get())
    tray_icon_controller_->Hide();
  if (bubble_controller_.get())
    bubble_controller_->CloseBubble();
}

void ChromeSpeechRecognitionManagerDelegate::InfoBubbleButtonClicked(
    int session_id, SpeechRecognitionBubble::Button button) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // Note, the session might have been destroyed, therefore avoid calls to the
  // manager which imply its existance (e.g., GetSessionContext()).

  if (button == SpeechRecognitionBubble::BUTTON_CANCEL) {
    GetBubbleController()->CloseBubble();
    last_session_config_.reset();

    // We can safely call AbortSession even if the session has already ended,
    // the manager's public methods are reliable and will handle it properly.
    SpeechRecognitionManager::GetInstance()->AbortSession(session_id);
  } else if (button == SpeechRecognitionBubble::BUTTON_TRY_AGAIN) {
    GetBubbleController()->CloseBubble();
    RestartLastSession();
  } else {
    NOTREACHED();
  }
}

void ChromeSpeechRecognitionManagerDelegate::InfoBubbleFocusChanged(
    int session_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // This check is needed since on some systems (MacOS), in rare cases, if the
  // user clicks repeatedly and fast on the input element, the FocusChanged
  // event (corresponding to the old session that should be aborted) can be
  // received after a new session (corresponding to the 2nd click) is started.
  if (GetBubbleController()->GetActiveSessionID() != session_id)
    return;

  // Note, the session might have been destroyed, therefore avoid calls to the
  // manager which imply its existance (e.g., GetSessionContext()).
  GetBubbleController()->CloseBubble();
  last_session_config_.reset();

  // Clicking outside the bubble means we should abort.
  SpeechRecognitionManager::GetInstance()->AbortSession(session_id);
}

void ChromeSpeechRecognitionManagerDelegate::RestartLastSession() {
  DCHECK(last_session_config_.get());
  SpeechRecognitionManager* manager = SpeechRecognitionManager::GetInstance();
  const int new_session_id = manager->CreateSession(*last_session_config_);
  DCHECK_NE(SpeechRecognitionManager::kSessionIDInvalid, new_session_id);
  last_session_config_.reset();
  manager->StartSession(new_session_id);
}

void ChromeSpeechRecognitionManagerDelegate::TabClosedCallback(
    int render_process_id, int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  SpeechRecognitionManager* manager = SpeechRecognitionManager::GetInstance();
  // |manager| becomes NULL if a browser shutdown happens between the post of
  // this task (from the UI thread) and this call (on the IO thread). In this
  // case we just return.
  if (!manager)
    return;

  manager->AbortAllSessionsForRenderView(render_process_id, render_view_id);

  if (bubble_controller_.get() &&
      bubble_controller_->IsShowingBubbleForRenderView(render_process_id,
                                                       render_view_id)) {
    bubble_controller_->CloseBubble();
  }
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
    GetBubbleController()->CreateBubble(session_id,
                                        context.render_process_id,
                                        context.render_view_id,
                                        context.element_rect);
  }

  // Register callback to auto abort session on tab closure.
  // |tab_watcher_| is lazyly istantiated on the first call.
  if (!tab_watcher_.get()) {
    tab_watcher_ = new TabWatcher(
        base::Bind(&ChromeSpeechRecognitionManagerDelegate::TabClosedCallback,
                   base::Unretained(this)),
        BrowserThread::IO);
  }
  tab_watcher_->Watch(context.render_process_id, context.render_view_id);
}

void ChromeSpeechRecognitionManagerDelegate::OnAudioStart(int session_id) {
  if (RequiresBubble(session_id)) {
    DCHECK_EQ(session_id, GetBubbleController()->GetActiveSessionID());
    GetBubbleController()->SetBubbleRecordingMode();
  } else if (RequiresTrayIcon(session_id)) {
    // We post the action to the UI thread for sessions requiring a tray icon.
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
  if (GetBubbleController()->GetActiveSessionID() == session_id) {
    DCHECK(RequiresBubble(session_id));
    GetBubbleController()->SetBubbleRecognizingMode();
  } else if (RequiresTrayIcon(session_id)) {
    GetTrayIconController()->Hide();
  }
}

void ChromeSpeechRecognitionManagerDelegate::OnRecognitionResults(
    int session_id, const content::SpeechRecognitionResults& result) {
  // The bubble will be closed upon the OnEnd event, which will follow soon.
}

void ChromeSpeechRecognitionManagerDelegate::OnRecognitionError(
    int session_id, const content::SpeechRecognitionError& error) {
  // An error can be dispatched when the bubble is not visible anymore.
  if (GetBubbleController()->GetActiveSessionID() != session_id)
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
      l10n_util::GetStringUTF16(error_message_id));
}

void ChromeSpeechRecognitionManagerDelegate::OnAudioLevelsChange(
    int session_id, float volume, float noise_volume) {
  if (GetBubbleController()->GetActiveSessionID() == session_id) {
    DCHECK(RequiresBubble(session_id));
    GetBubbleController()->SetBubbleInputVolume(volume, noise_volume);
  } else if (RequiresTrayIcon(session_id)) {
    GetTrayIconController()->SetVUMeterVolume(volume);
  }
}

void ChromeSpeechRecognitionManagerDelegate::OnRecognitionEnd(int session_id) {
  // The only case in which the OnRecognitionEnd should not close the bubble is
  // when we are showing an error. In this case the bubble will be closed by
  // the |InfoBubbleFocusChanged| method, when the users clicks either the
  // "Cancel" button or outside of the bubble.
  if (GetBubbleController()->GetActiveSessionID() == session_id &&
      !GetBubbleController()->IsShowingMessage()) {
    DCHECK(RequiresBubble(session_id));
    GetBubbleController()->CloseBubble();
  }
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
    base::Callback<void(bool ask_user, bool is_allowed)> callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  const content::SpeechRecognitionSessionContext& context =
      SpeechRecognitionManager::GetInstance()->GetSessionContext(session_id);

  // Make sure that initiators (extensions/web pages) properly set the
  // |render_process_id| field, which is needed later to retrieve the
  // ChromeSpeechRecognitionPreferences associated to their profile.
  DCHECK_NE(context.render_process_id, 0);

  // Check that the render view type is appropriate, and whether or not we
  // need to request permission from the user.
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&CheckRenderViewType,
                                     callback,
                                     context.render_process_id,
                                     context.render_view_id,
                                     RequiresTrayIcon(session_id)));
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
  // TODO(xians): clean up the code since we don't need to show the balloon
  // bubble any more.
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

  tray_icon_controller->Show(initiator_name);
}

void ChromeSpeechRecognitionManagerDelegate::CheckRenderViewType(
    base::Callback<void(bool ask_user, bool is_allowed)> callback,
    int render_process_id,
    int render_view_id,
    bool js_api) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const content::RenderViewHost* render_view_host =
      content::RenderViewHost::FromID(render_process_id, render_view_id);

  bool allowed = false;
  bool ask_permission = false;

  if (!render_view_host) {
    if (!js_api) {
      // If there is no render view, we cannot show the speech bubble, so this
      // is not allowed.
      allowed = false;
      ask_permission = false;
    } else {
      // This happens for extensions. Manifest should be checked for permission.
      allowed = true;
      ask_permission = false;
    }
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(callback, ask_permission, allowed));
    return;
  }

  WebContents* web_contents = WebContents::FromRenderViewHost(render_view_host);
  chrome::ViewType view_type = chrome::GetViewType(web_contents);

  if (view_type == chrome::VIEW_TYPE_TAB_CONTENTS ||
      web_contents->GetRenderProcessHost()->IsGuest()) {
    // If it is a tab, we can show the speech input bubble or ask for
    // permission.

    allowed = true;
    if (js_api)
      ask_permission = true;
  }

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(callback, ask_permission, allowed));
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
