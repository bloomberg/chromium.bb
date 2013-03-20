// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTERNAL_TAB_CONTAINER_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_EXTERNAL_TAB_CONTAINER_WIN_H_

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/automation/automation_resource_message_filter.h"
#include "chrome/browser/external_tab/external_tab_container.h"
#include "chrome/browser/infobars/infobar_container.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper_delegate.h"
#include "chrome/browser/ui/views/unhandled_keyboard_event_handler.h"
#include "content/public/browser/navigation_type.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/views/widget/widget_observer.h"

class AutomationProvider;
class Browser;
class Profile;
class TabContentsContainer;
class RenderViewContextMenuViews;
struct NavigationInfo;

namespace ui {
class ViewProp;
}

namespace views {
class View;
class WebView;
class Widget;
}

// This class serves as the container window for an external tab.
// An external tab is a Chrome tab that is meant to displayed in an
// external process. This class provides the FocusManger needed by the
// WebContents as well as an implementation of content::WebContentsDelegate.
class ExternalTabContainerWin : public ExternalTabContainer,
                                public content::WebContentsDelegate,
                                public content::WebContentsObserver,
                                public content::NotificationObserver,
                                public views::WidgetObserver,
                                public ui::AcceleratorTarget,
                                public InfoBarContainer::Delegate,
                                public BlockedContentTabHelperDelegate {
 public:
  typedef std::map<uintptr_t,
                   scoped_refptr<ExternalTabContainerWin> > PendingTabs;

  ExternalTabContainerWin(AutomationProvider* automation,
                          AutomationResourceMessageFilter* filter);

  static scoped_refptr<ExternalTabContainer> RemovePendingExternalTab(
      uintptr_t cookie);

  // Overridden from ExternalTabContainer:
  virtual bool Init(Profile* profile,
                    HWND parent,
                    const gfx::Rect& bounds,
                    DWORD style,
                    bool load_requests_via_automation,
                    bool handle_top_level_requests,
                    content::WebContents* existing_contents,
                    const GURL& initial_url,
                    const GURL& referrer,
                    bool infobars_enabled,
                    bool supports_full_tab_mode) OVERRIDE;
  virtual void Uninitialize() OVERRIDE;
  virtual bool Reinitialize(AutomationProvider* automation_provider,
                            AutomationResourceMessageFilter* filter,
                            HWND parent_window) OVERRIDE;
  virtual content::WebContents* GetWebContents() const OVERRIDE;
  virtual HWND GetExternalTabHWND() const OVERRIDE;
  virtual HWND GetContentHWND() const OVERRIDE;
  virtual void SetTabHandle(int handle) OVERRIDE;
  virtual int GetTabHandle() const OVERRIDE;
  virtual bool ExecuteContextMenuCommand(int command) OVERRIDE;
  virtual void RunUnloadHandlers(IPC::Message* reply_message) OVERRIDE;
  virtual void ProcessUnhandledAccelerator(const MSG& msg) OVERRIDE;
  virtual void FocusThroughTabTraversal(bool reverse,
                                        bool restore_focus_to_view) OVERRIDE;

  // Overridden from content::WebContentsDelegate:
  virtual content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE;
  virtual void NavigationStateChanged(const content::WebContents* source,
                                      unsigned changed_flags) OVERRIDE;
  virtual void AddNewContents(content::WebContents* source,
                              content::WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture,
                              bool* was_blocked) OVERRIDE;
  virtual void CloseContents(content::WebContents* source) OVERRIDE;
  virtual void MoveContents(content::WebContents* source,
                            const gfx::Rect& pos) OVERRIDE;
  virtual bool IsPopupOrPanel(
      const content::WebContents* source) const OVERRIDE;
  virtual void UpdateTargetURL(content::WebContents* source, int32 page_id,
                               const GURL& url) OVERRIDE;
  virtual void ContentsZoomChange(bool zoom_in) OVERRIDE;
  virtual void WebContentsCreated(content::WebContents* source_contents,
                                  int64 source_frame_id,
                                  const string16& frame_name,
                                  const GURL& target_url,
                                  content::WebContents* new_contents) OVERRIDE;
  virtual bool PreHandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event,
      bool* is_keyboard_shortcut) OVERRIDE;
  virtual void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) OVERRIDE;
  virtual bool TakeFocus(content::WebContents* source, bool reverse) OVERRIDE;
  virtual bool CanDownload(content::RenderViewHost* render_view_host,
                           int request_id,
                           const std::string& request_method) OVERRIDE;
  virtual bool OnGoToEntryOffset(int offset) OVERRIDE;
  virtual bool HandleContextMenu(
      const content::ContextMenuParams& params) OVERRIDE;
  virtual void BeforeUnloadFired(content::WebContents* tab,
                                 bool proceed,
                                 bool* proceed_to_fire_unload) OVERRIDE;
  virtual content::JavaScriptDialogManager*
      GetJavaScriptDialogManager() OVERRIDE;
  virtual void ShowRepostFormWarningDialog(
      content::WebContents* source) OVERRIDE;
  virtual void RunFileChooser(
      content::WebContents* tab,
      const content::FileChooserParams& params) OVERRIDE;
  virtual void EnumerateDirectory(content::WebContents* tab,
                                  int request_id,
                                  const base::FilePath& path) OVERRIDE;
  virtual void JSOutOfMemory(content::WebContents* tab);
  virtual void RegisterProtocolHandler(content::WebContents* tab,
                                       const std::string& protocol,
                                       const GURL& url,
                                       const string16& title,
                                       bool user_gesture) OVERRIDE;
  virtual void FindReply(content::WebContents* tab,
                         int request_id,
                         int number_of_matches,
                         const gfx::Rect& selection_rect,
                         int active_match_ordinal,
                         bool final_update) OVERRIDE;
  virtual void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) OVERRIDE;
  virtual bool RequestPpapiBrokerPermission(
      content::WebContents* web_contents,
      const GURL& url,
      const base::FilePath& plugin_path,
      const base::Callback<void(bool)>& callback) OVERRIDE;

  void RegisterRenderViewHost(content::RenderViewHost* render_view_host);
  void UnregisterRenderViewHost(content::RenderViewHost* render_view_host);

  // Overridden from content::WebContentsObserver:
  virtual void RenderViewDeleted(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidFailProvisionalLoad(
      int64 frame_id,
      bool is_main_frame,
      const GURL& validated_url,
      int error_code,
      const string16& error_description,
      content::RenderViewHost* render_view_host) OVERRIDE;

  // Message handlers
  void OnForwardMessageToExternalHost(const std::string& message,
                                      const std::string& origin,
                                      const std::string& target);

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

  // Overridden from ui::AcceleratorTarget:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;
  virtual bool CanHandleAccelerators() const OVERRIDE;

  void set_pending(bool pending) { pending_ = pending; }
  bool pending() const { return pending_; }

  void set_is_popup_window(bool is_popup_window) {
    is_popup_window_ = is_popup_window;
  }

  // Overridden from InfoBarContainer::Delegate:
  virtual SkColor GetInfoBarSeparatorColor() const OVERRIDE;
  virtual void InfoBarContainerStateChanged(bool is_animating) OVERRIDE;
  virtual bool DrawInfoBarArrows(int* x) const OVERRIDE;

  // Overridden from BlockedContentTabHelperDelegate:
  virtual content::WebContents* GetConstrainingWebContents(
      content::WebContents* source) OVERRIDE;

 protected:
  virtual ~ExternalTabContainerWin();

  // WidgetObserver overrides.
  virtual void OnWidgetCreated(views::Widget* widget) OVERRIDE;
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;
  virtual void OnWidgetDestroyed(views::Widget* widget) OVERRIDE;

  bool InitNavigationInfo(NavigationInfo* nav_info,
                          content::NavigationType nav_type,
                          int relative_offset);
  void Navigate(const GURL& url, const GURL& referrer);

  // Helper resource automation registration method, allowing registration of
  // pending RenderViewHosts.
  void RegisterRenderViewHostForAutomation(
      content::RenderViewHost* render_view_host,
      bool pending_view);

  // Helper function for processing keystokes coming back from the renderer
  // process.
  bool ProcessUnhandledKeyStroke(HWND window, UINT message, WPARAM wparam,
                                 LPARAM lparam);

  void LoadAccelerators();

  // Sends over pending Open URL requests to the external host.
  void ServicePendingOpenURLRequests();

  // Scheduled as a task in ExternalTabContainerWin::Reinitialize.
  void OnReinitialize();

  // Creates and initializes the view hierarchy for this
  // ExternalTabContainerWin.
  void SetupExternalTabView();

  views::Widget* widget_;
  scoped_ptr<content::WebContents> web_contents_;
  scoped_refptr<AutomationProvider> automation_;

  content::NotificationRegistrar registrar_;

  // A view to handle focus cycling
  views::WebView* tab_contents_container_;

  int tab_handle_;
  // A failed navigation like a 404 is followed in chrome with a success
  // navigation for the 404 page. We need to ignore the next navigation
  // to avoid confusing the clients of the external tab. This member variable
  // is set when we need to ignore the next load notification.
  bool ignore_next_load_notification_;

  scoped_ptr<RenderViewContextMenuViews> external_context_menu_;

  // A message filter to load resources via automation
  scoped_refptr<AutomationResourceMessageFilter>
      automation_resource_message_filter_;

  // If all the url requests for this tab are to be loaded via automation.
  bool load_requests_via_automation_;

  // whether top level URL requests are to be handled by the automation client.
  bool handle_top_level_requests_;

  // Set to true if the host needs to get notified of all top level navigations
  // in this page. This typically applies to hosts which would render the new
  // page without chrome frame.
  bool route_all_top_level_navigations_;

  // Contains ExternalTabContainers that have not been connected to as yet.
  static base::LazyInstance<PendingTabs> pending_tabs_;

  // Allows us to run tasks on the ExternalTabContainerWin instance which are
  // bound by its lifetime.
  base::WeakPtrFactory<ExternalTabContainerWin> weak_factory_;

  // The URL request context to be used for this tab. Can be NULL.
  scoped_refptr<ChromeURLRequestContextGetter> request_context_;

  UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  // A mapping between accelerators and commands.
  std::map<ui::Accelerator, int> accelerator_table_;

  // Top level navigations received for a tab while it is waiting for an ack
  // from the external host go here. Scenario is a window.open executes on a
  // page in ChromeFrame. A new WebContents is created and the current
  // ExternalTabContainerWin is notified via AddNewContents. At this point we
  // send off an attach tab request to the host browser. Before the host
  // browser sends over the ack, we receive a top level URL navigation for the
  // new tab, which needs to be routed over the correct automation channel.
  // We receive the automation channel only when the external host acks the
  // attach tab request.
  // Contains the list of URL requests which are pending waiting for an ack
  // from the external host.
  std::vector<content::OpenURLParams> pending_open_url_requests_;

  // Set to true if the ExternalTabContainerWin instance is waiting for an ack
  // from the host.
  bool pending_;

  views::FocusManager* focus_manager_;

  views::View* external_tab_view_;

  IPC::Message* unload_reply_message_;

  scoped_ptr<ui::ViewProp> prop_;

  // if this tab is a popup
  bool is_popup_window_;

  DISALLOW_COPY_AND_ASSIGN(ExternalTabContainerWin);
};

