// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/chrome_speech_recognition_manager_delegate.h"

#include <set>
#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/speech_recognition_manager.h"
#include "content/public/browser/speech_recognition_session_config.h"
#include "content/public/browser/speech_recognition_session_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/speech_recognition_error.h"
#include "content/public/common/speech_recognition_result.h"
#include "extensions/features/features.h"
#include "net/url_request/url_request_context_getter.h"

#if defined(OS_WIN)
#include "chrome/installer/util/wmi.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "extensions/browser/view_type_utils.h"
#endif

using content::BrowserThread;
using content::SpeechRecognitionManager;
using content::WebContents;

namespace speech {

namespace {

void TabClosedCallbackOnIOThread(int render_process_id, int render_view_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  SpeechRecognitionManager* manager = SpeechRecognitionManager::GetInstance();
  // |manager| becomes NULL if a browser shutdown happens between the post of
  // this task (from the UI thread) and this call (on the IO thread). In this
  // case we just return.
  if (!manager)
    return;

  manager->AbortAllSessionsForRenderView(render_process_id, render_view_id);
}

}  // namespace

// Simple utility to get notified when a WebContent (a tab or an extension's
// background page) is closed or crashes. The callback will always be called on
// the UI thread.
// There is no restriction on the constructor, however this class must be
// destroyed on the UI thread, due to the NotificationRegistrar dependency.
class ChromeSpeechRecognitionManagerDelegate::TabWatcher
    : public base::RefCountedThreadSafe<TabWatcher> {
 public:
  typedef base::Callback<void(int render_process_id, int render_view_id)>
      TabClosedCallback;

  explicit TabWatcher(TabClosedCallback tab_closed_callback)
      : tab_closed_callback_(tab_closed_callback) {
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

    // Avoid multiple registrations for the same |web_contents|.
    if (FindWebContents(web_contents) != registered_web_contents_.end())
      return;

    registered_web_contents_.push_back(new WebContentsTracker(
        web_contents, base::Bind(&TabWatcher::OnTabClosed,
                                 // |this| outlives WebContentsTracker.
                                 base::Unretained(this), web_contents),
        render_process_id, render_view_id));
  }

  void OnTabClosed(content::WebContents* web_contents) {
    ScopedVector<WebContentsTracker>::iterator iter =
        FindWebContents(web_contents);
    DCHECK(iter != registered_web_contents_.end());
    int render_process_id = (*iter)->render_process_id();
    int render_view_id = (*iter)->render_view_id();
    registered_web_contents_.erase(iter);

    tab_closed_callback_.Run(render_process_id, render_view_id);
  }

 private:
  class WebContentsTracker : public content::WebContentsObserver {
   public:
    WebContentsTracker(content::WebContents* web_contents,
                       const base::Closure& finished_callback,
                       int render_process_id,
                       int render_view_id)
        : content::WebContentsObserver(web_contents),
          web_contents_(web_contents),
          finished_callback_(finished_callback),
          render_process_id_(render_process_id),
          render_view_id_(render_view_id) {}

    ~WebContentsTracker() override {}

    int render_process_id() const { return render_process_id_; }
    int render_view_id() const { return render_view_id_; }
    const content::WebContents* GetWebContents() const { return web_contents_; }

   private:
    // content::WebContentsObserver overrides.
    void WebContentsDestroyed() override {
      Observe(nullptr);
      finished_callback_.Run();
      // NOTE: We are deleted now.
    }
    void RenderViewHostChanged(content::RenderViewHost* old_host,
                               content::RenderViewHost* new_host) override {
      Observe(nullptr);
      finished_callback_.Run();
      // NOTE: We are deleted now.
    }

    // Raw pointer to our WebContents.
    //
    // Although we are a WebContentsObserver, calling
    // WebContents::web_contents() would return NULL once we unregister
    // ourselves in WebContentsDestroyed() or RenderViewHostChanged(). So we
    // store a reference to perform cleanup.
    const content::WebContents* const web_contents_;
    const base::Closure finished_callback_;
    const int render_process_id_;
    const int render_view_id_;
  };

  friend class base::RefCountedThreadSafe<TabWatcher>;

  ~TabWatcher() {
    // Must be destroyed on the UI thread due to |registrar_| non thread-safety.
    // TODO(lazyboy): Do we still need this?
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
  }

  // Helper function to find the iterator in |registered_web_contents_| which
  // contains |web_contents|.
  ScopedVector<WebContentsTracker>::iterator FindWebContents(
      content::WebContents* web_contents) {
    for (ScopedVector<WebContentsTracker>::iterator i(
             registered_web_contents_.begin());
         i != registered_web_contents_.end(); ++i) {
      if ((*i)->GetWebContents() == web_contents)
        return i;
    }

    return registered_web_contents_.end();
  }

  // Keeps track of which WebContent(s) have been registered, in order to avoid
  // double registrations on WebContentsObserver and to pass the correct render
  // process id and render view id to |tab_closed_callback_| after the process
  // has gone away.
  ScopedVector<WebContentsTracker> registered_web_contents_;

  // Callback used to notify, on the thread specified by |callback_thread_| the
  // closure of a registered tab.
  TabClosedCallback tab_closed_callback_;

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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Tell the S.R. Manager (which lives on the IO thread) to abort all the
  // sessions for the given renderer view.
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, base::Bind(
      &TabClosedCallbackOnIOThread, render_process_id, render_view_id));
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
                   base::Unretained(this)));
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

