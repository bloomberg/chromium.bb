// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_host.h"

#include <list>

#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_preferences_util.h"
#include "chrome/browser/tab_contents/popup_menu_helper_mac.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/app_modal_dialogs/message_box_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/chrome_web_ui_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browsing_instance.h"
#include "content/browser/content_browser_client.h"
#include "content/browser/renderer_host/browser_render_process_host.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/common/bindings_policy.h"
#include "content/common/content_client.h"
#include "content/common/native_web_keyboard_event.h"
#include "content/common/notification_service.h"
#include "content/common/view_messages.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/glue/context_menu.h"

#if defined(TOOLKIT_VIEWS)
#include "views/widget/widget.h"
#endif

using WebKit::WebDragOperation;
using WebKit::WebDragOperationsMask;

// static
bool ExtensionHost::enable_dom_automation_ = false;

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
        ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) { }

  // Queue up a delayed task to process the next ExtensionHost in the queue.
  void PostTask() {
    if (!pending_create_) {
      MessageLoop::current()->PostTask(FROM_HERE,
          method_factory_.NewRunnableMethod(
             &ProcessCreationQueue::ProcessOneHost));
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
  ScopedRunnableMethodFactory<ProcessCreationQueue> method_factory_;
};

////////////////
// ExtensionHost

ExtensionHost::ExtensionHost(const Extension* extension,
                             SiteInstance* site_instance,
                             const GURL& url,
                             ViewType::Type host_type)
    : extension_(extension),
      extension_id_(extension->id()),
      profile_(Profile::FromBrowserContext(
          site_instance->browsing_instance()->browser_context())),
      did_stop_loading_(false),
      document_element_available_(false),
      url_(url),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          extension_function_dispatcher_(profile_, this)),
      extension_host_type_(host_type),
      associated_tab_contents_(NULL) {
  render_view_host_ = new RenderViewHost(site_instance, this, MSG_ROUTING_NONE,
                                         NULL);
  if (enable_dom_automation_)
    render_view_host_->AllowBindings(BindingsPolicy::DOM_AUTOMATION);

  // Listen for when the render process' handle is available so we can add it
  // to the task manager then.
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 Source<RenderProcessHost>(render_process_host()));
  // Listen for when an extension is unloaded from the same profile, as it may
  // be the same extension that this points to.
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 Source<Profile>(profile_));
}

ExtensionHost::~ExtensionHost() {
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_HOST_DESTROYED,
      Source<Profile>(profile_),
      Details<ExtensionHost>(this));
  ProcessCreationQueue::GetInstance()->Remove(this);
  GetJavaScriptDialogCreatorInstance()->ResetJavaScriptState(this);
  render_view_host_->Shutdown();  // deletes render_view_host
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

RenderProcessHost* ExtensionHost::render_process_host() const {
  return render_view_host_->process();
}

SiteInstance* ExtensionHost::site_instance() const {
  return render_view_host_->site_instance();
}

bool ExtensionHost::IsRenderViewLive() const {
  return render_view_host_->IsRenderViewLive();
}

void ExtensionHost::CreateRenderViewSoon(RenderWidgetHostView* host_view) {
  render_view_host_->SetView(host_view);
  if (render_view_host_->process()->HasConnection()) {
    // If the process is already started, go ahead and initialize the RenderView
    // synchronously. The process creation is the real meaty part that we want
    // to defer.
    CreateRenderViewNow();
  } else {
    ProcessCreationQueue::GetInstance()->CreateSoon(this);
  }
}

