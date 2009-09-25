// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_host.h"

#include "app/resource_bundle.h"
#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_theme_provider.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/dom_ui/dom_ui_factory.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/common/bindings_policy.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/view_types.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"

#include "grit/browser_resources.h"

#include "webkit/glue/context_menu.h"

using WebKit::WebDragOperation;
using WebKit::WebDragOperationsMask;

// static
bool ExtensionHost::enable_dom_automation_ = false;

static const char* kToolstripTextColorSubstitution = "$TEXT_COLOR$";

ExtensionHost::ExtensionHost(Extension* extension, SiteInstance* site_instance,
                             const GURL& url, ViewType::Type host_type)
    : extension_(extension),
      profile_(site_instance->browsing_instance()->profile()),
      did_stop_loading_(false),
      document_element_available_(false),
      url_(url),
      extension_host_type_(host_type) {
  render_view_host_ = new RenderViewHost(
      site_instance, this, MSG_ROUTING_NONE, NULL);
  render_view_host_->AllowBindings(BindingsPolicy::EXTENSION);
  if (enable_dom_automation_)
    render_view_host_->AllowBindings(BindingsPolicy::DOM_AUTOMATION);
}

ExtensionHost::~ExtensionHost() {
  NotificationService::current()->Notify(
      NotificationType::EXTENSION_HOST_DESTROYED,
      Source<Profile>(profile_),
      Details<ExtensionHost>(this));
  render_view_host_->Shutdown();  // deletes render_view_host
}

void ExtensionHost::CreateView(Browser* browser) {
#if defined(TOOLKIT_VIEWS)
  view_.reset(new ExtensionView(this, browser));
  // We own |view_|, so don't auto delete when it's removed from the view
  // hierarchy.
  view_->SetParentOwned(false);
#elif defined(OS_LINUX)
  view_.reset(new ExtensionViewGtk(this, browser));
  view_->Init();
#elif defined(OS_MACOSX)
  view_.reset(new ExtensionViewMac(this, browser));
  view_->Init();
#else
  // TODO(port)
  NOTREACHED();
#endif
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

void ExtensionHost::CreateRenderView(RenderWidgetHostView* host_view) {
  render_view_host_->set_view(host_view);
  render_view_host_->CreateRenderView();
  NavigateToURL(url_);
  DCHECK(IsRenderViewLive());
  NotificationService::current()->Notify(
      NotificationType::EXTENSION_PROCESS_CREATED,
      Source<Profile>(profile_),
      Details<ExtensionHost>(this));
}

void ExtensionHost::NavigateToURL(const GURL& url) {
  // Prevent explicit navigation to another extension id's pages.
  // This method is only called by some APIs, so we still need to protect
  // DidNavigate below (location = "").
  if (url.SchemeIs(chrome::kExtensionScheme) &&
      url.host() != extension_->id()) {
    // TODO(erikkay) communicate this back to the caller?
    return;
  }

  url_ = url;

  if (!is_background_page() && !extension_->GetBackgroundPageReady()) {
    // Make sure the background page loads before any others.
    registrar_.Add(this, NotificationType::EXTENSION_BACKGROUND_PAGE_READY,
                   Source<Extension>(extension_));
    return;
  }
  render_view_host_->NavigateToURL(url_);
}

void ExtensionHost::Observe(NotificationType type,
                            const NotificationSource& source,
                            const NotificationDetails& details) {
  DCHECK(type == NotificationType::EXTENSION_BACKGROUND_PAGE_READY);
  DCHECK(extension_->GetBackgroundPageReady());
  NavigateToURL(url_);
}

void ExtensionHost::UpdatePreferredWidth(int pref_width) {
  if (view_.get())
    view_->UpdatePreferredWidth(pref_width);
}

void ExtensionHost::RenderViewGone(RenderViewHost* render_view_host) {
  DCHECK_EQ(render_view_host_, render_view_host);
  NotificationService::current()->Notify(
      NotificationType::EXTENSION_PROCESS_CRASHED,
      Source<ExtensionsService>(profile_->GetExtensionsService()),
      Details<ExtensionHost>(this));
}

void ExtensionHost::DidNavigate(RenderViewHost* render_view_host,
    const ViewHostMsg_FrameNavigate_Params& params) {
  // We only care when the outer frame changes.
  switch (params.transition) {
    case PageTransition::AUTO_SUBFRAME:
    case PageTransition::MANUAL_SUBFRAME:
      return;
  }

  if (!params.url.SchemeIs(chrome::kExtensionScheme)) {
    extension_function_dispatcher_.reset(NULL);
    url_ = params.url;
    return;
  }

  // This catches two bogus use cases:
  // (1) URLs that look like chrome-extension://somethingbogus or
  //     chrome-extension://nosuchid/, in other words, no Extension would
  //     be found.
  // (2) URLs that refer to a different extension than this one.
  // In both cases, we preserve the old URL and reset the EFD to NULL.  This
  // will leave the host in kind of a bad state with poor UI and errors, but
  // it's better than the alternative.
  // TODO(erikkay) Perhaps we should display log errors or display a big 404
  // in the toolstrip or something like that.
  if (params.url.host() != extension_->id()) {
    extension_function_dispatcher_.reset(NULL);
    return;
  }

  url_ = params.url;
  extension_function_dispatcher_.reset(
      new ExtensionFunctionDispatcher(render_view_host_, this, url_));
}

void ExtensionHost::InsertThemeCSS() {
  DCHECK(!is_background_page());

  static const base::StringPiece toolstrip_theme_css(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_EXTENSIONS_TOOLSTRIP_THEME_CSS));

  std::string css = toolstrip_theme_css.as_string();
  ThemeProvider* theme_provider =
      render_view_host()->process()->profile()->GetThemeProvider();

  SkColor text_color = theme_provider ?
      theme_provider->GetColor(BrowserThemeProvider::COLOR_BOOKMARK_TEXT) :
      SK_ColorBLACK;

  std::string hex_color_string = StringPrintf(
      "#%02x%02x%02x", SkColorGetR(text_color),
                       SkColorGetG(text_color),
                       SkColorGetB(text_color));
  size_t pos = css.find(kToolstripTextColorSubstitution);
  while (pos != std::string::npos) {
    css.replace(pos, 12, hex_color_string);
    pos = css.find(kToolstripTextColorSubstitution);
  }

  // As a toolstrip, inject our toolstrip CSS to make it easier for toolstrips
  // to blend in with the chrome UI.
  render_view_host()->InsertCSSInWebFrame(L"", css, "ToolstripThemeCSS");
}

