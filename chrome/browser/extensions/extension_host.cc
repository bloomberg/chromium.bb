// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_modal_dialogs/message_box_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_view_type.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browsing_instance.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/views/widget/widget.h"
#endif

using WebKit::WebDragOperation;
using WebKit::WebDragOperationsMask;

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
                             content::ViewType host_type)
    : extension_(extension),
      extension_id_(extension->id()),
      profile_(Profile::FromBrowserContext(
          site_instance->browsing_instance()->browser_context())),
      did_stop_loading_(false),
      document_element_available_(false),
      initial_url_(url),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          extension_function_dispatcher_(profile_, this)),
      extension_host_type_(host_type),
      associated_tab_contents_(NULL) {
  host_contents_.reset(new TabContents(
      profile_, site_instance, MSG_ROUTING_NONE, NULL, NULL));
  TabContentsObserver::Observe(host_contents_.get());
  host_contents_->set_delegate(this);
  host_contents_->set_view_type(host_type);

  // Listen for when an extension is unloaded from the same profile, as it may
  // be the same extension that this points to.
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile_));
}

// This "mock" constructor should only be used by unit tests.
ExtensionHost::ExtensionHost(const Extension* extension,
                             content::ViewType host_type)
    : extension_(extension),
      extension_id_(extension->id()),
      profile_(NULL),
      did_stop_loading_(false),
      document_element_available_(false),
      initial_url_(GURL()),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          extension_function_dispatcher_(profile_, this)),
      extension_host_type_(host_type),
      associated_tab_contents_(NULL) {
}

ExtensionHost::~ExtensionHost() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
      content::Source<Profile>(profile_),
      content::Details<ExtensionHost>(this));
  ProcessCreationQueue::GetInstance()->Remove(this);
}

void ExtensionHost::CreateView(Browser* browser) {
#if defined(TOOLKIT_VIEWS)
  view_.reset(new ExtensionView(this, browser));
  // We own |view_|, so don't auto delete when it's removed from the view
  // hierarchy.
  view_->set_parent_owned(false);
#elif defined(OS_MACOSX)
  view_.reset(new ExtensionViewMac(this, browser));
  view_->Init();
#elif defined(TOOLKIT_USES_GTK)
  view_.reset(new ExtensionViewGtk(this, browser));
  view_->Init();
#else
  // TODO(port)
  NOTREACHED();
#endif
}

TabContents* ExtensionHost::GetAssociatedTabContents() const {
  return associated_tab_contents_;
}

content::RenderProcessHost* ExtensionHost::render_process_host() const {
  return host_contents()->GetRenderProcessHost();
}

RenderViewHost* ExtensionHost::render_view_host() const {
  // TODO(mpcomplete): This can be NULL. How do we handle that?
  return host_contents()->render_view_host();
}

bool ExtensionHost::IsRenderViewLive() const {
  return render_view_host()->IsRenderViewLive();
}

void ExtensionHost::CreateRenderViewSoon() {
  if (render_process_host()->HasConnection()) {
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
    profile_->GetExtensionService()->DidCreateRenderViewForBackgroundPage(this);
  }
}

const Browser* ExtensionHost::GetBrowser() const {
  return view() ? view()->browser() : NULL;
}

Browser* ExtensionHost::GetBrowser() {
  return view() ? view()->browser() : NULL;
}

gfx::NativeView ExtensionHost::GetNativeViewOfHost() {
  return view() ? view()->native_view() : NULL;
}

const GURL& ExtensionHost::GetURL() const {
  return host_contents()->GetURL();
}

void ExtensionHost::LoadInitialURL() {
  if (!is_background_page() &&
      !profile_->GetExtensionService()->IsBackgroundPageReady(extension_)) {
    // Make sure the background page loads before any others.
    registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,
                   content::Source<Extension>(extension_));
    return;
  }

  host_contents_->controller().LoadURL(
      initial_url_, GURL(), content::PAGE_TRANSITION_LINK, std::string());
}

