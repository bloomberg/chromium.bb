// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_host.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/common/bindings_policy.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"

#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

#include "webkit/glue/context_menu.h"

namespace {

class CrashedExtensionInfobarDelegate : public ConfirmInfoBarDelegate {
 public:
  CrashedExtensionInfobarDelegate(TabContents* tab_contents,
                                  ExtensionHost* extension_host)
      : ConfirmInfoBarDelegate(tab_contents),
        extension_host_(extension_host) {
  }

  virtual std::wstring GetMessageText() const {
    return l10n_util::GetStringF(IDS_EXTENSION_CRASHED_INFOBAR_MESSAGE,
        UTF8ToWide(extension_host_->extension()->name()));
  }

  virtual SkBitmap* GetIcon() const {
    // TODO(erikkay): Create extension-specific icon. http://crbug.com/14591
    return ResourceBundle::GetSharedInstance().GetBitmapNamed(
        IDR_INFOBAR_PLUGIN_CRASHED);
  }

  virtual int GetButtons() const {
    return BUTTON_OK;
  }

  virtual std::wstring GetButtonLabel(
      ConfirmInfoBarDelegate::InfoBarButton button) const {
    if (button == BUTTON_OK)
      return l10n_util::GetString(IDS_EXTENSION_CRASHED_INFOBAR_RESTART_BUTTON);
    return ConfirmInfoBarDelegate::GetButtonLabel(button);
  }

  virtual bool Accept() {
    extension_host_->RecoverCrashedExtension();
    return true;
  }

 private:
  ExtensionHost* extension_host_;

  DISALLOW_COPY_AND_ASSIGN(CrashedExtensionInfobarDelegate);
};

}  // namespace


// static
bool ExtensionHost::enable_dom_automation_ = false;

ExtensionHost::ExtensionHost(Extension* extension, SiteInstance* site_instance,
                             const GURL& url)
    : extension_(extension),
      profile_(site_instance->browsing_instance()->profile()),
      did_stop_loading_(false),
      document_element_available_(false),
      url_(url) {
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
  view_.reset(new ExtensionViewGtk(this));
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

void ExtensionHost::RecoverCrashedExtension() {
  DCHECK(!IsRenderViewLive());
#if defined(TOOLKIT_VIEWS)
  if (view_.get()) {
    // The view should call us back to CreateRenderView, which is the place
    // where we create the render process and fire notification.
    view_->RecoverCrashedExtension();
  } else {
    CreateRenderView(NULL);
  }
#else
  CreateRenderView(NULL);
#endif
}

void ExtensionHost::UpdatePreferredWidth(int pref_width) {
#if defined(OS_WIN)
  if (view_.get())
    view_->DidContentsPreferredWidthChange(pref_width);
#endif
}

void ExtensionHost::RenderViewGone(RenderViewHost* render_view_host) {
  DCHECK_EQ(render_view_host_, render_view_host);
  Browser* browser = GetBrowser();
  if (browser) {
    TabContents* current_tab = browser->GetSelectedTabContents();
    if (current_tab) {
      current_tab->AddInfoBar(
          new CrashedExtensionInfobarDelegate(current_tab, this));
    }
  }
  NotificationService::current()->Notify(
      NotificationType::EXTENSION_PROCESS_CRASHED,
      Source<Profile>(profile_),
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

  url_ = params.url;
  if (!url_.SchemeIs(chrome::kExtensionScheme)) {
    extension_function_dispatcher_.reset(NULL);
    return;
  }
  extension_function_dispatcher_.reset(
      new ExtensionFunctionDispatcher(render_view_host_, this, url_));
}

void ExtensionHost::DidStopLoading(RenderViewHost* render_view_host) {
#if defined(TOOLKIT_VIEWS)
  ExtensionView* view = view_.get();
  if (view) {
    // TODO(erikkay) this injection should really happen in the renderer.
    // When the Jerry's view type change lands, investigate moving this there.

    // As a toolstrip, inject our toolstrip CSS to make it easier for toolstrips
    // to blend in with the chrome UI.
    if (view->is_toolstrip()) {
      static const StringPiece toolstrip_css(
          ResourceBundle::GetSharedInstance().GetRawDataResource(
              IDR_EXTENSIONS_TOOLSTRIP_CSS));
      render_view_host->InsertCSSInWebFrame(L"", toolstrip_css.as_string());
    } else {
      // No CSS injecting currently, but call SetDidInsertCSS to tell the view
      // that it's OK to display.
      view->SetDidInsertCSS(true);
    }
  }
#endif

  did_stop_loading_ = true;
}

void ExtensionHost::DocumentAvailableInMainFrame(RenderViewHost* rvh) {
  document_element_available_ = true;
  if (is_background_page())
    extension_->SetBackgroundPageReady();
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

void ExtensionHost::DidInsertCSS() {
#if defined(TOOLKIT_VIEWS)
  if (view_.get())
    view_->SetDidInsertCSS(true);
#endif
}

RenderViewHostDelegate::View* ExtensionHost::GetViewDelegate() {
  return this;
}

void ExtensionHost::CreateNewWindow(int route_id,
                                    base::WaitableEvent* modal_dialog_event) {
  delegate_view_helper_.CreateNewWindow(
      route_id, modal_dialog_event, render_view_host()->process()->profile(),
      site_instance());
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
  // TODO(erikkay) - This is a temporary hack.  Show a menu here instead.
  DevToolsManager::GetInstance()->OpenDevToolsWindow(render_view_host());
}

void ExtensionHost::StartDragging(const WebDropData& drop_data) {
}

void ExtensionHost::UpdateDragCursor(bool is_drop_target) {
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
#if defined(OS_WIN)
  if (view_.get())
    return view_->browser();
#endif
  Browser* browser = BrowserList::GetLastActiveWithProfile(
      render_view_host()->process()->profile());
  // NOTE(rafaelw): This can return NULL in some circumstances. In particular,
  // a toolstrip or background_page onload chrome.tabs api call can make it
  // into here before the browser is sufficiently initialized to return here.
  // A similar situation may arise during shutdown.
  // TODO(rafaelw): Delay creation of background_page until the browser
  // is available. http://code.google.com/p/chromium/issues/detail?id=13284
  return browser;
}

void ExtensionHost::RenderViewCreated(RenderViewHost* render_view_host) {
}