void ExtensionHost::DidStopLoading(RenderViewHost* render_view_host) {
  if (!did_stop_loading_) {
    NotificationService::current()->Notify(
        NotificationType::EXTENSION_HOST_DID_STOP_LOADING,
        Source<Profile>(profile_),
        Details<ExtensionHost>(this));
    did_stop_loading_ = true;
  }
  if (extension_host_type_ == ViewType::EXTENSION_TOOLSTRIP ||
      extension_host_type_ == ViewType::EXTENSION_MOLE) {
#if defined(TOOLKIT_VIEWS)
    if (view_.get())
      view_->DidStopLoading();
#endif
  }
}

void ExtensionHost::DocumentAvailableInMainFrame(RenderViewHost* rvh) {
  document_element_available_ = true;
  if (is_background_page())
    extension_->SetBackgroundPageReady();
  else
    InsertThemeCSS();
}

void ExtensionHost::RunJavaScriptMessage(const std::wstring& message,
                                         const std::wstring& default_prompt,
                                         const GURL& frame_url,
                                         const int flags,
                                         IPC::Message* reply_msg,
                                         bool* did_suppress_message) {
  // Automatically cancel the javascript alert (otherwise the renderer hangs
  // indefinitely).
  *did_suppress_message = true;
  render_view_host()->JavaScriptMessageBoxClosed(reply_msg, true, L"");
}

WebPreferences ExtensionHost::GetWebkitPrefs() {
  PrefService* prefs = render_view_host()->process()->profile()->GetPrefs();
  const bool kIsDomUI = true;
  return RenderViewHostDelegateHelper::GetWebkitPrefs(prefs, kIsDomUI);
}

void ExtensionHost::ProcessDOMUIMessage(const std::string& message,
                                        const std::string& content,
                                        int request_id,
                                        bool has_callback) {
  if (extension_function_dispatcher_.get()) {
    extension_function_dispatcher_->HandleRequest(message, content, request_id,
                                                  has_callback);
  }
}

RenderViewHostDelegate::View* ExtensionHost::GetViewDelegate() {
  return this;
}