void ExtensionHost::Observe(int type,
                            const content::NotificationSource& source,
                            const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY:
      DCHECK(profile_->GetExtensionService()->
          IsBackgroundPageReady(extension_));
      LoadInitialURL();
      break;
    case chrome::NOTIFICATION_EXTENSION_UNLOADED:
      // The extension object will be deleted after this notification has been
      // sent. NULL it out so that dirty pointer issues don't arise in cases
      // when multiple ExtensionHost objects pointing to the same Extension are
      // present.
      if (extension_ ==
          content::Details<UnloadedExtensionInfo>(details)->extension) {
        extension_ = NULL;
      }
      break;
    default:
      NOTREACHED() << "Unexpected notification sent.";
      break;
  }
}

void ExtensionHost::UpdatePreferredSize(TabContents* source,
                                        const gfx::Size& pref_size) {
  if (view_.get())
    view_->UpdatePreferredSize(pref_size);
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

void ExtensionHost::DisableScrollbarsForSmallWindows(
    const gfx::Size& size_limit) {
  render_view_host()->DisableScrollbarsForThreshold(size_limit);
}

void ExtensionHost::DidStopLoading() {
  bool notify = !did_stop_loading_;
  did_stop_loading_ = true;
  if (extension_host_type_ == chrome::VIEW_TYPE_EXTENSION_POPUP ||
      extension_host_type_ == chrome::VIEW_TYPE_EXTENSION_DIALOG ||
      extension_host_type_ == chrome::VIEW_TYPE_EXTENSION_INFOBAR) {
#if defined(TOOLKIT_VIEWS) || defined(OS_MACOSX)
    if (view_.get())
      view_->DidStopLoading();
#endif
  }
  if (notify) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING,
        content::Source<Profile>(profile_),
        content::Details<ExtensionHost>(this));
    if (extension_host_type_ == chrome::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE) {
      UMA_HISTOGRAM_TIMES("Extensions.BackgroundPageLoadTime",
                          since_created_.Elapsed());
    } else if (extension_host_type_ == chrome::VIEW_TYPE_EXTENSION_DIALOG) {
      UMA_HISTOGRAM_TIMES("Extensions.DialogLoadTime",
                          since_created_.Elapsed());
    } else if (extension_host_type_ == chrome::VIEW_TYPE_EXTENSION_POPUP) {
      UMA_HISTOGRAM_TIMES("Extensions.PopupLoadTime",
                          since_created_.Elapsed());
    } else if (extension_host_type_ == chrome::VIEW_TYPE_EXTENSION_INFOBAR) {
      UMA_HISTOGRAM_TIMES("Extensions.InfobarLoadTime",
        since_created_.Elapsed());
    }
  }
}

void ExtensionHost::DocumentAvailableInMainFrame() {
  // If the document has already been marked as available for this host, then
  // bail. No need for the redundant setup. http://crbug.com/31170
  if (document_element_available_)
    return;

  document_element_available_ = true;
  if (is_background_page()) {
    profile_->GetExtensionService()->SetBackgroundPageReady(extension_);
  } else {
    switch (extension_host_type_) {
      case chrome::VIEW_TYPE_EXTENSION_INFOBAR:
        InsertInfobarCSS();
        break;
      default:
        break;  // No style sheet for other types, at the moment.
    }
  }
}

void ExtensionHost::CloseContents(TabContents* contents) {
  if (extension_host_type_ == chrome::VIEW_TYPE_EXTENSION_POPUP ||
      extension_host_type_ == chrome::VIEW_TYPE_EXTENSION_DIALOG ||
      extension_host_type_ == chrome::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE ||
      extension_host_type_ == chrome::VIEW_TYPE_EXTENSION_INFOBAR) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
        content::Source<Profile>(profile_),
        content::Details<ExtensionHost>(this));
  }
}

bool ExtensionHost::PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                           bool* is_keyboard_shortcut) {
  if (extension_host_type_ == chrome::VIEW_TYPE_EXTENSION_POPUP &&
      event.type == NativeWebKeyboardEvent::RawKeyDown &&
      event.windowsKeyCode == ui::VKEY_ESCAPE) {
    DCHECK(is_keyboard_shortcut != NULL);
    *is_keyboard_shortcut = true;
  }
  return false;
}

