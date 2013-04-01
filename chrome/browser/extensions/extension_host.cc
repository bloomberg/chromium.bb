// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_host.h"

#include <list>

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_modal_dialogs/javascript_dialog_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/background_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/feature_switch.h"
#include "chrome/common/extensions/request_media_access_permission_helper.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "extensions/browser/view_type_utils.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using WebKit::WebDragOperation;
using WebKit::WebDragOperationsMask;
using content::NativeWebKeyboardEvent;
using content::OpenURLParams;
using content::RenderViewHost;
using content::SiteInstance;
using content::WebContents;

namespace extensions {

// Helper class that rate-limits the creation of renderer processes for
// ExtensionHosts, to avoid blocking the UI.
class ExtensionHost::ProcessCreationQueue {
 public:
  static ProcessCreationQueue* GetInstance() {
    return Singleton<ProcessCreationQueue>::get();
  }

  // Add a host to the queue for RenderView creation.
  void CreateSoon(ExtensionHost* host) {
    queue_.push_back(host);
    PostTask();
  }

  // Remove a host from the queue (in case it's being deleted).
  void Remove(ExtensionHost* host) {
    Queue::iterator it = std::find(queue_.begin(), queue_.end(), host);
    if (it != queue_.end())
      queue_.erase(it);
  }

 private:
  friend class Singleton<ProcessCreationQueue>;
  friend struct DefaultSingletonTraits<ProcessCreationQueue>;
  ProcessCreationQueue()
      : pending_create_(false),
        ALLOW_THIS_IN_INITIALIZER_LIST(ptr_factory_(this)) { }

  // Queue up a delayed task to process the next ExtensionHost in the queue.
  void PostTask() {
    if (!pending_create_) {
      MessageLoop::current()->PostTask(FROM_HERE,
          base::Bind(&ProcessCreationQueue::ProcessOneHost,
                     ptr_factory_.GetWeakPtr()));
      pending_create_ = true;
    }
  }

  // Create the RenderView for the next host in the queue.
  void ProcessOneHost() {
    pending_create_ = false;
    if (queue_.empty())
      return;  // can happen on shutdown

    queue_.front()->CreateRenderViewNow();
    queue_.pop_front();

    if (!queue_.empty())
      PostTask();
  }

  typedef std::list<ExtensionHost*> Queue;
  Queue queue_;
  bool pending_create_;
  base::WeakPtrFactory<ProcessCreationQueue> ptr_factory_;
};

////////////////
// ExtensionHost

ExtensionHost::ExtensionHost(const Extension* extension,
                             SiteInstance* site_instance,
                             const GURL& url,
                             ViewType host_type)
    : extension_(extension),
      extension_id_(extension->id()),
      profile_(Profile::FromBrowserContext(
          site_instance->GetBrowserContext())),
      render_view_host_(NULL),
      did_stop_loading_(false),
      document_element_available_(false),
      initial_url_(url),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          extension_function_dispatcher_(profile_, this)),
      extension_host_type_(host_type),
      associated_web_contents_(NULL) {
  host_contents_.reset(WebContents::Create(
      WebContents::CreateParams(profile_, site_instance))),
  content::WebContentsObserver::Observe(host_contents_.get());
  host_contents_->SetDelegate(this);
  SetViewType(host_contents_.get(), host_type);

  PrefsTabHelper::CreateForWebContents(host_contents());

  render_view_host_ = host_contents_->GetRenderViewHost();

  // Listen for when an extension is unloaded from the same profile, as it may
  // be the same extension that this points to.
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile_));
}

ExtensionHost::~ExtensionHost() {
  if (extension_host_type_ == VIEW_TYPE_EXTENSION_BACKGROUND_PAGE &&
      extension_ && BackgroundInfo::HasLazyBackgroundPage(extension_)) {
    UMA_HISTOGRAM_LONG_TIMES("Extensions.EventPageActiveTime",
                             since_created_.Elapsed());
  }
  // Clear the extension's renderer process out from any WebContents
  // that might have it in their back history.
  // TODO(jyasskin): Remove this hack by making extension reloading
  // wait for renderer shutdown.
  content::NotificationService::current()->Notify(
      content::NOTIFICATION_RENDERER_PROCESS_CLOSING,
      content::Source<content::RenderProcessHost>(render_process_host()),
      content::NotificationService::NoDetails());

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
      content::Source<Profile>(profile_),
      content::Details<ExtensionHost>(this));
  ProcessCreationQueue::GetInstance()->Remove(this);
}

