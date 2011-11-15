// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTERNAL_TAB_CONTAINER_WIN_H_
#define CHROME_BROWSER_EXTERNAL_TAB_CONTAINER_WIN_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/automation/automation_resource_message_filter.h"
#include "chrome/browser/infobars/infobar_container.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper_delegate.h"
#include "chrome/browser/ui/views/frame/browser_bubble_host.h"
#include "chrome/browser/ui/views/unhandled_keyboard_event_handler.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "content/public/browser/navigation_types.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/models/accelerator.h"
#include "views/widget/native_widget_win.h"

class AutomationProvider;
class Browser;
class Profile;
class TabContentsContainer;
class TabContentsWrapper;
class RenderViewContextMenuViews;
struct NavigationInfo;

namespace ui {
class ViewProp;
}

// This class serves as the container window for an external tab.
// An external tab is a Chrome tab that is meant to displayed in an
// external process. This class provides the FocusManger needed by the
// TabContents as well as an implementation of TabContentsDelegate.
class ExternalTabContainer : public TabContentsDelegate,
                             public TabContentsObserver,
                             public content::NotificationObserver,
                             public views::NativeWidgetWin,
                             public base::RefCounted<ExternalTabContainer>,
                             public ui::AcceleratorTarget,
                             public InfoBarContainer::Delegate,
                             public BrowserBubbleHost,
                             public BlockedContentTabHelperDelegate {
 public:
  typedef std::map<uintptr_t, scoped_refptr<ExternalTabContainer> > PendingTabs;

  ExternalTabContainer(AutomationProvider* automation,
                       AutomationResourceMessageFilter* filter);

  TabContents* tab_contents() const;
  TabContentsWrapper* tab_contents_wrapper() { return tab_contents_.get(); }

  // Temporary hack so we can send notifications back
  void SetTabHandle(int handle);

  int tab_handle() const {
    return tab_handle_;
  }

  bool Init(Profile* profile,
            HWND parent,
            const gfx::Rect& bounds,
            DWORD style,
            bool load_requests_via_automation,
            bool handle_top_level_requests,
            TabContentsWrapper* existing_tab_contents,
            const GURL& initial_url,
            const GURL& referrer,
            bool infobars_enabled,
            bool supports_full_tab_mode);

  // Unhook the keystroke listener and notify about the closing TabContents.
  // This function gets called from three places, which is fine.
  // 1. OnFinalMessage
  // 2. In the destructor.
  // 3. In AutomationProvider::CreateExternalTab
  void Uninitialize();

  // Used to reinitialize the automation channel and related information
  // for this container. Typically used when an ExternalTabContainer
  // instance is created by Chrome and attached to an automation client.
  bool Reinitialize(AutomationProvider* automation_provider,
                    AutomationResourceMessageFilter* filter,
                    gfx::NativeWindow parent_window);

  // This is invoked when the external host reflects back to us a keyboard
  // message it did not process
  void ProcessUnhandledAccelerator(const MSG& msg);

  // See TabContents::FocusThroughTabTraversal.  Called from AutomationProvider.
  void FocusThroughTabTraversal(bool reverse, bool restore_focus_to_view);

  // A helper method that tests whether the given window is an
  // ExternalTabContainer window
  static bool IsExternalTabContainer(HWND window);

  // A helper function that returns a pointer to the ExternalTabContainer
  // instance associated with a native view.  Returns NULL if the window
  // is not an ExternalTabContainer.
  static ExternalTabContainer* GetExternalContainerFromNativeWindow(
      gfx::NativeView native_window);

  // A helper method that retrieves the ExternalTabContainer object that
  // hosts the given tab window.
  static ExternalTabContainer* GetContainerForTab(HWND tab_window);

  // Overridden from TabContentsDelegate:

  // Deprecated. Please used two-arguments variant.
  // TODO(adriansc): Remove method once refactoring changed all call sites.
  virtual TabContents* OpenURLFromTab(
      TabContents* source,
      const GURL& url,
      const GURL& referrer,
      WindowOpenDisposition disposition,
      content::PageTransition transition) OVERRIDE;
  virtual TabContents* OpenURLFromTab(TabContents* source,
                                      const OpenURLParams& params) OVERRIDE;
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags) OVERRIDE;
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture) OVERRIDE;
  virtual void CloseContents(TabContents* source) OVERRIDE;
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos) OVERRIDE;
  virtual bool IsPopupOrPanel(const TabContents* source) const OVERRIDE;
  virtual void UpdateTargetURL(TabContents* source, int32 page_id,
                               const GURL& url) OVERRIDE;
  virtual void ContentsZoomChange(bool zoom_in) OVERRIDE;
  virtual gfx::NativeWindow GetFrameNativeWindow() OVERRIDE;
  virtual bool PreHandleKeyboardEvent(const NativeWebKeyboardEvent& event,
                                      bool* is_keyboard_shortcut) OVERRIDE;
  virtual void HandleKeyboardEvent(
      const NativeWebKeyboardEvent& event) OVERRIDE;
  virtual bool TakeFocus(bool reverse) OVERRIDE;
  virtual bool CanDownload(TabContents* source, int request_id) OVERRIDE;
  virtual bool OnGoToEntryOffset(int offset) OVERRIDE;
  virtual bool HandleContextMenu(const ContextMenuParams& params) OVERRIDE;
  virtual bool ExecuteContextMenuCommand(int command) OVERRIDE;
  virtual void BeforeUnloadFired(TabContents* tab,
                                 bool proceed,
                                 bool* proceed_to_fire_unload) OVERRIDE;
  virtual content::JavaScriptDialogCreator*
      GetJavaScriptDialogCreator() OVERRIDE;
  virtual void ShowRepostFormWarningDialog(TabContents* tab_contents) OVERRIDE;
  virtual void RunFileChooser(
      TabContents* tab,
      const ViewHostMsg_RunFileChooser_Params& params) OVERRIDE;
  virtual void EnumerateDirectory(TabContents* tab,
                                  int request_id,
                                  const FilePath& path) OVERRIDE;
  virtual void JSOutOfMemory(TabContents* tab);
  virtual void RegisterProtocolHandler(TabContents* tab,
                                       const std::string& protocol,
                                       const GURL& url,
                                       const string16& title) OVERRIDE;
  virtual void RegisterIntentHandler(TabContents* tab,
                                     const string16& action,
                                     const string16& type,
                                     const string16& href,
                                     const string16& title,
                                     const string16& disposition) OVERRIDE;
  virtual void WebIntentDispatch(TabContents* tab,
                                 int routing_id,
                                 const webkit_glue::WebIntentData& intent,
                                 int intent_id) OVERRIDE;
  virtual void FindReply(TabContents* tab,
                         int request_id,
                         int number_of_matches,
                         const gfx::Rect& selection_rect,
                         int active_match_ordinal,
                         bool final_update) OVERRIDE;
  virtual void CrashedPlugin(TabContents* tab,
                             const FilePath& plugin_path) OVERRIDE;

  void RegisterRenderViewHost(RenderViewHost* render_view_host);
  void UnregisterRenderViewHost(RenderViewHost* render_view_host);

  // Overridden from TabContentsObserver:
  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message);

  // Message handlers
  void OnForwardMessageToExternalHost(const std::string& message,
                                      const std::string& origin,
                                      const std::string& target);

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

  // Returns the ExternalTabContainer instance associated with the cookie
  // passed in. It also erases the corresponding reference from the map.
  // Returns NULL if we fail to find the cookie in the map.
  static scoped_refptr<ExternalTabContainer> RemovePendingTab(uintptr_t cookie);

  // Handles the specified |accelerator| being pressed.
  bool AcceleratorPressed(const ui::Accelerator& accelerator);

  bool pending() const {
    return pending_;
  }

  void set_pending(bool pending) {
    pending_ = pending;
  }

  void set_is_popup_window(bool is_popup_window) {
    is_popup_window_ = is_popup_window;
  }

  // InfoBarContainer::Delegate overrides
  virtual SkColor GetInfoBarSeparatorColor() const OVERRIDE;
  virtual void InfoBarContainerStateChanged(bool is_animating) OVERRIDE;
  virtual bool DrawInfoBarArrows(int* x) const OVERRIDE;

  virtual void TabContentsCreated(TabContents* new_contents);

  void RunUnloadHandlers(IPC::Message* reply_message);

  // Overridden from BlockedContentTabHelperDelegate:
  virtual TabContentsWrapper* GetConstrainingContentsWrapper(
      TabContentsWrapper* source) OVERRIDE;

 protected:
  ~ExternalTabContainer();
  // Overridden from views::NativeWidgetWin:
  virtual LRESULT OnCreate(LPCREATESTRUCT create_struct);
  virtual void OnDestroy();
  virtual void OnFinalMessage(HWND window);

  bool InitNavigationInfo(NavigationInfo* nav_info,
                          content::NavigationType nav_type,
                          int relative_offset);
  void Navigate(const GURL& url, const GURL& referrer);

  friend class base::RefCounted<ExternalTabContainer>;

  // Helper resource automation registration method, allowing registration of
  // pending RenderViewHosts.
  void RegisterRenderViewHostForAutomation(RenderViewHost* render_view_host,
                                           bool pending_view);

  // Helper function for processing keystokes coming back from the renderer
  // process.
  bool ProcessUnhandledKeyStroke(HWND window, UINT message, WPARAM wparam,
                                 LPARAM lparam);

  void LoadAccelerators();

  // Sends over pending Open URL requests to the external host.
  void ServicePendingOpenURLRequests();

  // Scheduled as a task in ExternalTabContainer::Reinitialize
  void OnReinitialize();

  // Creates and initializes the view hierarchy for this ExternalTabContainer.
  void SetupExternalTabView();

  scoped_ptr<TabContentsWrapper> tab_contents_;
  scoped_refptr<AutomationProvider> automation_;

  content::NotificationRegistrar registrar_;

  // A view to handle focus cycling
  TabContentsContainer* tab_contents_container_;

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

  // Contains ExternalTabContainers that have not been connected to as yet.
  static base::LazyInstance<PendingTabs> pending_tabs_;

  // Allows us to run tasks on the ExternalTabContainer instance which are
  // bound by its lifetime.
  ScopedRunnableMethodFactory<ExternalTabContainer> external_method_factory_;

  // The URL request context to be used for this tab. Can be NULL.
  scoped_refptr<ChromeURLRequestContextGetter> request_context_;

  UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  // A mapping between accelerators and commands.
  std::map<ui::Accelerator, int> accelerator_table_;

  // Top level navigations received for a tab while it is waiting for an ack
  // from the external host go here. Scenario is a window.open executes on a
  // page in ChromeFrame. A new TabContents is created and the current
  // ExternalTabContainer is notified via AddNewContents. At this point we
  // send off an attach tab request to the host browser. Before the host
  // browser sends over the ack, we receive a top level URL navigation for the
  // new tab, which needs to be routed over the correct automation channel.
  // We receive the automation channel only when the external host acks the
  // attach tab request.
  // Contains the list of URL requests which are pending waiting for an ack
  // from the external host.
  std::vector<OpenURLParams> pending_open_url_requests_;

  // Set to true if the ExternalTabContainer instance is waiting for an ack
  // from the host.
  bool pending_;

  views::FocusManager* focus_manager_;

  views::View* external_tab_view_;

  IPC::Message* unload_reply_message_;

  // set to true if the host needs to get notified of all top level navigations
  // in this page. This typically applies to hosts which would render the new
  // page without chrome frame.
  bool route_all_top_level_navigations_;

  scoped_ptr<ui::ViewProp> prop_;

  // if this tab is a popup
  bool is_popup_window_;

  DISALLOW_COPY_AND_ASSIGN(ExternalTabContainer);
};