void ExtensionHost::HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {
  if (extension_host_type_ == chrome::VIEW_TYPE_EXTENSION_POPUP) {
    if (event.type == NativeWebKeyboardEvent::RawKeyDown &&
        event.windowsKeyCode == ui::VKEY_ESCAPE) {
      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
          content::Source<Profile>(profile_),
          content::Details<ExtensionHost>(this));
      return;
    }
  }
  UnhandledKeyboardEvent(event);
}

bool ExtensionHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ExtensionHost, message)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_Request, OnRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExtensionHost::OnRequest(const ExtensionHostMsg_Request_Params& params) {
  extension_function_dispatcher_.Dispatch(params, render_view_host());
}

void ExtensionHost::RenderViewCreated(RenderViewHost* render_view_host) {
  if (view_.get())
    view_->RenderViewCreated();

  if (extension_host_type_ == chrome::VIEW_TYPE_EXTENSION_POPUP ||
      extension_host_type_ == chrome::VIEW_TYPE_EXTENSION_INFOBAR) {
    render_view_host->EnablePreferredSizeMode();
  }

  // If the host is bound to a browser, then extract its window id.
  // Extensions hosted in ExternalTabContainer objects may not have
  // an associated browser.
  const Browser* browser = GetBrowser();
  if (browser) {
    render_view_host->Send(new ExtensionMsg_UpdateBrowserWindowId(
        render_view_host->routing_id(),
        ExtensionTabUtil::GetWindowId(browser)));
  }
}

content::JavaScriptDialogCreator* ExtensionHost::GetJavaScriptDialogCreator() {
  return GetJavaScriptDialogCreatorInstance();
}

void ExtensionHost::AddNewContents(TabContents* source,
                                   TabContents* new_contents,
                                   WindowOpenDisposition disposition,
                                   const gfx::Rect& initial_pos,
                                   bool user_gesture) {
  // TODO(mpcomplete): is all this necessary? Maybe we can just call the
  // brower's delegate, and fall back to browser::Navigate if browser is NULL.
  TabContents* contents = new_contents;
  if (!contents)
    return;
  Profile* profile = Profile::FromBrowserContext(contents->browser_context());

  if (disposition == NEW_POPUP) {
    // Find a browser with a matching profile for creating a popup.
    // (If none is found, NULL argument to NavigateParams is valid.)
    Browser* browser = BrowserList::FindTabbedBrowser(
        profile, false);  // Match incognito exactly.
    TabContentsWrapper* wrapper = new TabContentsWrapper(contents);
    browser::NavigateParams params(browser, wrapper);
    if (!browser)
      params.profile = profile;
    // The extension_app_id parameter ends up as app_name in the Browser
    // which causes the Browser to return true for is_app().  This affects
    // among other things, whether the location bar gets displayed.
    params.extension_app_id = extension_id_;
    params.disposition = NEW_POPUP;
    params.window_bounds = initial_pos;
    params.window_action = browser::NavigateParams::SHOW_WINDOW;
    params.user_gesture = user_gesture;
    browser::Navigate(&params);
    return;
  }

  // First, if the creating extension view was associated with a tab contents,
  // use that tab content's delegate. We must be careful here that the
  // associated tab contents has the same profile as the new tab contents. In
  // the case of extensions in 'spanning' incognito mode, they can mismatch.
  // We don't want to end up putting a normal tab into an incognito window, or
  // vice versa.
  TabContents* associated_contents = GetAssociatedTabContents();
  if (associated_contents &&
      associated_contents->browser_context() == contents->browser_context()) {
    associated_contents->AddNewContents(
        contents, disposition, initial_pos, user_gesture);
    return;
  }

  // If there's no associated tab contents, or it doesn't have a matching
  // profile, try finding an open window. Again, we must make sure to find a
  // window with the correct profile.
  Browser* browser = BrowserList::FindTabbedBrowser(
      profile, false);  // Match incognito exactly.

  // If there's no Browser open with the right profile, create a new one.
  if (!browser) {
    browser = Browser::Create(profile);
    browser->window()->Show();
  }
  browser->AddTabContents(contents, disposition, initial_pos, user_gesture);
}


void ExtensionHost::RenderViewReady() {
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_HOST_CREATED,
      content::Source<Profile>(profile_),
      content::Details<ExtensionHost>(this));
}