void ExtensionHost::CreateView(Browser* browser) {
#if defined(TOOLKIT_VIEWS)
  view_.reset(new ExtensionViewViews(this, browser));
  // We own |view_|, so don't auto delete when it's removed from the view
  // hierarchy.
  view_->set_owned_by_client();
#elif defined(OS_MACOSX)
  view_.reset(new ExtensionViewMac(this, browser));
  view_->Init();
#elif defined(TOOLKIT_GTK)
  view_.reset(new ExtensionViewGtk(this, browser));
  view_->Init();
#else
  // TODO(port)
  NOTREACHED();
#endif
}

WebContents* ExtensionHost::GetAssociatedWebContents() const {
  return associated_web_contents_;
}

void ExtensionHost::SetAssociatedWebContents(
    content::WebContents* web_contents) {
  associated_web_contents_ = web_contents;
  if (web_contents) {
    registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                   content::Source<WebContents>(associated_web_contents_));
  }
}

content::RenderProcessHost* ExtensionHost::render_process_host() const {
  return render_view_host()->GetProcess();
}

RenderViewHost* ExtensionHost::render_view_host() const {
  // TODO(mpcomplete): This can be NULL. How do we handle that?
  return render_view_host_;
}

bool ExtensionHost::IsRenderViewLive() const {
  return render_view_host()->IsRenderViewLive();
}

void ExtensionHost::CreateRenderViewSoon() {
  if ((render_process_host() && render_process_host()->HasConnection())) {
    // If the process is already started, go ahead and initialize the RenderView
    // synchronously. The process creation is the real meaty part that we want
    // to defer.
    CreateRenderViewNow();
  } else {
    ProcessCreationQueue::GetInstance()->CreateSoon(this);
  }
}

void ExtensionHost::CreateRenderViewNow() {
  LoadInitialURL();
  if (is_background_page()) {
    DCHECK(IsRenderViewLive());
    ExtensionSystem::Get(profile_)->extension_service()->
        DidCreateRenderViewForBackgroundPage(this);
  }
}

WindowController* ExtensionHost::GetExtensionWindowController() const {
  return view() && view()->browser() ?
      view()->browser()->extension_window_controller() : NULL;
}

const GURL& ExtensionHost::GetURL() const {
  return host_contents()->GetURL();
}

void ExtensionHost::LoadInitialURL() {
  if (!is_background_page() &&
      !ExtensionSystem::Get(profile_)->extension_service()->
          IsBackgroundPageReady(extension_)) {
    // Make sure the background page loads before any others.
    registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,
                   content::Source<Extension>(extension_));
    return;
  }

  host_contents_->GetController().LoadURL(
      initial_url_, content::Referrer(), content::PAGE_TRANSITION_LINK,
      std::string());
}

void ExtensionHost::Close() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
      content::Source<Profile>(profile_),
      content::Details<ExtensionHost>(this));
}

void ExtensionHost::Observe(int type,
                            const content::NotificationSource& source,
                            const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY:
      DCHECK(ExtensionSystem::Get(profile_)->extension_service()->
          IsBackgroundPageReady(extension_));
      LoadInitialURL();
      break;
    case chrome::NOTIFICATION_EXTENSION_UNLOADED:
      // The extension object will be deleted after this notification has been
      // sent. NULL it out so that dirty pointer issues don't arise in cases
      // when multiple ExtensionHost objects pointing to the same Extension are
      // present.
      if (extension_ == content::Details<UnloadedExtensionInfo>(details)->
          extension) {
        extension_ = NULL;
      }
      break;
    case content::NOTIFICATION_WEB_CONTENTS_DESTROYED:
      if (content::Source<WebContents>(source).ptr() ==
          associated_web_contents_) {
        associated_web_contents_ = NULL;
      }
      break;
    default:
      NOTREACHED() << "Unexpected notification sent.";
      break;
  }
}