void ExtensionHost::CreateRenderViewNow() {
  render_view_host_->CreateRenderView(string16());
  if (extension_host_type_ == ViewType::EXTENSION_POPUP ||
      extension_host_type_ == ViewType::EXTENSION_DIALOG ||
      extension_host_type_ == ViewType::EXTENSION_INFOBAR) {
    // If the host is bound to a browser, then extract its window id.
    // Extensions hosted in ExternalTabContainer objects may not have
    // an associated browser.
    const Browser* browser = GetBrowser();
    if (browser && render_view_host_) {
      render_view_host_->Send(new ExtensionMsg_UpdateBrowserWindowId(
          render_view_host_->routing_id(),
          ExtensionTabUtil::GetWindowId(browser)));
    }
  }
  NavigateToURL(url_);
  DCHECK(IsRenderViewLive());
  if (is_background_page())
    profile_->GetExtensionService()->DidCreateRenderViewForBackgroundPage(
        this);
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

void ExtensionHost::NavigateToURL(const GURL& url) {
  // Prevent explicit navigation to another extension id's pages.
  // This method is only called by some APIs, so we still need to protect
  // DidNavigate below (location = "").
  if (url.SchemeIs(chrome::kExtensionScheme) && url.host() != extension_id()) {
    // TODO(erikkay) communicate this back to the caller?
    return;
  }

  url_ = url;

  if (!is_background_page() &&
      !profile_->GetExtensionService()->IsBackgroundPageReady(extension_)) {
    // Make sure the background page loads before any others.
    registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,
                   Source<Extension>(extension_));
    return;
  }

  render_view_host_->NavigateToURL(url_);
}

void ExtensionHost::Observe(int type,
                            const NotificationSource& source,
                            const NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY:
      DCHECK(profile_->GetExtensionService()->
          IsBackgroundPageReady(extension_));
      NavigateToURL(url_);
      break;
    case content::NOTIFICATION_RENDERER_PROCESS_CREATED:
      NotificationService::current()->Notify(
          chrome::NOTIFICATION_EXTENSION_PROCESS_CREATED,
          Source<Profile>(profile_),
          Details<ExtensionHost>(this));
      break;
    case chrome::NOTIFICATION_EXTENSION_UNLOADED:
      // The extension object will be deleted after this notification has been
      // sent. NULL it out so that dirty pointer issues don't arise in cases
      // when multiple ExtensionHost objects pointing to the same Extension are
      // present.
      if (extension_ == Details<UnloadedExtensionInfo>(details)->extension)
        extension_ = NULL;
      break;
    default:
      NOTREACHED() << "Unexpected notification sent.";
      break;
  }
}

void ExtensionHost::UpdatePreferredSize(const gfx::Size& new_size) {
  if (view_.get())
    view_->UpdatePreferredSize(new_size);
}

void ExtensionHost::RenderViewGone(RenderViewHost* render_view_host,
                                   base::TerminationStatus status,
                                   int error_code) {
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
  DCHECK_EQ(render_view_host_, render_view_host);
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_PROCESS_TERMINATED,
      Source<Profile>(profile_),
      Details<ExtensionHost>(this));
}

void ExtensionHost::DidNavigate(RenderViewHost* render_view_host,
    const ViewHostMsg_FrameNavigate_Params& params) {
  // We only care when the outer frame changes.
  if (!PageTransition::IsMainFrame(params.transition))
    return;

  url_ = params.url;
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
  if (extension_host_type_ == ViewType::EXTENSION_POPUP ||
      extension_host_type_ == ViewType::EXTENSION_DIALOG ||
      extension_host_type_ == ViewType::EXTENSION_INFOBAR) {
#if defined(TOOLKIT_VIEWS)
    if (view_.get())
      view_->DidStopLoading();
#endif
  }
  if (notify) {
    NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING,
        Source<Profile>(profile_),
        Details<ExtensionHost>(this));
    if (extension_host_type_ == ViewType::EXTENSION_BACKGROUND_PAGE) {
      UMA_HISTOGRAM_TIMES("Extensions.BackgroundPageLoadTime",
                          since_created_.Elapsed());
    } else if (extension_host_type_ == ViewType::EXTENSION_DIALOG) {
      UMA_HISTOGRAM_TIMES("Extensions.DialogLoadTime",
                          since_created_.Elapsed());
    } else if (extension_host_type_ == ViewType::EXTENSION_POPUP) {
      UMA_HISTOGRAM_TIMES("Extensions.PopupLoadTime",
                          since_created_.Elapsed());
    } else if (extension_host_type_ == ViewType::EXTENSION_INFOBAR) {
      UMA_HISTOGRAM_TIMES("Extensions.InfobarLoadTime",
        since_created_.Elapsed());
    }
  }
}

