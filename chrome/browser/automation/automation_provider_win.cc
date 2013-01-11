// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_provider.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/debug/trace_event.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/automation/automation_browser_tracker.h"
#include "chrome/browser/automation/automation_tab_tracker.h"
#include "chrome/browser/automation/automation_window_tracker.h"
#include "chrome/browser/external_tab/external_tab_container.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/automation_messages.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_zoom.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/views/focus/accelerator_handler.h"
#include "ui/views/widget/root_view.h"

using content::NavigationController;
using content::RenderViewHost;
using content::WebContents;

void AutomationProvider::CreateExternalTab(
    const ExternalTabSettings& settings,
    gfx::NativeWindow* tab_container_window, gfx::NativeWindow* tab_window,
    int* tab_handle, int* session_id) {
  TRACE_EVENT_BEGIN_ETW("AutomationProvider::CreateExternalTab", 0, "");

  *tab_handle = 0;
  *tab_container_window = NULL;
  *tab_window = NULL;
  *session_id = -1;
  scoped_refptr<ExternalTabContainer> external_tab_container =
      ExternalTabContainer::Create(this, automation_resource_message_filter_);

  Profile* profile = settings.is_incognito ?
      profile_->GetOffTheRecordProfile() : profile_;

  // When the ExternalTabContainer window is created we grab a reference on it
  // which is released when the window is destroyed.
  external_tab_container->Init(profile, settings.parent, settings.dimensions,
      settings.style, settings.load_requests_via_automation,
      settings.handle_top_level_requests, NULL, settings.initial_url,
      settings.referrer, settings.infobars_enabled,
      settings.route_all_top_level_navigations);

  if (AddExternalTab(external_tab_container)) {
    WebContents* web_contents = external_tab_container->GetWebContents();
    SessionTabHelper* session_tab_helper =
        SessionTabHelper::FromWebContents(web_contents);
    *tab_handle = external_tab_container->GetTabHandle();
    *tab_container_window = external_tab_container->GetExternalTabNativeView();
    *tab_window = web_contents->GetNativeView();
    *session_id = session_tab_helper->session_id().id();
  } else {
    external_tab_container->Uninitialize();
  }

  TRACE_EVENT_END_ETW("AutomationProvider::CreateExternalTab", 0, "");
}

bool AutomationProvider::AddExternalTab(ExternalTabContainer* external_tab) {
  DCHECK(external_tab != NULL);

  WebContents* web_contents = external_tab->GetWebContents();
  if (web_contents) {
    int tab_handle = tab_tracker_->Add(&web_contents->GetController());
    external_tab->SetTabHandle(tab_handle);
    return true;
  }

  return false;
}

void AutomationProvider::ProcessUnhandledAccelerator(
    const IPC::Message& message, int handle, const MSG& msg) {
  ExternalTabContainer* external_tab = GetExternalTabForHandle(handle);
  if (external_tab) {
    external_tab->ProcessUnhandledAccelerator(msg);
  }
  // This message expects no response.
}

void AutomationProvider::SetInitialFocus(const IPC::Message& message,
                                         int handle, bool reverse,
                                         bool restore_focus_to_view) {
  ExternalTabContainer* external_tab = GetExternalTabForHandle(handle);
  if (external_tab) {
    external_tab->FocusThroughTabTraversal(reverse, restore_focus_to_view);
  }
  // This message expects no response.
}

void AutomationProvider::PrintAsync(int tab_handle) {
  WebContents* web_contents = GetWebContentsForHandle(tab_handle, NULL);
  if (!web_contents)
    return;

  printing::PrintViewManager* print_view_manager =
      printing::PrintViewManager::FromWebContents(web_contents);
  print_view_manager->PrintNow();
}

ExternalTabContainer* AutomationProvider::GetExternalTabForHandle(int handle) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    return ExternalTabContainer::GetContainerForTab(
        tab->GetWebContents()->GetNativeView());
  }

  return NULL;
}

void AutomationProvider::OnTabReposition(
    int tab_handle, const Reposition_Params& params) {
  if (!tab_tracker_->ContainsHandle(tab_handle))
    return;

  if (!IsWindow(params.window))
    return;

  unsigned long process_id = 0;
  unsigned long thread_id = 0;

  thread_id = GetWindowThreadProcessId(params.window, &process_id);

  if (thread_id != GetCurrentThreadId()) {
    DCHECK_EQ(thread_id, GetCurrentThreadId());
    return;
  }

  SetWindowPos(params.window, params.window_insert_after, params.left,
               params.top, params.width, params.height, params.flags);

  if (params.set_parent) {
    if (IsWindow(params.parent_window)) {
      if (!SetParent(params.window, params.parent_window))
        DLOG(WARNING) << "SetParent failed. Error 0x%x" << GetLastError();
    }
  }
}