void ExtensionHost::ResizeDueToAutoResize(WebContents* source,
                                          const gfx::Size& new_size) {
  if (view())
    view()->ResizeDueToAutoResize(new_size);
}

void ExtensionHost::RenderViewGone(base::TerminationStatus status) {
  // During browser shutdown, we may use sudden termination on an extension
  // process, so it is expected to lose our connection to the render view.
  // Do nothing.
  if (browser_shutdown::GetShutdownType() != browser_shutdown::NOT_VALID)
    return;

  // In certain cases, multiple ExtensionHost objects may have pointed to
  // the same Extension at some point (one with a background page and a
  // popup, for example). When the first ExtensionHost goes away, the extension
  // is unloaded, and any other host that pointed to that extension will have
  // its pointer to it NULLed out so that any attempt to unload a dirty pointer
  // will be averted.
  if (!extension_)
    return;

  // TODO(aa): This is suspicious. There can be multiple views in an extension,
  // and they aren't all going to use ExtensionHost. This should be in someplace
  // more central, like EPM maybe.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_PROCESS_TERMINATED,
      content::Source<Profile>(profile_),
      content::Details<ExtensionHost>(this));
}

void ExtensionHost::InsertInfobarCSS() {
  DCHECK(!is_background_page());

  static const base::StringPiece css(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_EXTENSIONS_INFOBAR_CSS));

  render_view_host()->InsertCSS(string16(), css.as_string());
}

void ExtensionHost::DidStopLoading(content::RenderViewHost* render_view_host) {
  bool notify = !did_stop_loading_;
  did_stop_loading_ = true;
  if (extension_host_type_ == VIEW_TYPE_EXTENSION_POPUP ||
      extension_host_type_ == VIEW_TYPE_EXTENSION_DIALOG ||
      extension_host_type_ == VIEW_TYPE_EXTENSION_INFOBAR ||
      extension_host_type_ == VIEW_TYPE_PANEL) {
#if defined(TOOLKIT_VIEWS) || defined(OS_MACOSX)
    if (view())
      view()->DidStopLoading();
#endif
  }
  if (notify) {
    if (extension_host_type_ == VIEW_TYPE_EXTENSION_BACKGROUND_PAGE) {
      if (extension_ && BackgroundInfo::HasLazyBackgroundPage(extension_)) {
        UMA_HISTOGRAM_TIMES("Extensions.EventPageLoadTime",
                            since_created_.Elapsed());
      } else {
        UMA_HISTOGRAM_TIMES("Extensions.BackgroundPageLoadTime",
                            since_created_.Elapsed());
      }
    } else if (extension_host_type_ == VIEW_TYPE_EXTENSION_DIALOG) {
      UMA_HISTOGRAM_TIMES("Extensions.DialogLoadTime",
                          since_created_.Elapsed());
    } else if (extension_host_type_ == VIEW_TYPE_EXTENSION_POPUP) {
      UMA_HISTOGRAM_TIMES("Extensions.PopupLoadTime",
                          since_created_.Elapsed());
    } else if (extension_host_type_ == VIEW_TYPE_EXTENSION_INFOBAR) {
      UMA_HISTOGRAM_TIMES("Extensions.InfobarLoadTime",
        since_created_.Elapsed());
    } else if (extension_host_type_ == VIEW_TYPE_PANEL) {
      UMA_HISTOGRAM_TIMES("Extensions.PanelLoadTime", since_created_.Elapsed());
    }

    // Send the notification last, because it might result in this being
    // deleted.
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING,
        content::Source<Profile>(profile_),
        content::Details<ExtensionHost>(this));
  }
}

void ExtensionHost::DocumentAvailableInMainFrame() {
  // If the document has already been marked as available for this host, then
  // bail. No need for the redundant setup. http://crbug.com/31170
  if (document_element_available_)
    return;

  document_element_available_ = true;
  if (is_background_page()) {
    ExtensionSystem::Get(profile_)->extension_service()->
        SetBackgroundPageReady(extension_);
  } else {
    switch (extension_host_type_) {
      case VIEW_TYPE_EXTENSION_INFOBAR:
        InsertInfobarCSS();
        break;
      default:
        break;  // No style sheet for other types, at the moment.
    }
  }
}