// This class is instantiated for handling requests to open popups for external
// tabs hosted in browsers which need to be notified about all top level
// navigations. An instance of this class is created for handling window.open
// or link navigations with target blank, etc.
class TemporaryPopupExternalTabContainerWin : public ExternalTabContainerWin {
 public:
  TemporaryPopupExternalTabContainerWin(
      AutomationProvider* automation,
      AutomationResourceMessageFilter* filter);
  virtual ~TemporaryPopupExternalTabContainerWin();

  virtual bool OnGoToEntryOffset(int offset) {
    NOTREACHED();
    return false;
  }

  virtual bool ProcessUnhandledKeyStroke(HWND window, UINT message,
                                         WPARAM wparam, LPARAM lparam) {
    NOTREACHED();
    return false;
  }

  virtual void Observe(int type, const content::NotificationSource& source,
                       const content::NotificationDetails& details) {}

  virtual content::WebContents* OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) OVERRIDE;

  virtual void NavigationStateChanged(const content::WebContents* source,
                                      unsigned changed_flags) {
    NOTREACHED();
  }

  virtual void CloseContents(content::WebContents* source) {
    NOTREACHED();
  }

  virtual void UpdateTargetURL(content::WebContents* source, int32 page_id,
                               const GURL& url) {
    NOTREACHED();
  }

  void ForwardMessageToExternalHost(const std::string& message,
                                    const std::string& origin,
                                    const std::string& target) {
    NOTREACHED();
  }

  virtual bool TakeFocus(bool reverse) {
    NOTREACHED();
    return false;
  }

  virtual bool HandleContextMenu(const content::ContextMenuParams& params) {
    NOTREACHED();
    return false;
  }

  virtual void BeforeUnloadFired(content::WebContents* tab, bool proceed,
                                 bool* proceed_to_fire_unload) {
    NOTREACHED();
  }
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTERNAL_TAB_CONTAINER_WIN_H_