void ExtensionHost::DocumentAvailableInMainFrame(RenderViewHost* rvh) {
  // If the document has already been marked as available for this host, then
  // bail. No need for the redundant setup. http://crbug.com/31170
  if (document_element_available_)
    return;

  document_element_available_ = true;
  if (is_background_page()) {
    profile_->GetExtensionService()->SetBackgroundPageReady(extension_);
  } else {
    switch (extension_host_type_) {
      case ViewType::EXTENSION_INFOBAR:
        InsertInfobarCSS();
        break;
      default:
        break;  // No style sheet for other types, at the moment.
    }
  }
}

void ExtensionHost::DocumentOnLoadCompletedInMainFrame(RenderViewHost* rvh,
                                                       int32 page_id) {
  if (ViewType::EXTENSION_POPUP == GetRenderViewType()) {
    NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_POPUP_VIEW_READY,
        Source<Profile>(profile_),
        Details<ExtensionHost>(this));
  }
}

void ExtensionHost::RunJavaScriptMessage(const RenderViewHost* rvh,
                                         const string16& message,
                                         const string16& default_prompt,
                                         const GURL& frame_url,
                                         const int flags,
                                         IPC::Message* reply_msg,
                                         bool* did_suppress_message) {
  bool suppress_this_message = false;

  string16 title;
  if (extension_->location() == Extension::COMPONENT)
    title = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
  else
    title = UTF8ToUTF16(extension_->name());

  GetJavaScriptDialogCreatorInstance()->RunJavaScriptDialog(
      this,
      content::JavaScriptDialogCreator::DIALOG_TITLE_PLAIN_STRING,
      title,
      flags,
      message,
      default_prompt,
      reply_msg,
      &suppress_this_message);

  if (suppress_this_message) {
    // If we are suppressing messages, just reply as if the user immediately
    // pressed "Cancel".
    OnDialogClosed(reply_msg, false, string16());
  }

  *did_suppress_message = suppress_this_message;
}

gfx::NativeWindow ExtensionHost::GetDialogRootWindow() {
  // If we have a view, use that.
  gfx::NativeView native_view = GetNativeViewOfHost();
  if (native_view)
    return platform_util::GetTopLevel(native_view);

  // Otherwise, try the active tab's view.
  Browser* browser = extension_function_dispatcher_.GetCurrentBrowser(
      render_view_host_, true);
  if (browser) {
    TabContents* active_tab = browser->GetSelectedTabContents();
    if (active_tab)
      return active_tab->view()->GetTopLevelNativeWindow();
  }

  return NULL;
}

void ExtensionHost::OnDialogClosed(IPC::Message* reply_msg,
                                   bool success,
                                   const string16& user_input) {
  render_view_host()->JavaScriptDialogClosed(reply_msg,
                                             success,
                                             user_input);
}

void ExtensionHost::Close(RenderViewHost* render_view_host) {
  if (extension_host_type_ == ViewType::EXTENSION_POPUP ||
      extension_host_type_ == ViewType::EXTENSION_DIALOG ||
      extension_host_type_ == ViewType::EXTENSION_BACKGROUND_PAGE ||
      extension_host_type_ == ViewType::EXTENSION_INFOBAR) {
    NotificationService::current()->Notify(
        chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
        Source<Profile>(profile_),
        Details<ExtensionHost>(this));
  }
}

RendererPreferences ExtensionHost::GetRendererPrefs(
    content::BrowserContext* browser_context) const {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  RendererPreferences preferences;

  TabContents* associated_contents = GetAssociatedTabContents();
  if (associated_contents)
    preferences =
        static_cast<RenderViewHostDelegate*>(associated_contents)->
            GetRendererPrefs(profile);

  renderer_preferences_util::UpdateFromSystemSettings(&preferences, profile);
  return preferences;
}

