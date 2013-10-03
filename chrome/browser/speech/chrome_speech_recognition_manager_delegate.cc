// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/chrome_speech_recognition_manager_delegate.h"

#include <set>
#include <string>

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/tab_contents/tab_util.h"
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
#include "extensions/browser/view_type_utils.h"
#include "net/url_request/url_request_context_getter.h"

#if defined(OS_WIN)
#include "chrome/installer/util/wmi.h"
#endif

using content::BrowserThread;
using content::SpeechRecognitionManager;
using content::WebContents;

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
    // prefs::kMetricsReportingEnabled is not registered for OS_CHROMEOS.
#if !defined(OS_CHROMEOS)
    if (g_browser_process->local_state()->GetBoolean(
        prefs::kMetricsReportingEnabled)) {
      // Access potentially slow OS calls from the FILE thread.
      BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
          base::Bind(&OptionalRequestInfo::GetHardwareInfo, this));
    }
#endif
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
    if (FindWebContents(web_contents) !=  registered_web_contents_.end()) {
      return;
    }
    registered_web_contents_.push_back(
        WebContentsInfo(web_contents, render_process_id, render_view_id));

    // Lazy initialize the registrar.
    if (!registrar_.get())
      registrar_.reset(new content::NotificationRegistrar());

    registrar_->Add(this,
                    content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
                    content::Source<WebContents>(web_contents));
    registrar_->Add(this,
                    content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
                    content::Source<WebContents>(web_contents));
  }

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DCHECK(type == content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED ||
           type == content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED);

    WebContents* web_contents = content::Source<WebContents>(source).ptr();
    std::vector<WebContentsInfo>::iterator iter = FindWebContents(web_contents);
    DCHECK(iter != registered_web_contents_.end());
    int render_process_id = iter->render_process_id;
    int render_view_id = iter->render_view_id;
    registered_web_contents_.erase(iter);

    registrar_->Remove(this,
                       content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
                       content::Source<WebContents>(web_contents));
    registrar_->Remove(this,
                       content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
                       content::Source<WebContents>(web_contents));

    BrowserThread::PostTask(callback_thread_, FROM_HERE, base::Bind(
        tab_closed_callback_, render_process_id, render_view_id));
  }

 private:
  struct WebContentsInfo {
    WebContentsInfo(content::WebContents* web_contents,
                    int render_process_id,
                    int render_view_id)
        : web_contents(web_contents),
          render_process_id(render_process_id),
          render_view_id(render_view_id) {}

    ~WebContentsInfo() {}

    content::WebContents* web_contents;
    int render_process_id;
    int render_view_id;
  };

  friend class base::RefCountedThreadSafe<TabWatcher>;

  virtual ~TabWatcher() {
    // Must be destroyed on the UI thread due to |registrar_| non thread-safety.
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  }

  // Helper function to find the iterator in |registered_web_contents_| which
  // contains |web_contents|.
  std::vector<WebContentsInfo>::iterator FindWebContents(
      content::WebContents* web_contents) {
    for (std::vector<WebContentsInfo>::iterator i(
         registered_web_contents_.begin());
         i != registered_web_contents_.end(); ++i) {
      if (i->web_contents == web_contents)
        return i;
    }

    return registered_web_contents_.end();
  }

  // Lazy-initialized and used on the UI thread to handle web contents
  // notifications (tab closing).
  scoped_ptr<content::NotificationRegistrar> registrar_;

  // Keeps track of which WebContent(s) have been registered, in order to avoid
  // double registrations on |registrar_| and to pass the correct render
  // process id and render view id to |tab_closed_callback_| after the process
  // has gone away.
  std::vector<WebContentsInfo> registered_web_contents_;

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
}

void ChromeSpeechRecognitionManagerDelegate::OnRecognitionStart(
    int session_id) {
  const content::SpeechRecognitionSessionContext& context =
      SpeechRecognitionManager::GetInstance()->GetSessionContext(session_id);

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
}

void ChromeSpeechRecognitionManagerDelegate::OnEnvironmentEstimationComplete(
    int session_id) {
}

void ChromeSpeechRecognitionManagerDelegate::OnSoundStart(int session_id) {
}

void ChromeSpeechRecognitionManagerDelegate::OnSoundEnd(int session_id) {
}

void ChromeSpeechRecognitionManagerDelegate::OnAudioEnd(int session_id) {
}

void ChromeSpeechRecognitionManagerDelegate::OnRecognitionResults(
    int session_id, const content::SpeechRecognitionResults& result) {
}

void ChromeSpeechRecognitionManagerDelegate::OnRecognitionError(
    int session_id, const content::SpeechRecognitionError& error) {
}

void ChromeSpeechRecognitionManagerDelegate::OnAudioLevelsChange(
    int session_id, float volume, float noise_volume) {
}

void ChromeSpeechRecognitionManagerDelegate::OnRecognitionEnd(int session_id) {
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
  // |render_process_id| field, which is needed later to retrieve the profile.
  DCHECK_NE(context.render_process_id, 0);

  // Check that the render view type is appropriate, and whether or not we
  // need to request permission from the user.
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&CheckRenderViewType,
                                     callback,
                                     context.render_process_id,
                                     context.render_view_id,
                                     !context.requested_by_page_element));
}

content::SpeechRecognitionEventListener*
ChromeSpeechRecognitionManagerDelegate::GetEventListener() {
  return this;
}

bool ChromeSpeechRecognitionManagerDelegate::FilterProfanities(
    int render_process_id) {
  content::RenderProcessHost* rph =
      content::RenderProcessHost::FromID(render_process_id);
  if (!rph)  // Guard against race conditions on RPH lifetime.
    return true;

  return Profile::FromBrowserContext(rph->GetBrowserContext())->GetPrefs()->
      GetBoolean(prefs::kSpeechRecognitionFilterProfanities);
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
  bool check_permission = false;

  if (!render_view_host) {
    if (!js_api) {
      // If there is no render view, we cannot show the speech bubble, so this
      // is not allowed.
      allowed = false;
      check_permission = false;
    } else {
      // This happens for extensions. Manifest should be checked for permission.
      allowed = true;
      check_permission = false;
    }
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(callback, check_permission, allowed));
    return;
  }

  WebContents* web_contents = WebContents::FromRenderViewHost(render_view_host);
  extensions::ViewType view_type = extensions::GetViewType(web_contents);

  // TODO(kalman): Also enable speech bubble for extension popups
  // (VIEW_TYPE_EXTENSION_POPUP) once popup-like control UI works properly in
  // extensions: http://crbug.com/163851.
  // Right now the extension popup closes and dismisses immediately on user
  // click.
  if (view_type == extensions::VIEW_TYPE_TAB_CONTENTS ||
      view_type == extensions::VIEW_TYPE_APP_SHELL ||
      view_type == extensions::VIEW_TYPE_VIRTUAL_KEYBOARD ||
      // Only allow requests through JavaScript API (|js_api| = true).
      // Requests originating from html element (|js_api| = false) would want
      // to show bubble which isn't quite intuitive from a background page. Also
      // see todo above about issues with rendering such bubbles from extension
      // popups.
      (view_type == extensions::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE &&
           js_api)) {
    // If it is a tab, we can show the speech input bubble or check for
    // permission. For apps, this means manifest would be checked for
    // permission.

    allowed = true;
    if (js_api)
      check_permission = true;
  }

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(callback, check_permission, allowed));
}

}  // namespace speech
