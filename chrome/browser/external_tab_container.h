// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTERNAL_TAB_CONTAINER_H_
#define CHROME_BROWSER_EXTERNAL_TAB_CONTAINER_H_

#include <vector>
#include <map>

#include "chrome/browser/automation/automation_resource_message_filter.h"
#include "chrome/browser/automation/automation_profile_impl.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/tab_contents/tab_contents_delegate.h"
#include "chrome/common/navigation_types.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "views/widget/widget_win.h"

class AutomationProvider;
class Profile;
class TabContentsContainer;
class RenderViewContextMenuExternalWin;

namespace IPC {
struct NavigationInfo;
}

// This class serves as the container window for an external tab.
// An external tab is a Chrome tab that is meant to displayed in an
// external process. This class provides the FocusManger needed by the
// TabContents as well as an implementation of TabContentsDelagate.
class ExternalTabContainer : public TabContentsDelegate,
                             public NotificationObserver,
                             public views::WidgetWin,
                             public base::RefCounted<ExternalTabContainer> {
 public:
  typedef std::map<intptr_t, scoped_refptr<ExternalTabContainer> > PendingTabs;

  ExternalTabContainer(AutomationProvider* automation,
      AutomationResourceMessageFilter* filter);
  ~ExternalTabContainer();

  TabContents* tab_contents() const { return tab_contents_; }

  // Temporary hack so we can send notifications back
  void set_tab_handle(int handle) {
    tab_handle_ = handle;
    if (automation_profile_.get())
      automation_profile_->set_tab_handle(handle);
  }

  int tab_handle() const {
    return tab_handle_;
  }

  bool Init(Profile* profile,
            HWND parent,
            const gfx::Rect& bounds,
            DWORD style,
            bool load_requests_via_automation,
            bool handle_top_level_requests,
            TabContents* existing_tab_contents,
            const GURL& initial_url);

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
                    AutomationResourceMessageFilter* filter);

  // This is invoked when the external host reflects back to us a keyboard
  // message it did not process
  void ProcessUnhandledAccelerator(const MSG& msg);

  // See TabContents::FocusThroughTabTraversal.  Called from AutomationProvider.
  void FocusThroughTabTraversal(bool reverse);

  // A helper method that tests whether the given window is an
  // ExternalTabContainer window
  static bool IsExternalTabContainer(HWND window);

  // A helper method that retrieves the ExternalTabContainer object that
  // hosts the given tab window.
  static ExternalTabContainer* GetContainerForTab(HWND tab_window);

  // Overridden from TabContentsDelegate:
  virtual void OpenURLFromTab(TabContents* source,
                              const GURL& url,
                              const GURL& referrer,
                              WindowOpenDisposition disposition,
                              PageTransition::Type transition);
  virtual void NavigationStateChanged(const TabContents* source,
                                      unsigned changed_flags);
  virtual void AddNewContents(TabContents* source,
                              TabContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture);
  virtual void ActivateContents(TabContents* contents);
  virtual void LoadingStateChanged(TabContents* source);
  virtual void CloseContents(TabContents* source);
  virtual void MoveContents(TabContents* source, const gfx::Rect& pos);
  virtual bool IsPopup(TabContents* source);
  virtual void URLStarredChanged(TabContents* source, bool starred);
  virtual void UpdateTargetURL(TabContents* source, const GURL& url);
  virtual void ContentsZoomChange(bool zoom_in);
  virtual void ToolbarSizeChanged(TabContents* source, bool is_animating);
  virtual void ForwardMessageToExternalHost(const std::string& message,
                                            const std::string& origin,
                                            const std::string& target);
  virtual bool IsExternalTabContainer() const {
    return true;
  };

  virtual bool HandleKeyboardEvent(const NativeWebKeyboardEvent& event);

  virtual bool TakeFocus(bool reverse);
  virtual bool OnGoToEntryOffset(int offset);

  virtual void ShowPageInfo(Profile* profile,
                            const GURL& url,
                            const NavigationEntry::SSLStatus& ssl,
                            bool show_history);

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Handles the context menu display operation. This allows external
  // hosts to customize the menu.
  virtual bool HandleContextMenu(const ContextMenuParams& params);

  // Executes the context menu command identified by the command
  // parameter.
  virtual bool ExecuteContextMenuCommand(int command);

  // Show a dialog with HTML content. |delegate| contains a pointer to the
  // delegate who knows how to display the dialog (which file URL and JSON
  // string input to use during initialization). |parent_window| is the window
  // that should be parent of the dialog, or NULL for the default.
  virtual void ShowHtmlDialog(HtmlDialogUIDelegate* delegate,
                              gfx::NativeWindow parent_window);

  // Returns the ExternalTabContainer instance associated with the cookie
  // passed in. It also erases the corresponding reference from the map.
  // Returns NULL if we fail to find the cookie in the map.
  static ExternalTabContainer* RemovePendingTab(intptr_t cookie);

 protected:
  // Overridden from views::WidgetWin:
  virtual LRESULT OnCreate(LPCREATESTRUCT create_struct);
  virtual void OnDestroy();
  virtual void OnFinalMessage(HWND window);

  bool InitNavigationInfo(IPC::NavigationInfo* nav_info,
                          NavigationType::Type nav_type,
                          int relative_offset);
  void Navigate(const GURL& url, const GURL& referrer);

 private:
  // Helper function for processing keystokes coming back from the renderer
  // process.
  bool ProcessUnhandledKeyStroke(HWND window, UINT message, WPARAM wparam,
                                 LPARAM lparam);

  TabContents* tab_contents_;
  scoped_refptr<AutomationProvider> automation_;

  NotificationRegistrar registrar_;

  // A view to handle focus cycling
  TabContentsContainer* tab_contents_container_;

  int tab_handle_;
  // A failed navigation like a 404 is followed in chrome with a success
  // navigation for the 404 page. We need to ignore the next navigation
  // to avoid confusing the clients of the external tab. This member variable
  // is set when we need to ignore the next load notification.
  bool ignore_next_load_notification_;

  // Contains the list of disabled context menu identifiers.
  std::vector<int> disabled_context_menu_ids_;

  scoped_ptr<RenderViewContextMenuExternalWin> external_context_menu_;

  // A message filter to load resources via automation
  AutomationResourceMessageFilter* automation_resource_message_filter_;

  // If all the url requests for this tab are to be loaded via automation.
  bool load_requests_via_automation_;

  // whether top level URL requests are to be handled by the automation client.
  bool handle_top_level_requests_;

  // Scoped browser object for this ExternalTabContainer instance.
  scoped_ptr<Browser> browser_;

  // A customized profile for automation specific needs.
  scoped_ptr<AutomationProfileImpl> automation_profile_;

  // Contains ExternalTabContainers that have not been connected to as yet.
  static PendingTabs pending_tabs_;

  DISALLOW_COPY_AND_ASSIGN(ExternalTabContainer);
};

#endif  // CHROME_BROWSER_EXTERNAL_TAB_CONTAINER_H_