void ExtensionHost::CreateNewWindow(int route_id,
                                    base::WaitableEvent* modal_dialog_event) {
  delegate_view_helper_.CreateNewWindow(
      route_id, modal_dialog_event, render_view_host()->process()->profile(),
      site_instance(), DOMUIFactory::GetDOMUIType(url_), NULL);
}

void ExtensionHost::CreateNewWidget(int route_id, bool activatable) {
  delegate_view_helper_.CreateNewWidget(route_id, activatable,
                                        site_instance()->GetProcess());
}

void ExtensionHost::ShowCreatedWindow(int route_id,
                                      WindowOpenDisposition disposition,
                                      const gfx::Rect& initial_pos,
                                      bool user_gesture,
                                      const GURL& creator_url) {
  TabContents* contents = delegate_view_helper_.GetCreatedWindow(route_id);
  if (contents) {
    Browser* browser = GetBrowser();
    DCHECK(browser);
    if (!browser)
      return;
    browser->AddTabContents(contents, disposition, initial_pos,
                            user_gesture);
  }
}

void ExtensionHost::ShowCreatedWidget(int route_id,
                                      const gfx::Rect& initial_pos) {
  RenderWidgetHostView* widget_host_view =
      delegate_view_helper_.GetCreatedWidget(route_id);
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

void ExtensionHost::StartDragging(const WebDropData& drop_data,
    WebDragOperationsMask operation_mask) {
}

void ExtensionHost::UpdateDragCursor(WebDragOperation operation) {
}

void ExtensionHost::GotFocus() {
}

void ExtensionHost::TakeFocus(bool reverse) {
}

void ExtensionHost::HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {
}

void ExtensionHost::HandleMouseEvent() {
#if defined(OS_WIN)
  if (view_.get())
    view_->HandleMouseEvent();
#endif
}

void ExtensionHost::HandleMouseLeave() {
#if defined(OS_WIN)
  if (view_.get())
    view_->HandleMouseLeave();
#endif
}

Browser* ExtensionHost::GetBrowser() {
  if (view_.get())
    return view_->browser();

  Profile* profile = render_view_host()->process()->profile();
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile);

  // It's possible for a browser to exist, but to have never been active.
  // This can happen if you launch the browser on a machine without an active
  // desktop (a headless buildbot) or if you quickly give another app focus
  // at launch time.  This is easy to do with browser_tests.
  if (!browser)
    browser = BrowserList::FindBrowserWithProfile(profile);

  // TODO(erikkay): can this still return NULL?  Is Rafael's comment still
  // valid here?
  // NOTE(rafaelw): This can return NULL in some circumstances. In particular,
  // a toolstrip or background_page onload chrome.tabs api call can make it
  // into here before the browser is sufficiently initialized to return here.
  // A similar situation may arise during shutdown.
  // TODO(rafaelw): Delay creation of background_page until the browser
  // is available. http://code.google.com/p/chromium/issues/detail?id=13284
  return browser;
}

void ExtensionHost::SetRenderViewType(ViewType::Type type) {
  DCHECK(type == ViewType::EXTENSION_MOLE ||
         type == ViewType::EXTENSION_TOOLSTRIP);
  extension_host_type_ = type;
  render_view_host()->ViewTypeChanged(extension_host_type_);
}

ViewType::Type ExtensionHost::GetRenderViewType() const {
  return extension_host_type_;
}

void ExtensionHost::RenderViewCreated(RenderViewHost* render_view_host) {
  // TODO(mpcomplete): This is duplicated in DidNavigate, which means that
  // we'll create 2 EFDs for the first navigation. We should try to find a
  // better way to unify them.
  // See http://code.google.com/p/chromium/issues/detail?id=18240
  extension_function_dispatcher_.reset(
      new ExtensionFunctionDispatcher(render_view_host, this, url_));

  render_view_host->Send(new ViewMsg_EnableIntrinsicWidthChangedMode(
      render_view_host->routing_id()));
}

int ExtensionHost::GetBrowserWindowID() const {
  int window_id = -1;
  if (extension_host_type_ == ViewType::EXTENSION_TOOLSTRIP) {
    window_id = ExtensionTabUtil::GetWindowId(
        const_cast<ExtensionHost* >(this)->GetBrowser());
  } else if (extension_host_type_ == ViewType::EXTENSION_BACKGROUND_PAGE) {
    // Background page is not attached to any browser window, so pass -1.
    window_id = -1;
  } else {
    NOTREACHED();
  }
  return window_id;
}