void AutomationProvider::OnForwardContextMenuCommandToChrome(int tab_handle,
                                                             int command) {
  ExternalTabContainer* external_tab = GetExternalTabForHandle(tab_handle);
  if (external_tab)
    external_tab->ExecuteContextMenuCommand(command);
}

void AutomationProvider::ConnectExternalTab(
    uint64 cookie,
    bool allow,
    gfx::NativeWindow parent_window,
    gfx::NativeWindow* tab_container_window,
    gfx::NativeWindow* tab_window,
    int* tab_handle,
    int* session_id) {
  TRACE_EVENT_BEGIN_ETW("AutomationProvider::ConnectExternalTab", 0, "");

  *tab_handle = 0;
  *tab_container_window = NULL;
  *tab_window = NULL;
  *session_id = -1;

  scoped_refptr<ExternalTabContainer> external_tab_container =
      ExternalTabContainer::RemovePendingTab(static_cast<uintptr_t>(cookie));
  if (!external_tab_container.get()) {
    NOTREACHED();
    return;
  }

  if (allow && AddExternalTab(external_tab_container)) {
    external_tab_container->Reinitialize(this,
                                         automation_resource_message_filter_,
                                         parent_window);
    WebContents* web_contents = external_tab_container->GetWebContents();
    SessionTabHelper* session_tab_helper =
        SessionTabHelper::FromWebContents(web_contents);
    *tab_handle = external_tab_container->GetTabHandle();
    *tab_container_window = external_tab_container->GetExternalTabNativeView();
    *tab_window = web_contents->GetNativeView();
    *session_id = session_tab_helper->session_id().id();
  } else {
    external_tab_container->Uninitialize();
  }

  TRACE_EVENT_END_ETW("AutomationProvider::ConnectExternalTab", 0, "");
}

void AutomationProvider::OnBrowserMoved(int tab_handle) {
  ExternalTabContainer* external_tab = GetExternalTabForHandle(tab_handle);
  if (!external_tab) {
    DLOG(WARNING) <<
      "AutomationProvider::OnBrowserMoved called with invalid tab handle.";
  }
}

void AutomationProvider::OnMessageFromExternalHost(int handle,
                                                   const std::string& message,
                                                   const std::string& origin,
                                                   const std::string& target) {
  RenderViewHost* view_host = GetViewForTab(handle);
  if (!view_host)
    return;

  view_host->Send(new ChromeViewMsg_HandleMessageFromExternalHost(
      view_host->GetRoutingID(), message, origin, target));
}

void AutomationProvider::NavigateInExternalTab(
    int handle, const GURL& url, const GURL& referrer,
    AutomationMsg_NavigationResponseValues* status) {
  *status = AUTOMATION_MSG_NAVIGATION_ERROR;

  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    tab->LoadURL(
        url,
        content::Referrer(referrer, WebKit::WebReferrerPolicyDefault),
        content::PAGE_TRANSITION_TYPED, std::string());
    *status = AUTOMATION_MSG_NAVIGATION_SUCCESS;
  }
}

void AutomationProvider::NavigateExternalTabAtIndex(
    int handle, int navigation_index,
    AutomationMsg_NavigationResponseValues* status) {
  *status = AUTOMATION_MSG_NAVIGATION_ERROR;

  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    tab->GoToIndex(navigation_index);
    *status = AUTOMATION_MSG_NAVIGATION_SUCCESS;
  }
}

void AutomationProvider::OnRunUnloadHandlers(
    int handle, IPC::Message* reply_message) {
  ExternalTabContainer* external_tab = GetExternalTabForHandle(handle);
  if (external_tab) {
    external_tab->RunUnloadHandlers(reply_message);
  }
}

void AutomationProvider::OnSetZoomLevel(int handle, int zoom_level) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    if (tab->GetWebContents() && tab->GetWebContents()->GetRenderViewHost()) {
      RenderViewHost* host = tab->GetWebContents()->GetRenderViewHost();
      content::PageZoom zoom = static_cast<content::PageZoom>(zoom_level);
      host->Zoom(zoom);
    }
  }
}
