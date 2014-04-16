// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_host.h"

#include <list>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_error.h"
#include "extensions/browser/extension_host_delegate.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/runtime_data.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"

using content::BrowserContext;
using content::OpenURLParams;
using content::RenderProcessHost;
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
        ptr_factory_(this) {}

  // Queue up a delayed task to process the next ExtensionHost in the queue.
  void PostTask() {
    if (!pending_create_) {
      base::MessageLoop::current()->PostTask(FROM_HERE,
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
    : delegate_(ExtensionsBrowserClient::Get()->CreateExtensionHostDelegate()),
      extension_(extension),
      extension_id_(extension->id()),
      browser_context_(site_instance->GetBrowserContext()),
      render_view_host_(NULL),
      did_stop_loading_(false),
      document_element_available_(false),
      initial_url_(url),
      extension_function_dispatcher_(browser_context_, this),
      extension_host_type_(host_type) {
  // Not used for panels, see PanelHost.
  DCHECK(host_type == VIEW_TYPE_EXTENSION_BACKGROUND_PAGE ||
         host_type == VIEW_TYPE_EXTENSION_DIALOG ||
         host_type == VIEW_TYPE_EXTENSION_INFOBAR ||
         host_type == VIEW_TYPE_EXTENSION_POPUP);
  host_contents_.reset(WebContents::Create(
      WebContents::CreateParams(browser_context_, site_instance))),
  content::WebContentsObserver::Observe(host_contents_.get());
  host_contents_->SetDelegate(this);
  SetViewType(host_contents_.get(), host_type);

  render_view_host_ = host_contents_->GetRenderViewHost();

  // Listen for when an extension is unloaded from the same profile, as it may
  // be the same extension that this points to.
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED,
                 content::Source<BrowserContext>(browser_context_));

  // Set up web contents observers and pref observers.
  delegate_->OnExtensionHostCreated(host_contents());
}

ExtensionHost::~ExtensionHost() {
  if (extension_host_type_ == VIEW_TYPE_EXTENSION_BACKGROUND_PAGE &&
      extension_ && BackgroundInfo::HasLazyBackgroundPage(extension_)) {
    UMA_HISTOGRAM_LONG_TIMES("Extensions.EventPageActiveTime",
                             since_created_.Elapsed());
  }
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
      content::Source<BrowserContext>(browser_context_),
      content::Details<ExtensionHost>(this));
  ProcessCreationQueue::GetInstance()->Remove(this);
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
  if (IsBackgroundPage()) {
    DCHECK(IsRenderViewLive());
    // Connect orphaned dev-tools instances.
    delegate_->OnRenderViewCreatedForBackgroundPage(this);
  }
}

const GURL& ExtensionHost::GetURL() const {
  return host_contents()->GetURL();
}

void ExtensionHost::LoadInitialURL() {
  host_contents_->GetController().LoadURL(
      initial_url_, content::Referrer(), content::PAGE_TRANSITION_LINK,
      std::string());
}

bool ExtensionHost::IsBackgroundPage() const {
  DCHECK(extension_host_type_ == VIEW_TYPE_EXTENSION_BACKGROUND_PAGE);
  return true;
}

void ExtensionHost::Close() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
      content::Source<BrowserContext>(browser_context_),
      content::Details<ExtensionHost>(this));
}

void ExtensionHost::Observe(int type,
                            const content::NotificationSource& source,
                            const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED:
      // The extension object will be deleted after this notification has been
      // sent. NULL it out so that dirty pointer issues don't arise in cases
      // when multiple ExtensionHost objects pointing to the same Extension are
      // present.
      if (extension_ == content::Details<UnloadedExtensionInfo>(details)->
          extension) {
        extension_ = NULL;
      }
      break;
    default:
      NOTREACHED() << "Unexpected notification sent.";
      break;
  }
}

void ExtensionHost::RenderProcessGone(base::TerminationStatus status) {
  // During browser shutdown, we may use sudden termination on an extension
  // process, so it is expected to lose our connection to the render view.
  // Do nothing.
  RenderProcessHost* process_host = host_contents_->GetRenderProcessHost();
  if (process_host && process_host->FastShutdownStarted())
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
      content::Source<BrowserContext>(browser_context_),
      content::Details<ExtensionHost>(this));
}

void ExtensionHost::DidStopLoading(content::RenderViewHost* render_view_host) {
  bool notify = !did_stop_loading_;
  did_stop_loading_ = true;
  OnDidStopLoading();
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
    }

    // Send the notification last, because it might result in this being
    // deleted.
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING,
        content::Source<BrowserContext>(browser_context_),
        content::Details<ExtensionHost>(this));
  }
}

void ExtensionHost::OnDidStopLoading() {
  DCHECK(extension_host_type_ == VIEW_TYPE_EXTENSION_BACKGROUND_PAGE);
  // Nothing to do for background pages.
}

void ExtensionHost::DocumentAvailableInMainFrame() {
  // If the document has already been marked as available for this host, then
  // bail. No need for the redundant setup. http://crbug.com/31170
  if (document_element_available_)
    return;
  document_element_available_ = true;
  OnDocumentAvailable();
}

void ExtensionHost::OnDocumentAvailable() {
  DCHECK(extension_host_type_ == VIEW_TYPE_EXTENSION_BACKGROUND_PAGE);
  ExtensionSystem::Get(browser_context_)
      ->runtime_data()
      ->SetBackgroundPageReady(extension_, true);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,
      content::Source<const Extension>(extension_),
      content::NotificationService::NoDetails());
}

void ExtensionHost::CloseContents(WebContents* contents) {
  Close();
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
  EventRouter* router = EventRouter::Get(browser_context_);
  if (router)
    router->OnEventAck(browser_context_, extension_id());
}

void ExtensionHost::OnIncrementLazyKeepaliveCount() {
  ProcessManager* pm = ExtensionSystem::Get(
      browser_context_)->process_manager();
  if (pm)
    pm->IncrementLazyKeepaliveCount(extension());
}

void ExtensionHost::OnDecrementLazyKeepaliveCount() {
  ProcessManager* pm = ExtensionSystem::Get(
      browser_context_)->process_manager();
  if (pm)
    pm->DecrementLazyKeepaliveCount(extension());
}

// content::WebContentsObserver

void ExtensionHost::RenderViewCreated(RenderViewHost* render_view_host) {
  render_view_host_ = render_view_host;
}

void ExtensionHost::RenderViewDeleted(RenderViewHost* render_view_host) {
  // If our RenderViewHost is deleted, fall back to the host_contents' current
  // RVH. There is sometimes a small gap between the pending RVH being deleted
  // and RenderViewCreated being called, so we update it here.
  if (render_view_host == render_view_host_)
    render_view_host_ = host_contents_->GetRenderViewHost();
}

content::JavaScriptDialogManager* ExtensionHost::GetJavaScriptDialogManager() {
  return delegate_->GetJavaScriptDialogManager();
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

  delegate_->CreateTab(
      new_contents, extension_id_, disposition, initial_pos, user_gesture);
}

void ExtensionHost::RenderViewReady() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_HOST_CREATED,
      content::Source<BrowserContext>(browser_context_),
      content::Details<ExtensionHost>(this));
}

void ExtensionHost::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback) {
  delegate_->ProcessMediaAccessRequest(
      web_contents, request, callback, extension());
}

bool ExtensionHost::IsNeverVisible(content::WebContents* web_contents) {
  ViewType view_type = extensions::GetViewType(web_contents);
  return view_type == extensions::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE;
}

}  // namespace extensions