void ExtensionHost::CloseContents(WebContents* contents) {
  // TODO(mpcomplete): is this check really necessary?
  if (extension_host_type_ == VIEW_TYPE_EXTENSION_POPUP ||
      extension_host_type_ == VIEW_TYPE_EXTENSION_DIALOG ||
      extension_host_type_ == VIEW_TYPE_EXTENSION_BACKGROUND_PAGE ||
      extension_host_type_ == VIEW_TYPE_EXTENSION_INFOBAR ||
      extension_host_type_ == VIEW_TYPE_PANEL) {
    Close();
  }
}

void ExtensionHost::WillRunJavaScriptDialog() {
  ExtensionProcessManager* pm =
      ExtensionSystem::Get(profile_)->process_manager();
  if (pm)
    pm->IncrementLazyKeepaliveCount(extension());
}

void ExtensionHost::DidCloseJavaScriptDialog() {
  ExtensionProcessManager* pm =
      ExtensionSystem::Get(profile_)->process_manager();
  if (pm)
    pm->DecrementLazyKeepaliveCount(extension());
}

WebContents* ExtensionHost::OpenURLFromTab(WebContents* source,
                                           const OpenURLParams& params) {
  // Whitelist the dispositions we will allow to be opened.
  switch (params.disposition) {
    case SINGLETON_TAB:
    case NEW_FOREGROUND_TAB:
    case NEW_BACKGROUND_TAB:
    case NEW_POPUP:
    case NEW_WINDOW:
    case SAVE_TO_DISK:
    case OFF_THE_RECORD: {
      // Only allow these from hosts that are bound to a browser (e.g. popups).
      // Otherwise they are not driven by a user gesture.
      Browser* browser = view() ? view()->browser() : NULL;
      return browser ? browser->OpenURL(params) : NULL;
    }
    default:
      return NULL;
  }
}

bool ExtensionHost::PreHandleKeyboardEvent(WebContents* source,
                                           const NativeWebKeyboardEvent& event,
                                           bool* is_keyboard_shortcut) {
  if (extension_host_type_ == VIEW_TYPE_EXTENSION_POPUP &&
      event.type == NativeWebKeyboardEvent::RawKeyDown &&
      event.windowsKeyCode == ui::VKEY_ESCAPE) {
    DCHECK(is_keyboard_shortcut != NULL);
    *is_keyboard_shortcut = true;
    return false;
  }

  // Handle higher priority browser shortcuts such as Ctrl-w.
  Browser* browser = view() ? view()->browser() : NULL;
  if (browser)
    return browser->PreHandleKeyboardEvent(source, event, is_keyboard_shortcut);

  *is_keyboard_shortcut = false;
  return false;
}

void ExtensionHost::HandleKeyboardEvent(WebContents* source,
                                        const NativeWebKeyboardEvent& event) {
  if (extension_host_type_ == VIEW_TYPE_EXTENSION_POPUP) {
    if (event.type == NativeWebKeyboardEvent::RawKeyDown &&
        event.windowsKeyCode == ui::VKEY_ESCAPE) {
      Close();
      return;
    }
  }
  UnhandledKeyboardEvent(source, event);
}

bool ExtensionHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionHost, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_Request, OnRequest)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_EventAck, OnEventAck)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_IncrementLazyKeepaliveCount,
                        OnIncrementLazyKeepaliveCount)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_DecrementLazyKeepaliveCount,
                        OnDecrementLazyKeepaliveCount)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExtensionHost::OnRequest(const ExtensionHostMsg_Request_Params& params) {
  extension_function_dispatcher_.Dispatch(params, render_view_host());
}

void ExtensionHost::OnEventAck() {
  EventRouter* router = ExtensionSystem::Get(profile_)->event_router();
  if (router)
    router->OnEventAck(profile_, extension_id());
}

void ExtensionHost::OnIncrementLazyKeepaliveCount() {
  ExtensionProcessManager* pm =
      ExtensionSystem::Get(profile_)->process_manager();
  if (pm)
    pm->IncrementLazyKeepaliveCount(extension());
}