WebPreferences ExtensionHost::GetWebkitPrefs() {
  Profile* profile = Profile::FromBrowserContext(
      render_view_host()->process()->browser_context());
  WebPreferences webkit_prefs =
      RenderViewHostDelegateHelper::GetWebkitPrefs(profile,
                                                   false);  // is_web_ui
  // Extensions are trusted so we override any user preferences for disabling
  // javascript or images.
  webkit_prefs.loads_images_automatically = true;
  webkit_prefs.javascript_enabled = true;

  if (extension_host_type_ == ViewType::EXTENSION_POPUP ||
      extension_host_type_ == ViewType::EXTENSION_DIALOG ||
      extension_host_type_ == ViewType::EXTENSION_BACKGROUND_PAGE ||
      extension_host_type_ == ViewType::EXTENSION_INFOBAR)
    webkit_prefs.allow_scripts_to_close_windows = true;

  // Disable anything that requires the GPU process for background pages.
  // See http://crbug.com/64512 and http://crbug.com/64841.
  if (extension_host_type_ == ViewType::EXTENSION_BACKGROUND_PAGE) {
    webkit_prefs.experimental_webgl_enabled = false;
    webkit_prefs.accelerated_compositing_enabled = false;
    webkit_prefs.accelerated_2d_canvas_enabled = false;
  }

  return webkit_prefs;
}

RenderViewHostDelegate::View* ExtensionHost::GetViewDelegate() {
  return this;
}

bool ExtensionHost::PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                           bool* is_keyboard_shortcut) {
  if (extension_host_type_ == ViewType::EXTENSION_POPUP &&
      event.type == NativeWebKeyboardEvent::RawKeyDown &&
      event.windowsKeyCode == ui::VKEY_ESCAPE) {
    DCHECK(is_keyboard_shortcut != NULL);
    *is_keyboard_shortcut = true;
  }
  return false;
}

void ExtensionHost::HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {
  if (extension_host_type_ == ViewType::EXTENSION_POPUP) {
    if (event.type == NativeWebKeyboardEvent::RawKeyDown &&
        event.windowsKeyCode == ui::VKEY_ESCAPE) {
      NotificationService::current()->Notify(
          chrome::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
          Source<Profile>(profile_),
          Details<ExtensionHost>(this));
      return;
    }
  }
  UnhandledKeyboardEvent(event);
}

void ExtensionHost::HandleMouseMove() {
#if defined(OS_WIN)
  if (view_.get())
    view_->HandleMouseMove();
#endif
}

void ExtensionHost::HandleMouseDown() {
}

void ExtensionHost::HandleMouseLeave() {
#if defined(OS_WIN)
  if (view_.get())
    view_->HandleMouseLeave();
#endif
}

void ExtensionHost::HandleMouseUp() {
}

void ExtensionHost::HandleMouseActivate() {
}

void ExtensionHost::RunFileChooser(
    RenderViewHost* render_view_host,
    const ViewHostMsg_RunFileChooser_Params& params) {
  // This object is destroyed when the file selection is performed or
  // cancelled.
  FileSelectHelper* file_select_helper = new FileSelectHelper(profile());
  file_select_helper->RunFileChooser(render_view_host,
                                     GetAssociatedTabContents(),
                                     params);
}

void ExtensionHost::CreateNewWindow(
    int route_id,
    const ViewHostMsg_CreateWindow_Params& params) {
  // TODO(aa): Use the browser's profile if the extension is split mode
  // incognito.
  Profile* profile = Profile::FromBrowserContext(
      render_view_host()->process()->browser_context());
  TabContents* new_contents = delegate_view_helper_.CreateNewWindow(
      route_id,
      profile,
      site_instance(),
      ChromeWebUIFactory::GetInstance()->GetWebUIType(
          render_view_host()->process()->browser_context(), url_),
      this,
      params.window_container_type,
      params.frame_name);

  TabContents* associated_contents = GetAssociatedTabContents();
  if (associated_contents && associated_contents->delegate())
    associated_contents->delegate()->TabContentsCreated(new_contents);
}

void ExtensionHost::CreateNewWidget(int route_id,
                                    WebKit::WebPopupType popup_type) {
  CreateNewWidgetInternal(route_id, popup_type);
}

void ExtensionHost::CreateNewFullscreenWidget(int route_id) {
  NOTREACHED()
      << "ExtensionHost does not support showing full screen popups yet.";
}