// This class is instantiated for handling requests to open popups for external
// tabs hosted in browsers which need to be notified about all top level
// navigations. An instance of this class is created for handling window.open
// or link navigations with target blank, etc.
class TemporaryPopupExternalTabContainer : public ExternalTabContainer {
 public:
  TemporaryPopupExternalTabContainer(AutomationProvider* automation,
      AutomationResourceMessageFilter* filter);
  virtual ~TemporaryPopupExternalTabContainer();

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

  // Deprecated. Please use the two-argument variant.
  // TODO(adriansc): Remove method once refactoring changed all call sites.
  virtual TabContents* OpenURLFromTab(
      TabContents* source, const GURL& url,
      const GURL& referrer,
      WindowOpenDisposition disposition,
      content::PageTransition transition) OVERRIDE;
  virtual TabContents* OpenURLFromTab(TabContents* source,
                                      const OpenURLParams& params) OVERRIDE;

  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags) {
    NOTREACHED();
  }

  virtual void CloseContents(TabContents* source) {
    NOTREACHED();
  }

  virtual void UpdateTargetURL(TabContents* source, int32 page_id,
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

  virtual bool HandleContextMenu(const ContextMenuParams& params) {
    NOTREACHED();
    return false;
  }

  virtual void BeforeUnloadFired(TabContents* tab, bool proceed,
                                 bool* proceed_to_fire_unload) {
    NOTREACHED();
  }
};

#endif  // CHROME_BROWSER_EXTERNAL_TAB_CONTAINER_WIN_H_