void ExtensionHost::OnDecrementLazyKeepaliveCount() {
  ExtensionProcessManager* pm =
      ExtensionSystem::Get(profile_)->process_manager();
  if (pm)
    pm->DecrementLazyKeepaliveCount(extension());
}

void ExtensionHost::UnhandledKeyboardEvent(
    WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  Browser* browser = view() ? view()->browser() : NULL;
  if (browser) {
    // Handle lower priority browser shortcuts such as Ctrl-f.
    return browser->HandleKeyboardEvent(source, event);
  } else {
#if defined(TOOLKIT_VIEWS)
    // In case there's no Browser (e.g. for dialogs), pass it to
    // ExtensionViewViews to handle acceleratos. The view's FocusManager does
    // not know anything about Browser accelerators, but might know others such
    // as Ash's.
    if (view())
      view()->HandleKeyboardEvent(event);
#endif
  }
}

void ExtensionHost::RenderViewCreated(RenderViewHost* render_view_host) {
  render_view_host_ = render_view_host;

  if (view())
    view()->RenderViewCreated();

  // If the host is bound to a window, then extract its id. Extensions hosted
  // in ExternalTabContainer objects may not have an associated window.
  WindowController* window = GetExtensionWindowController();
  if (window) {
    render_view_host->Send(new ExtensionMsg_UpdateBrowserWindowId(
        render_view_host->GetRoutingID(), window->GetWindowId()));
  }
}

void ExtensionHost::RenderViewDeleted(RenderViewHost* render_view_host) {
  // If our RenderViewHost is deleted, fall back to the host_contents' current
  // RVH. There is sometimes a small gap between the pending RVH being deleted
  // and RenderViewCreated being called, so we update it here.
  if (render_view_host == render_view_host_)
    render_view_host_ = host_contents_->GetRenderViewHost();
}

content::JavaScriptDialogManager* ExtensionHost::GetJavaScriptDialogManager() {
  if (!dialog_manager_.get()) {
    dialog_manager_.reset(CreateJavaScriptDialogManagerInstance(this));
  }
  return dialog_manager_.get();
}

void ExtensionHost::RunFileChooser(WebContents* tab,
                                   const content::FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(tab, params);
}

void ExtensionHost::AddNewContents(WebContents* source,
                                   WebContents* new_contents,
                                   WindowOpenDisposition disposition,
                                   const gfx::Rect& initial_pos,
                                   bool user_gesture,
                                   bool* was_blocked) {
  // First, if the creating extension view was associated with a tab contents,
  // use that tab content's delegate. We must be careful here that the
  // associated tab contents has the same profile as the new tab contents. In
  // the case of extensions in 'spanning' incognito mode, they can mismatch.
  // We don't want to end up putting a normal tab into an incognito window, or
  // vice versa.
  // Note that we don't do this for popup windows, because we need to associate
  // those with their extension_app_id.
  if (disposition != NEW_POPUP) {
    WebContents* associated_contents = GetAssociatedWebContents();
    if (associated_contents &&
        associated_contents->GetBrowserContext() ==
            new_contents->GetBrowserContext()) {
      WebContentsDelegate* delegate = associated_contents->GetDelegate();
      if (delegate) {
        delegate->AddNewContents(
            associated_contents, new_contents, disposition, initial_pos,
            user_gesture, was_blocked);
        return;
      }
    }
  }

  ExtensionTabUtil::CreateTab(new_contents, extension_id_, disposition,
                              initial_pos, user_gesture);
}

void ExtensionHost::RenderViewReady() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_HOST_CREATED,
      content::Source<Profile>(profile_),
      content::Details<ExtensionHost>(this));
}

void ExtensionHost::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback) {
  // Get the preferred default devices for the request.
  content::MediaStreamDevices devices;
  MediaCaptureDevicesDispatcher::GetInstance()->GetDefaultDevicesForProfile(
      profile_,
      content::IsAudioMediaType(request.audio_type),
      content::IsVideoMediaType(request.video_type),
      &devices);

  // For tab capture device, we require the tabCapture permission.
  RequestMediaAccessPermissionHelper::AuthorizeRequest(
      devices, request, callback, extension(), false);
}

}  // namespace extensions