void ChromeSpeechRecognitionManagerDelegate::CheckRecognitionIsAllowed(
    int session_id,
    base::Callback<void(bool ask_user, bool is_allowed)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const content::SpeechRecognitionSessionContext& context =
      SpeechRecognitionManager::GetInstance()->GetSessionContext(session_id);

  // Make sure that initiators (extensions/web pages) properly set the
  // |render_process_id| field, which is needed later to retrieve the profile.
  DCHECK_NE(context.render_process_id, 0);

  int render_process_id = context.render_process_id;
  int render_view_id = context.render_view_id;
  if (context.embedder_render_process_id) {
    // If this is a request originated from a guest, we need to re-route the
    // permission check through the embedder (app).
    render_process_id = context.embedder_render_process_id;
    render_view_id = context.embedder_render_view_id;
  }

  // Check that the render view type is appropriate, and whether or not we
  // need to request permission from the user.
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&CheckRenderViewType,
                                     callback,
                                     render_process_id,
                                     render_view_id));
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

// static.
void ChromeSpeechRecognitionManagerDelegate::CheckRenderViewType(
    base::Callback<void(bool ask_user, bool is_allowed)> callback,
    int render_process_id,
    int render_view_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::RenderViewHost* render_view_host =
      content::RenderViewHost::FromID(render_process_id, render_view_id);

  bool allowed = false;
  bool check_permission = false;

  if (!render_view_host) {
    // This happens for extensions. Manifest should be checked for permission.
    allowed = true;
    check_permission = false;
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(callback, check_permission, allowed));
    return;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  WebContents* web_contents = WebContents::FromRenderViewHost(render_view_host);
  extensions::ViewType view_type = extensions::GetViewType(web_contents);

  if (view_type == extensions::VIEW_TYPE_TAB_CONTENTS ||
      view_type == extensions::VIEW_TYPE_APP_WINDOW ||
      view_type == extensions::VIEW_TYPE_LAUNCHER_PAGE ||
      view_type == extensions::VIEW_TYPE_COMPONENT ||
      view_type == extensions::VIEW_TYPE_EXTENSION_POPUP ||
      view_type == extensions::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE) {
    // If it is a tab, we can check for permission. For apps, this means
    // manifest would be checked for permission.
    allowed = true;
    check_permission = true;
  }
#else
  // Otherwise this should be a regular tab contents.
  allowed = true;
  check_permission = true;
#endif

  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(callback, check_permission, allowed));
}

}  // namespace speech