RenderWidgetHostView* ExtensionHost::CreateNewWidgetInternal(
    int route_id, WebKit::WebPopupType popup_type) {
  return delegate_view_helper_.CreateNewWidget(route_id, popup_type,
                                               site_instance()->GetProcess());
}

void ExtensionHost::ShowCreatedWindow(int route_id,
                                      WindowOpenDisposition disposition,
                                      const gfx::Rect& initial_pos,
                                      bool user_gesture) {
  TabContents* contents = delegate_view_helper_.GetCreatedWindow(route_id);
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

  // If the tab contents isn't a popup, it's a normal tab. We need to find a
  // home for it. This is typically a Browser, but it can also be some other
  // TabContentsDelegate in the case of ChromeFrame.

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

void ExtensionHost::ShowCreatedWidget(int route_id,
                                      const gfx::Rect& initial_pos) {
  ShowCreatedWidgetInternal(delegate_view_helper_.GetCreatedWidget(route_id),
                            initial_pos);
}

void ExtensionHost::ShowCreatedFullscreenWidget(int route_id) {
  NOTREACHED()
      << "ExtensionHost does not support showing full screen popups yet.";
}

void ExtensionHost::ShowCreatedWidgetInternal(
    RenderWidgetHostView* widget_host_view,
    const gfx::Rect& initial_pos) {
  Browser *browser = GetBrowser();
  DCHECK(browser);
  if (!browser)
    return;
  browser->BrowserRenderWidgetShowing();
  // TODO(erikkay): These two lines could be refactored with TabContentsView.
  widget_host_view->InitAsPopup(render_view_host()->view(), initial_pos);
  widget_host_view->GetRenderWidgetHost()->Init();
}

void ExtensionHost::ShowContextMenu(const ContextMenuParams& params) {
  // TODO(erikkay) Show a default context menu.
}

void ExtensionHost::ShowPopupMenu(const gfx::Rect& bounds,
                                  int item_height,
                                  double item_font_size,
                                  int selected_item,
                                  const std::vector<WebMenuItem>& items,
                                  bool right_aligned) {
#if defined(OS_MACOSX)
  PopupMenuHelper popup_menu_helper(render_view_host());
  popup_menu_helper.ShowPopupMenu(bounds, item_height, item_font_size,
                                  selected_item, items, right_aligned);
#else
  // Only on Mac are select popup menus external.
  NOTREACHED();
#endif
}

void ExtensionHost::StartDragging(const WebDropData& drop_data,
    WebDragOperationsMask operation_mask,
    const SkBitmap& image,
    const gfx::Point& image_offset) {
  // We're not going to do any drag & drop, but we have to tell the renderer the
  // drag & drop ended, othewise the renderer thinks the drag operation is
  // underway and mouse events won't work.  See bug 34061.
  // TODO(twiz) Implement drag & drop support for ExtensionHost instances.
  // See feature issue 36288.
  render_view_host()->DragSourceSystemDragEnded();
}

void ExtensionHost::UpdateDragCursor(WebDragOperation operation) {
}

void ExtensionHost::GotFocus() {
#if defined(TOOLKIT_VIEWS) && !defined(TOUCH_UI)
  // Request focus so that the FocusManager has a focused view and can perform
  // normally its key event processing (so that it lets tab key events go to the
  // renderer).
  view()->RequestFocus();
#else
  // TODO(port)
#endif
}

void ExtensionHost::TakeFocus(bool reverse) {
}

ViewType::Type ExtensionHost::GetRenderViewType() const {
  return extension_host_type_;
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
  extension_function_dispatcher_.Dispatch(params, render_view_host_);
}

const GURL& ExtensionHost::GetURL() const {
  return url_;
}

void ExtensionHost::RenderViewCreated(RenderViewHost* render_view_host) {
  if (view_.get())
    view_->RenderViewCreated();

  if (extension_host_type_ == ViewType::EXTENSION_POPUP ||
      extension_host_type_ == ViewType::EXTENSION_INFOBAR) {
        render_view_host->EnablePreferredSizeMode(
            kPreferredSizeWidth | kPreferredSizeHeightThisIsSlow);
  }
}
