// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_provider.h"

#include <set>

#include "app/message_box_flags.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/keyboard_codes.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/thread.h"
#include "base/trace_event.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "base/waitable_event.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/app_modal_dialog.h"
#include "chrome/browser/app_modal_dialog_queue.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/automation/automation_autocomplete_edit_tracker.h"
#include "chrome/browser/automation/automation_browser_tracker.h"
#include "chrome/browser/automation/automation_extension_tracker.h"
#include "chrome/browser/automation/automation_provider_json.h"
#include "chrome/browser/automation/automation_provider_list.h"
#include "chrome/browser/automation/automation_provider_observers.h"
#include "chrome/browser/automation/automation_resource_message_filter.h"
#include "chrome/browser/automation/automation_tab_tracker.h"
#include "chrome/browser/automation/automation_window_tracker.h"
#include "chrome/browser/automation/extension_port_container.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/blocked_popup_container.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_storage.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/browsing_data_remover.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/dom_operation_notification_details.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/download/download_item.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/download/save_package.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/browser/find_bar.h"
#include "chrome/browser/find_bar_controller.h"
#include "chrome/browser/find_notification_details.h"
#include "chrome/browser/host_content_settings_map.h"
#include "chrome/browser/importer/importer.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/location_bar.h"
#include "chrome/browser/login_prompt.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/ssl/ssl_manager.h"
#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "chrome/common/automation_constants.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/automation_messages.h"
#include "chrome/test/automation/tab_proxy.h"
#include "net/proxy/proxy_service.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/url_request/url_request_context.h"
#include "chrome/browser/automation/ui_controls.h"
#include "views/event.h"
#include "webkit/glue/password_form.h"
#include "webkit/glue/plugins/plugin_list.h"

#if defined(OS_WIN)
#include "chrome/browser/external_tab_container_win.h"
#endif  // defined(OS_WIN)

using base::Time;

AutomationProvider::AutomationProvider(Profile* profile)
    : profile_(profile),
      reply_message_(NULL) {
  TRACE_EVENT_BEGIN("AutomationProvider::AutomationProvider", 0, "");

  browser_tracker_.reset(new AutomationBrowserTracker(this));
  extension_tracker_.reset(new AutomationExtensionTracker(this));
  tab_tracker_.reset(new AutomationTabTracker(this));
  window_tracker_.reset(new AutomationWindowTracker(this));
  autocomplete_edit_tracker_.reset(
      new AutomationAutocompleteEditTracker(this));
  new_tab_ui_load_observer_.reset(new NewTabUILoadObserver(this));
  dom_operation_observer_.reset(new DomOperationNotificationObserver(this));
  metric_event_duration_observer_.reset(new MetricEventDurationObserver());
  extension_test_result_observer_.reset(
      new ExtensionTestResultNotificationObserver(this));
  g_browser_process->AddRefModule();

  TRACE_EVENT_END("AutomationProvider::AutomationProvider", 0, "");
}

AutomationProvider::~AutomationProvider() {
  STLDeleteContainerPairSecondPointers(port_containers_.begin(),
                                       port_containers_.end());
  port_containers_.clear();

  // Make sure that any outstanding NotificationObservers also get destroyed.
  ObserverList<NotificationObserver>::Iterator it(notification_observer_list_);
  NotificationObserver* observer;
  while ((observer = it.GetNext()) != NULL)
    delete observer;

  if (channel_.get()) {
    channel_->Close();
  }
  g_browser_process->ReleaseModule();
}

void AutomationProvider::ConnectToChannel(const std::string& channel_id) {
  TRACE_EVENT_BEGIN("AutomationProvider::ConnectToChannel", 0, "");

  automation_resource_message_filter_ = new AutomationResourceMessageFilter;
  channel_.reset(
      new IPC::SyncChannel(channel_id, IPC::Channel::MODE_CLIENT, this,
                           automation_resource_message_filter_,
                           g_browser_process->io_thread()->message_loop(),
                           true, g_browser_process->shutdown_event()));
  chrome::VersionInfo version_info;

  // Send a hello message with our current automation protocol version.
  channel_->Send(new AutomationMsg_Hello(0, version_info.Version().c_str()));

  TRACE_EVENT_END("AutomationProvider::ConnectToChannel", 0, "");
}

void AutomationProvider::SetExpectedTabCount(size_t expected_tabs) {
  if (expected_tabs == 0) {
    Send(new AutomationMsg_InitialLoadsComplete(0));
  } else {
    initial_load_observer_.reset(new InitialLoadObserver(expected_tabs, this));
  }
}

NotificationObserver* AutomationProvider::AddNavigationStatusListener(
    NavigationController* tab, IPC::Message* reply_message,
    int number_of_navigations, bool include_current_navigation) {
  NotificationObserver* observer =
      new NavigationNotificationObserver(tab, this, reply_message,
                                         number_of_navigations,
                                         include_current_navigation);

  notification_observer_list_.AddObserver(observer);
  return observer;
}

void AutomationProvider::RemoveNavigationStatusListener(
    NotificationObserver* obs) {
  notification_observer_list_.RemoveObserver(obs);
}

NotificationObserver* AutomationProvider::AddTabStripObserver(
    Browser* parent,
    IPC::Message* reply_message) {
  NotificationObserver* observer =
      new TabAppendedNotificationObserver(parent, this, reply_message);
  notification_observer_list_.AddObserver(observer);

  return observer;
}

void AutomationProvider::RemoveTabStripObserver(NotificationObserver* obs) {
  notification_observer_list_.RemoveObserver(obs);
}

void AutomationProvider::AddLoginHandler(NavigationController* tab,
                                         LoginHandler* handler) {
  login_handler_map_[tab] = handler;
}

void AutomationProvider::RemoveLoginHandler(NavigationController* tab) {
  DCHECK(login_handler_map_[tab]);
  login_handler_map_.erase(tab);
}

void AutomationProvider::AddPortContainer(ExtensionPortContainer* port) {
  int port_id = port->port_id();
  DCHECK_NE(-1, port_id);
  DCHECK(port_containers_.find(port_id) == port_containers_.end());

  port_containers_[port_id] = port;
}

void AutomationProvider::RemovePortContainer(ExtensionPortContainer* port) {
  int port_id = port->port_id();
  DCHECK_NE(-1, port_id);

  PortContainerMap::iterator it = port_containers_.find(port_id);
  DCHECK(it != port_containers_.end());

  if (it != port_containers_.end()) {
    delete it->second;
    port_containers_.erase(it);
  }
}

ExtensionPortContainer* AutomationProvider::GetPortContainer(
    int port_id) const {
  PortContainerMap::const_iterator it = port_containers_.find(port_id);
  if (it == port_containers_.end())
    return NULL;

  return it->second;
}

int AutomationProvider::GetIndexForNavigationController(
    const NavigationController* controller, const Browser* parent) const {
  DCHECK(parent);
  return parent->GetIndexOfController(controller);
}

int AutomationProvider::AddExtension(Extension* extension) {
  DCHECK(extension);
  return extension_tracker_->Add(extension);
}

Extension* AutomationProvider::GetExtension(int extension_handle) {
  return extension_tracker_->GetResource(extension_handle);
}

Extension* AutomationProvider::GetEnabledExtension(int extension_handle) {
  Extension* extension = extension_tracker_->GetResource(extension_handle);
  ExtensionsService* service = profile_->GetExtensionsService();
  if (extension && service &&
      service->GetExtensionById(extension->id(), false))
    return extension;
  return NULL;
}

Extension* AutomationProvider::GetDisabledExtension(int extension_handle) {
  Extension* extension = extension_tracker_->GetResource(extension_handle);
  ExtensionsService* service = profile_->GetExtensionsService();
  if (extension && service &&
      service->GetExtensionById(extension->id(), true) &&
      !service->GetExtensionById(extension->id(), false))
    return extension;
  return NULL;
}

void AutomationProvider::OnMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(AutomationProvider, message)
#if !defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_WindowDrag,
                                    WindowSimulateDrag)
#endif  // !defined(OS_MACOSX)
#if defined(OS_WIN)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabHWND, GetTabHWND)
#endif  // defined(OS_WIN)
    IPC_MESSAGE_HANDLER(AutomationMsg_HandleUnused, HandleUnused)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetProxyConfig, SetProxyConfig);
    IPC_MESSAGE_HANDLER(AutomationMsg_PrintAsync, PrintAsync)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_Find, HandleFindRequest)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_SendJSONRequest,
                                    SendJSONRequest)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetPageCurrentEncoding,
                        GetPageCurrentEncoding)
    IPC_MESSAGE_HANDLER(AutomationMsg_OverrideEncoding, OverrideEncoding)
    IPC_MESSAGE_HANDLER(AutomationMsg_SelectAll, SelectAll)
    IPC_MESSAGE_HANDLER(AutomationMsg_Cut, Cut)
    IPC_MESSAGE_HANDLER(AutomationMsg_Copy, Copy)
    IPC_MESSAGE_HANDLER(AutomationMsg_Paste, Paste)
    IPC_MESSAGE_HANDLER(AutomationMsg_ReloadAsync, ReloadAsync)
    IPC_MESSAGE_HANDLER(AutomationMsg_StopAsync, StopAsync)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetPageFontSize, OnSetPageFontSize)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_InstallExtension,
                                    InstallExtension)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_LoadExpandedExtension,
                                    LoadExpandedExtension)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetEnabledExtensions,
                        GetEnabledExtensions)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_WaitForExtensionTestResult,
                                    WaitForExtensionTestResult)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        AutomationMsg_InstallExtensionAndGetHandle,
        InstallExtensionAndGetHandle)
    IPC_MESSAGE_HANDLER(AutomationMsg_UninstallExtension,
                        UninstallExtension)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_EnableExtension,
                                    EnableExtension)
    IPC_MESSAGE_HANDLER(AutomationMsg_DisableExtension,
                        DisableExtension)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        AutomationMsg_ExecuteExtensionActionInActiveTabAsync,
        ExecuteExtensionActionInActiveTabAsync)
    IPC_MESSAGE_HANDLER(AutomationMsg_MoveExtensionBrowserAction,
                        MoveExtensionBrowserAction)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetExtensionProperty,
                        GetExtensionProperty)
    IPC_MESSAGE_HANDLER(AutomationMsg_ShutdownSessionService,
                        ShutdownSessionService)
    IPC_MESSAGE_HANDLER(AutomationMsg_SaveAsAsync, SaveAsAsync)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetContentSetting, SetContentSetting)
    IPC_MESSAGE_HANDLER(AutomationMsg_RemoveBrowsingData, RemoveBrowsingData)
    IPC_MESSAGE_HANDLER(AutomationMsg_ResetToDefaultTheme, ResetToDefaultTheme)
#if defined(OS_WIN)
    // These are for use with external tabs.
    IPC_MESSAGE_HANDLER(AutomationMsg_CreateExternalTab, CreateExternalTab)
    IPC_MESSAGE_HANDLER(AutomationMsg_ProcessUnhandledAccelerator,
                        ProcessUnhandledAccelerator)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetInitialFocus, SetInitialFocus)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabReposition, OnTabReposition)
    IPC_MESSAGE_HANDLER(AutomationMsg_ForwardContextMenuCommandToChrome,
                        OnForwardContextMenuCommandToChrome)
    IPC_MESSAGE_HANDLER(AutomationMsg_NavigateInExternalTab,
                        NavigateInExternalTab)
    IPC_MESSAGE_HANDLER(AutomationMsg_NavigateExternalTabAtIndex,
                        NavigateExternalTabAtIndex)
    IPC_MESSAGE_HANDLER(AutomationMsg_ConnectExternalTab, ConnectExternalTab)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetEnableExtensionAutomation,
                        SetEnableExtensionAutomation)
    IPC_MESSAGE_HANDLER(AutomationMsg_HandleMessageFromExternalHost,
                        OnMessageFromExternalHost)
    IPC_MESSAGE_HANDLER(AutomationMsg_BrowserMove, OnBrowserMoved)
    IPC_MESSAGE_HANDLER(AutomationMsg_RunUnloadHandlers, OnRunUnloadHandlers)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetZoomLevel, OnSetZoomLevel)
#endif  // defined(OS_WIN)
#if defined(OS_CHROMEOS)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_LoginWithUserAndPass,
                                    LoginWithUserAndPass)
#endif  // defined(OS_CHROMEOS)

    IPC_MESSAGE_UNHANDLED(OnUnhandledMessage());
  IPC_END_MESSAGE_MAP()
}

void AutomationProvider::OnUnhandledMessage() {
  // We should not hang here. Print a message to indicate what's going on,
  // and disconnect the channel to notify the caller about the error
  // in a way it can't ignore, and make any further attempts to send
  // messages fail fast.
  LOG(ERROR) << "AutomationProvider received a message it can't handle. "
             << "Please make sure that you use switches::kTestingChannelID "
             << "for test code (TestingAutomationProvider), and "
             << "switches::kAutomationClientChannelID for everything else "
             << "(like ChromeFrame). Closing the automation channel.";
  channel_->Close();
}

void AutomationProvider::ShutdownSessionService(int handle, bool* result) {
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    browser->profile()->ShutdownSessionService();
    *result = true;
  } else {
    *result = false;
  }
}

// This task just adds another task to the event queue.  This is useful if
// you want to ensure that any tasks added to the event queue after this one
// have already been processed by the time |task| is run.
class InvokeTaskLaterTask : public Task {
 public:
  explicit InvokeTaskLaterTask(Task* task) : task_(task) {}
  virtual ~InvokeTaskLaterTask() {}

  virtual void Run() {
    MessageLoop::current()->PostTask(FROM_HERE, task_);
  }

 private:
  Task* task_;

  DISALLOW_COPY_AND_ASSIGN(InvokeTaskLaterTask);
};

void AutomationProvider::HandleUnused(const IPC::Message& message, int handle) {
  if (window_tracker_->ContainsHandle(handle)) {
    window_tracker_->Remove(window_tracker_->GetResource(handle));
  }
}

void AutomationProvider::OnChannelError() {
  LOG(INFO) << "AutomationProxy went away, shutting down app.";
  AutomationProviderList::GetInstance()->RemoveProvider(this);
}

bool AutomationProvider::Send(IPC::Message* msg) {
  DCHECK(channel_.get());
  return channel_->Send(msg);
}

Browser* AutomationProvider::FindAndActivateTab(
    NavigationController* controller) {
  int tab_index;
  Browser* browser = Browser::GetBrowserForController(controller, &tab_index);
  if (browser)
    browser->SelectTabContentsAt(tab_index, true);

  return browser;
}

void AutomationProvider::HandleFindRequest(
    int handle,
    const AutomationMsg_Find_Params& params,
    IPC::Message* reply_message) {
  if (!tab_tracker_->ContainsHandle(handle)) {
    AutomationMsg_FindInPage::WriteReplyParams(reply_message, -1, -1);
    Send(reply_message);
    return;
  }

  NavigationController* nav = tab_tracker_->GetResource(handle);
  TabContents* tab_contents = nav->tab_contents();

  find_in_page_observer_.reset(new
      FindInPageNotificationObserver(this, tab_contents, reply_message));

  tab_contents->set_current_find_request_id(
      FindInPageNotificationObserver::kFindInPageRequestId);
  tab_contents->render_view_host()->StartFinding(
      FindInPageNotificationObserver::kFindInPageRequestId,
      params.search_string, params.forward, params.match_case,
      params.find_next);
}

// Sample json input: { "command": "SetWindowDimensions",
//                      "x": 20,         # optional
//                      "y": 20,         # optional
//                      "width": 800,    # optional
//                      "height": 600 }  # optional
void AutomationProvider::SetWindowDimensions(Browser* browser,
                                             DictionaryValue* args,
                                             IPC::Message* reply_message) {
  gfx::Rect rect = browser->window()->GetRestoredBounds();
  int x, y, width, height;
  if (args->GetInteger("x", &x))
    rect.set_x(x);
  if (args->GetInteger("y", &y))
    rect.set_y(y);
  if (args->GetInteger("width", &width))
    rect.set_width(width);
  if (args->GetInteger("height", &height))
    rect.set_height(height);
  browser->window()->SetBounds(rect);
  AutomationJSONReply(this, reply_message).SendSuccess(NULL);
}

ListValue* AutomationProvider::GetInfobarsInfo(TabContents* tc) {
  // Each infobar may have different properties depending on the type.
  ListValue* infobars = new ListValue;
  for (int infobar_index = 0;
       infobar_index < tc->infobar_delegate_count();
       ++infobar_index) {
    DictionaryValue* infobar_item = new DictionaryValue;
    InfoBarDelegate* infobar = tc->GetInfoBarDelegateAt(infobar_index);
    if (infobar->AsConfirmInfoBarDelegate()) {
      // Also covers ThemeInstalledInfoBarDelegate and
      // CrashedExtensionInfoBarDelegate.
      infobar_item->SetString("type", "confirm_infobar");
      ConfirmInfoBarDelegate* confirm_infobar =
        infobar->AsConfirmInfoBarDelegate();
      infobar_item->SetString("text", confirm_infobar->GetMessageText());
      infobar_item->SetString("link_text", confirm_infobar->GetLinkText());
      ListValue* buttons_list = new ListValue;
      int buttons = confirm_infobar->GetButtons();
      if (ConfirmInfoBarDelegate::BUTTON_OK & buttons) {
        StringValue* button_label = new StringValue(
            confirm_infobar->GetButtonLabel(
              ConfirmInfoBarDelegate::BUTTON_OK));
        buttons_list->Append(button_label);
      }
      if (ConfirmInfoBarDelegate::BUTTON_CANCEL & buttons) {
        StringValue* button_label = new StringValue(
            confirm_infobar->GetButtonLabel(
              ConfirmInfoBarDelegate::BUTTON_CANCEL));
        buttons_list->Append(button_label);
      }
      infobar_item->Set("buttons", buttons_list);
    } else if (infobar->AsAlertInfoBarDelegate()) {
      infobar_item->SetString("type", "alert_infobar");
      AlertInfoBarDelegate* alert_infobar =
        infobar->AsAlertInfoBarDelegate();
      infobar_item->SetString("text", alert_infobar->GetMessageText());
    } else if (infobar->AsLinkInfoBarDelegate()) {
      infobar_item->SetString("type", "link_infobar");
      LinkInfoBarDelegate* link_infobar = infobar->AsLinkInfoBarDelegate();
      infobar_item->SetString("link_text", link_infobar->GetLinkText());
    } else if (infobar->AsTranslateInfoBarDelegate()) {
      infobar_item->SetString("type", "translate_infobar");
      TranslateInfoBarDelegate* translate_infobar =
          infobar->AsTranslateInfoBarDelegate();
      infobar_item->SetString("original_lang_code",
                              translate_infobar->GetOriginalLanguageCode());
      infobar_item->SetString("target_lang_code",
                              translate_infobar->GetTargetLanguageCode());
    } else if (infobar->AsExtensionInfoBarDelegate()) {
      infobar_item->SetString("type", "extension_infobar");
    } else {
      infobar_item->SetString("type", "unknown_infobar");
    }
    infobars->Append(infobar_item);
  }
  return infobars;
}

// Sample json input: { "command": "PerformActionOnInfobar",
//                      "action": "dismiss",
//                      "infobar_index": 0,
//                      "tab_index": 0 }
// Sample output: {}
void AutomationProvider::PerformActionOnInfobar(Browser* browser,
                                                DictionaryValue* args,
                                                IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  int tab_index;
  int infobar_index;
  std::string action;
  if (!args->GetInteger("tab_index", &tab_index) ||
      !args->GetInteger("infobar_index", &infobar_index) ||
      !args->GetString("action", &action)) {
    reply.SendError("Invalid or missing args");
    return;
  }
  TabContents* tab_contents = browser->GetTabContentsAt(tab_index);
  if (!tab_contents) {
    reply.SendError(StringPrintf("No such tab at index %d", tab_index));
    return;
  }
  InfoBarDelegate* infobar = NULL;
  if (infobar_index < 0 ||
      infobar_index >= tab_contents->infobar_delegate_count() ||
      !(infobar = tab_contents->GetInfoBarDelegateAt(infobar_index))) {
    reply.SendError(StringPrintf("No such infobar at index %d", infobar_index));
    return;
  }
  if ("dismiss" == action) {
    infobar->InfoBarDismissed();
    tab_contents->RemoveInfoBar(infobar);
    reply.SendSuccess(NULL);
    return;
  }
  if ("accept" == action || "cancel" == action) {
    ConfirmInfoBarDelegate* confirm_infobar;
    if (!(confirm_infobar = infobar->AsConfirmInfoBarDelegate())) {
      reply.SendError("Not a confirm infobar");
      return;
    }
    if ("accept" == action) {
      if (confirm_infobar->Accept())
        tab_contents->RemoveInfoBar(infobar);
    } else if ("cancel" == action) {
      if (confirm_infobar->Cancel())
        tab_contents->RemoveInfoBar(infobar);
    }
    reply.SendSuccess(NULL);
    return;
  }
  reply.SendError("Invalid action");
}

namespace {

// Task to get info about BrowserChildProcessHost. Must run on IO thread to
// honor the semantics of BrowserChildProcessHost.
// Used by AutomationProvider::GetBrowserInfo().
class GetChildProcessHostInfoTask : public Task {
 public:
  GetChildProcessHostInfoTask(base::WaitableEvent* event,
                              ListValue* child_processes)
    : event_(event),
      child_processes_(child_processes) {}

  virtual void Run() {
    DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
    for (BrowserChildProcessHost::Iterator iter; !iter.Done(); ++iter) {
      // Only add processes which are already started,
      // since we need their handle.
      if ((*iter)->handle() == base::kNullProcessHandle) {
        continue;
      }
      ChildProcessInfo* info = *iter;
      DictionaryValue* item = new DictionaryValue;
      item->SetString("name", WideToUTF16Hack(info->name()));
      item->SetString("type",
                      ChildProcessInfo::GetTypeNameInEnglish(info->type()));
      item->SetInteger("pid", base::GetProcId(info->handle()));
      child_processes_->Append(item);
    }
    event_->Signal();
  }

 private:
  base::WaitableEvent* const event_;  // weak
  ListValue* child_processes_;

  DISALLOW_COPY_AND_ASSIGN(GetChildProcessHostInfoTask);
};

}  // namespace

// Sample json input: { "command": "GetBrowserInfo" }
// Refer to GetBrowserInfo() in chrome/test/pyautolib/pyauto.py for
// sample json output.
void AutomationProvider::GetBrowserInfo(Browser* browser,
                                        DictionaryValue* args,
                                        IPC::Message* reply_message) {
  DictionaryValue* properties = new DictionaryValue;
  properties->SetString("ChromeVersion", chrome::kChromeVersion);
  properties->SetString("BrowserProcessExecutableName",
                        WideToUTF16Hack(chrome::kBrowserProcessExecutableName));
  properties->SetString("HelperProcessExecutableName",
                        WideToUTF16Hack(chrome::kHelperProcessExecutableName));
  properties->SetString("BrowserProcessExecutablePath",
                        WideToUTF16Hack(chrome::kBrowserProcessExecutablePath));
  properties->SetString("HelperProcessExecutablePath",
                        chrome::kHelperProcessExecutablePath);
  properties->SetString("command_line_string",
      CommandLine::ForCurrentProcess()->command_line_string());
  FilePath dumps_path;
  PathService::Get(chrome::DIR_CRASH_DUMPS, &dumps_path);
  properties->SetString("DIR_CRASH_DUMPS", dumps_path.value());

  std::string branding;
#if defined(GOOGLE_CHROME_BUILD)
  branding = "Google Chrome";
#elif defined(CHROMIUM_BUILD)
  branding = "Chromium";
#else
  branding = "Unknown Branding";
#endif
  properties->SetString("branding", branding);

  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  return_value->Set("properties", properties);

  return_value->SetInteger("browser_pid", base::GetCurrentProcId());
  // Add info about all windows in a list of dictionaries, one dictionary
  // item per window.
  ListValue* windows = new ListValue;
  int windex = 0;
  for (BrowserList::const_iterator it = BrowserList::begin();
       it != BrowserList::end();
       ++it, ++windex) {
    DictionaryValue* browser_item = new DictionaryValue;
    browser = *it;
    browser_item->SetInteger("index", windex);
    // Window properties
    gfx::Rect rect = browser->window()->GetRestoredBounds();
    browser_item->SetInteger("x", rect.x());
    browser_item->SetInteger("y", rect.y());
    browser_item->SetInteger("width", rect.width());
    browser_item->SetInteger("height", rect.height());
    browser_item->SetBoolean("fullscreen",
                             browser->window()->IsFullscreen());
    browser_item->SetInteger("selected_tab", browser->selected_index());
    browser_item->SetBoolean("incognito",
                             browser->profile()->IsOffTheRecord());
    // For each window, add info about all tabs in a list of dictionaries,
    // one dictionary item per tab.
    ListValue* tabs = new ListValue;
    for (int i = 0; i < browser->tab_count(); ++i) {
      TabContents* tc = browser->GetTabContentsAt(i);
      DictionaryValue* tab = new DictionaryValue;
      tab->SetInteger("index", i);
      tab->SetString("url", tc->GetURL().spec());
      tab->SetInteger("renderer_pid",
                      base::GetProcId(tc->GetRenderProcessHost()->GetHandle()));
      tab->Set("infobars", GetInfobarsInfo(tc));
      tabs->Append(tab);
    }
    browser_item->Set("tabs", tabs);

    windows->Append(browser_item);
  }
  return_value->Set("windows", windows);

  return_value->SetString("child_process_path",
                          ChildProcessHost::GetChildPath(true).value());
  // Child processes are the processes for plugins and other workers.
  // Add all child processes in a list of dictionaries, one dictionary item
  // per child process.
  ListValue* child_processes = new ListValue;
  base::WaitableEvent event(true   /* manual reset */,
                            false  /* not initially signaled */);
  CHECK(ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      new GetChildProcessHostInfoTask(&event, child_processes)));
  event.Wait();
  return_value->Set("child_processes", child_processes);

  // Add all extension processes in a list of dictionaries, one dictionary
  // item per extension process.
  ListValue* extension_processes = new ListValue;
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  for (ProfileManager::const_iterator it = profile_manager->begin();
       it != profile_manager->end(); ++it) {
    ExtensionProcessManager* process_manager =
        (*it)->GetExtensionProcessManager();
    ExtensionProcessManager::const_iterator jt;
    for (jt = process_manager->begin(); jt != process_manager->end(); ++jt) {
      ExtensionHost* ex_host = *jt;
      // Don't add dead extension processes.
      if (!ex_host->IsRenderViewLive())
        continue;
      DictionaryValue* item = new DictionaryValue;
      item->SetString("name", ex_host->extension()->name());
      item->SetInteger(
          "pid",
          base::GetProcId(ex_host->render_process_host()->GetHandle()));
      extension_processes->Append(item);
    }
  }
  return_value->Set("extension_processes", extension_processes);
  AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
}

// Sample json input: { "command": "GetNavigationInfo" }
// Refer to GetNavigationInfo() in chrome/test/pyautolib/pyauto.py for
// sample json output.
void AutomationProvider::GetNavigationInfo(Browser* browser,
                                           DictionaryValue* args,
                                           IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  int tab_index;
  TabContents* tab_contents = NULL;
  if (!args->GetInteger("tab_index", &tab_index) ||
      !(tab_contents = browser->GetTabContentsAt(tab_index))) {
    reply.SendError("tab_index missing or invalid.");
    return;
  }
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  const NavigationController& controller = tab_contents->controller();
  NavigationEntry* nav_entry = controller.GetActiveEntry();
  DCHECK(nav_entry);

  // Security info.
  DictionaryValue* ssl = new DictionaryValue;
  std::map<SecurityStyle, std::string> style_to_string;
  style_to_string[SECURITY_STYLE_UNKNOWN] = "SECURITY_STYLE_UNKNOWN";
  style_to_string[SECURITY_STYLE_UNAUTHENTICATED] =
      "SECURITY_STYLE_UNAUTHENTICATED";
  style_to_string[SECURITY_STYLE_AUTHENTICATION_BROKEN] =
      "SECURITY_STYLE_AUTHENTICATION_BROKEN";
  style_to_string[SECURITY_STYLE_AUTHENTICATED] =
      "SECURITY_STYLE_AUTHENTICATED";

  NavigationEntry::SSLStatus ssl_status = nav_entry->ssl();
  ssl->SetString("security_style",
                 style_to_string[ssl_status.security_style()]);
  ssl->SetBoolean("ran_insecure_content", ssl_status.ran_insecure_content());
  ssl->SetBoolean("displayed_insecure_content",
                  ssl_status.displayed_insecure_content());
  return_value->Set("ssl", ssl);

  // Page type.
  std::map<NavigationEntry::PageType, std::string> pagetype_to_string;
  pagetype_to_string[NavigationEntry::NORMAL_PAGE] = "NORMAL_PAGE";
  pagetype_to_string[NavigationEntry::ERROR_PAGE] = "ERROR_PAGE";
  pagetype_to_string[NavigationEntry::INTERSTITIAL_PAGE] = "INTERSTITIAL_PAGE";
  return_value->SetString("page_type",
                          pagetype_to_string[nav_entry->page_type()]);

  return_value->SetString("favicon_url", nav_entry->favicon().url().spec());
  reply.SendSuccess(return_value.get());
}

// Sample json input: { "command": "GetHistoryInfo",
//                      "search_text": "some text" }
// Refer chrome/test/pyautolib/history_info.py for sample json output.
void AutomationProvider::GetHistoryInfo(Browser* browser,
                                        DictionaryValue* args,
                                        IPC::Message* reply_message) {
  consumer_.CancelAllRequests();

  string16 search_text;
  args->GetString("search_text", &search_text);

  // Fetch history.
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  history::QueryOptions options;
  // The observer owns itself.  It deletes itself after it fetches history.
  AutomationProviderHistoryObserver* history_observer =
      new AutomationProviderHistoryObserver(this, reply_message);
  hs->QueryHistory(
      search_text,
      options,
      &consumer_,
      NewCallback(history_observer,
                  &AutomationProviderHistoryObserver::HistoryQueryComplete));
}

// Sample json input: { "command": "AddHistoryItem",
//                      "item": { "URL": "http://www.google.com",
//                                "title": "Google",   # optional
//                                "time": 12345        # optional (time_t)
//                               } }
// Refer chrome/test/pyautolib/pyauto.py for details on input.
void AutomationProvider::AddHistoryItem(Browser* browser,
                                        DictionaryValue* args,
                                        IPC::Message* reply_message) {
  DictionaryValue* item = NULL;
  args->GetDictionary("item", &item);
  string16 url_text;
  string16 title;
  base::Time time = base::Time::Now();
  AutomationJSONReply reply(this, reply_message);

  if (!item->GetString("url", &url_text)) {
    reply.SendError("bad args (no URL in dict?)");
    return;
  }
  GURL gurl(url_text);
  item->GetString("title", &title);  // Don't care if it fails.
  int it;
  double dt;
  if (item->GetInteger("time", &it))
    time = base::Time::FromTimeT(it);
  else if (item->GetReal("time", &dt))
    time = base::Time::FromDoubleT(dt);

  // Ideas for "dummy" values (e.g. id_scope) came from
  // chrome/browser/autocomplete/history_contents_provider_unittest.cc
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  const void* id_scope = reinterpret_cast<void*>(1);
  hs->AddPage(gurl, time,
              id_scope,
              0,
              GURL(),
              PageTransition::LINK,
              history::RedirectList(),
              history::SOURCE_BROWSED,
              false);
  if (title.length())
    hs->SetPageTitle(gurl, title);
  reply.SendSuccess(NULL);
}

DictionaryValue* AutomationProvider::GetDictionaryFromDownloadItem(
    const DownloadItem* download) {
  std::map<DownloadItem::DownloadState, std::string> state_to_string;
  state_to_string[DownloadItem::IN_PROGRESS] = std::string("IN_PROGRESS");
  state_to_string[DownloadItem::CANCELLED] = std::string("CANCELLED");
  state_to_string[DownloadItem::REMOVING] = std::string("REMOVING");
  state_to_string[DownloadItem::COMPLETE] = std::string("COMPLETE");

  std::map<DownloadItem::SafetyState, std::string> safety_state_to_string;
  safety_state_to_string[DownloadItem::SAFE] = std::string("SAFE");
  safety_state_to_string[DownloadItem::DANGEROUS] = std::string("DANGEROUS");
  safety_state_to_string[DownloadItem::DANGEROUS_BUT_VALIDATED] =
      std::string("DANGEROUS_BUT_VALIDATED");

  DictionaryValue* dl_item_value = new DictionaryValue;
  dl_item_value->SetInteger("id", static_cast<int>(download->id()));
  dl_item_value->SetString("url", download->url().spec());
  dl_item_value->SetString("referrer_url", download->referrer_url().spec());
  dl_item_value->SetString("file_name", download->GetFileName().value());
  dl_item_value->SetString("full_path", download->full_path().value());
  dl_item_value->SetBoolean("is_paused", download->is_paused());
  dl_item_value->SetBoolean("open_when_complete",
                            download->open_when_complete());
  dl_item_value->SetBoolean("is_extension_install",
                            download->is_extension_install());
  dl_item_value->SetBoolean("is_temporary", download->is_temporary());
  dl_item_value->SetBoolean("is_otr", download->is_otr());  // off-the-record
  dl_item_value->SetString("state", state_to_string[download->state()]);
  dl_item_value->SetString("safety_state",
                           safety_state_to_string[download->safety_state()]);
  dl_item_value->SetInteger("PercentComplete", download->PercentComplete());

  return dl_item_value;
}

// Sample json input: { "command": "GetDownloadsInfo" }
// Refer chrome/test/pyautolib/download_info.py for sample json output.
void AutomationProvider::GetDownloadsInfo(Browser* browser,
                                          DictionaryValue* args,
                                          IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);

  if (!profile_->HasCreatedDownloadManager()) {
      reply.SendError("no download manager");
      return;
  }

  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  std::vector<DownloadItem*> downloads;
  profile_->GetDownloadManager()->GetAllDownloads(FilePath(), &downloads);

  ListValue* list_of_downloads = new ListValue;
  for (std::vector<DownloadItem*>::iterator it = downloads.begin();
       it != downloads.end();
       it++) {  // Fill info about each download item.
    list_of_downloads->Append(GetDictionaryFromDownloadItem(*it));
  }
  return_value->Set("downloads", list_of_downloads);
  reply.SendSuccess(return_value.get());
  // All value objects allocated above are owned by |return_value|
  // and get freed by it.
}

void AutomationProvider::WaitForDownloadsToComplete(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);

  // Look for a quick return.
  if (!profile_->HasCreatedDownloadManager()) {
    reply.SendSuccess(NULL);  // No download manager.
    return;
  }
  std::vector<DownloadItem*> downloads;
  profile_->GetDownloadManager()->GetCurrentDownloads(FilePath(), &downloads);
  if (downloads.empty()) {
    reply.SendSuccess(NULL);
    return;
  }

  // The observer owns itself.  When the last observed item pings, it
  // deletes itself.
  AutomationProviderDownloadItemObserver* item_observer =
      new AutomationProviderDownloadItemObserver(
          this, reply_message, downloads.size());
  for (std::vector<DownloadItem*>::iterator i = downloads.begin();
       i != downloads.end();
       i++) {
    (*i)->AddObserver(item_observer);
  }
}

namespace {

DownloadItem* GetDownloadItemFromId(int id, DownloadManager* download_manager) {
  std::vector<DownloadItem*> downloads;
  download_manager->GetAllDownloads(FilePath(), &downloads);
  DownloadItem* selected_item = NULL;

  for (std::vector<DownloadItem*>::iterator it = downloads.begin();
       it != downloads.end();
       it++) {
    DownloadItem* curr_item = *it;
    if (curr_item->id() == id) {
      selected_item = curr_item;
      break;
    }
  }
  return selected_item;
}

} // namespace

// See PerformActionOnDownload() in chrome/test/pyautolib/pyauto.py for sample
// json input and output.
void AutomationProvider::PerformActionOnDownload(Browser* browser,
                                                 DictionaryValue* args,
                                                 IPC::Message* reply_message) {
  int id;
  std::string action;

  if (!profile_->HasCreatedDownloadManager()) {
    AutomationJSONReply(this, reply_message).SendError("No download manager.");
    return;
  }
  if (!args->GetInteger("id", &id) || !args->GetString("action", &action)) {
    AutomationJSONReply(this, reply_message).SendError(
        "Must include int id and string action.");
    return;
  }

  DownloadManager* download_manager = profile_->GetDownloadManager();
  DownloadItem* selected_item = GetDownloadItemFromId(id, download_manager);
  if (!selected_item) {
    AutomationJSONReply(this, reply_message).SendError(
        StringPrintf("No download with an id of %d\n", id));
    return;
  }

  if (action == "open") {
    selected_item->AddObserver(
        new AutomationProviderDownloadUpdatedObserver(
            this, reply_message, true));
    selected_item->OpenDownload();
  } else if (action == "toggle_open_files_like_this") {
    selected_item->OpenFilesBasedOnExtension(
        !selected_item->ShouldOpenFileBasedOnExtension());
    AutomationJSONReply(this, reply_message).SendSuccess(NULL);
  } else if (action == "remove") {
    download_manager->AddObserver(
        new AutomationProviderDownloadModelChangedObserver(
            this, reply_message, download_manager));
    selected_item->Remove(false);
  } else if (action == "decline_dangerous_download") {
    // This is the same as removing the file with delete_file=true.
    download_manager->AddObserver(
        new AutomationProviderDownloadModelChangedObserver(
            this, reply_message, download_manager));
    selected_item->Remove(true);
  } else if (action == "save_dangerous_download") {
    selected_item->AddObserver(new AutomationProviderDownloadUpdatedObserver(
        this, reply_message, false));
    selected_item->DangerousDownloadValidated();
  } else if (action == "toggle_pause") {
    selected_item->AddObserver(new AutomationProviderDownloadUpdatedObserver(
        this, reply_message, false));
    // This will still return if download has already completed.
    selected_item->TogglePause();
  } else if (action == "cancel") {
    selected_item->AddObserver(new AutomationProviderDownloadUpdatedObserver(
        this, reply_message, false));
    selected_item->Cancel(true);
  } else {
    AutomationJSONReply(this, reply_message).SendError(
        StringPrintf("Invalid action '%s' given.", action.c_str()));
  }
}

// Sample json input: { "command": "GetPrefsInfo" }
// Refer chrome/test/pyautolib/prefs_info.py for sample json output.
void AutomationProvider::GetPrefsInfo(Browser* browser,
                                      DictionaryValue* args,
                                      IPC::Message* reply_message) {
  const PrefService::PreferenceSet& prefs =
      profile_->GetPrefs()->preference_set();
  DictionaryValue* items = new DictionaryValue;
  for (PrefService::PreferenceSet::const_iterator it = prefs.begin();
       it != prefs.end(); ++it) {
    items->Set((*it)->name(), (*it)->GetValue()->DeepCopy());
  }
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  return_value->Set("prefs", items);  // return_value owns items.
  AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
}

// Sample json input: { "command": "SetPrefs", "path": path, "value": value }
void AutomationProvider::SetPrefs(Browser* browser,
                                  DictionaryValue* args,
                                  IPC::Message* reply_message) {
  std::string path;
  Value* val;
  AutomationJSONReply reply(this, reply_message);
  if (args->GetString("path", &path) && args->Get("value", &val)) {
    PrefService* pref_service = profile_->GetPrefs();
    const PrefService::Preference* pref =
        pref_service->FindPreference(path.c_str());
    if (!pref) {  // Not a registered pref.
      reply.SendError("pref not registered.");
      return;
    } else if (pref->IsManaged()) {  // Do not attempt to change a managed pref.
      reply.SendError("pref is managed. cannot be changed.");
      return;
    } else {  // Set the pref.
      pref_service->Set(path.c_str(), *val);
    }
  } else {
    reply.SendError("no pref path or value given.");
    return;
  }

  reply.SendSuccess(NULL);
}

// Sample json input: { "command": "GetOmniboxInfo" }
// Refer chrome/test/pyautolib/omnibox_info.py for sample json output.
void AutomationProvider::GetOmniboxInfo(Browser* browser,
                                        DictionaryValue* args,
                                        IPC::Message* reply_message) {
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);

  LocationBar* loc_bar = browser->window()->GetLocationBar();
  AutocompleteEditView* edit_view = loc_bar->location_entry();
  AutocompleteEditModel* model = edit_view->model();

  // Fill up matches.
  ListValue* matches = new ListValue;
  const AutocompleteResult& result = model->result();
  for (AutocompleteResult::const_iterator i = result.begin();
       i != result.end(); ++i) {
    const AutocompleteMatch& match = *i;
    DictionaryValue* item = new DictionaryValue;  // owned by return_value
    item->SetString("type", AutocompleteMatch::TypeToString(match.type));
    item->SetBoolean("starred", match.starred);
    item->SetString("destination_url", match.destination_url.spec());
    item->SetString("contents", WideToUTF16Hack(match.contents));
    item->SetString("description", WideToUTF16Hack(match.description));
    matches->Append(item);
  }
  return_value->Set("matches", matches);

  // Fill up other properties.
  DictionaryValue* properties = new DictionaryValue;  // owned by return_value
  properties->SetBoolean("has_focus", model->has_focus());
  properties->SetBoolean("query_in_progress", model->query_in_progress());
  properties->SetString("keyword", WideToUTF16Hack(model->keyword()));
  properties->SetString("text", WideToUTF16Hack(edit_view->GetText()));
  return_value->Set("properties", properties);

  AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
}

// Sample json input: { "command": "SetOmniboxText",
//                      "text": "goog" }
void AutomationProvider::SetOmniboxText(Browser* browser,
                                        DictionaryValue* args,
                                        IPC::Message* reply_message) {
  string16 text;
  AutomationJSONReply reply(this, reply_message);
  if (!args->GetString("text", &text)) {
    reply.SendError("text missing");
    return;
  }
  browser->FocusLocationBar();
  LocationBar* loc_bar = browser->window()->GetLocationBar();
  AutocompleteEditView* edit_view = loc_bar->location_entry();
  edit_view->model()->OnSetFocus(false);
  edit_view->SetUserText(UTF16ToWideHack(text));
  reply.SendSuccess(NULL);
}

// Sample json input: { "command": "OmniboxMovePopupSelection",
//                      "count": 1 }
// Negative count implies up, positive implies down. Count values will be
// capped by the size of the popup list.
void AutomationProvider::OmniboxMovePopupSelection(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  int count;
  AutomationJSONReply reply(this, reply_message);
  if (!args->GetInteger("count", &count)) {
    reply.SendError("count missing");
    return;
  }
  LocationBar* loc_bar = browser->window()->GetLocationBar();
  AutocompleteEditModel* model = loc_bar->location_entry()->model();
  model->OnUpOrDownKeyPressed(count);
  reply.SendSuccess(NULL);
}

// Sample json input: { "command": "OmniboxAcceptInput" }
void AutomationProvider::OmniboxAcceptInput(Browser* browser,
                                            DictionaryValue* args,
                                            IPC::Message* reply_message) {
  NavigationController& controller =
      browser->GetSelectedTabContents()->controller();
  // Setup observer to wait until the selected item loads.
  NotificationObserver* observer =
      new OmniboxAcceptNotificationObserver(&controller, this, reply_message);
  notification_observer_list_.AddObserver(observer);

  browser->window()->GetLocationBar()->AcceptInput();
}

// Sample json input: { "command": "GetInitialLoadTimes" }
// Refer to InitialLoadObserver::GetTimingInformation() for sample output.
void AutomationProvider::GetInitialLoadTimes(
    Browser*,
    DictionaryValue*,
    IPC::Message* reply_message) {
  scoped_ptr<DictionaryValue> return_value(
      initial_load_observer_->GetTimingInformation());

  std::string json_return;
  base::JSONWriter::Write(return_value.get(), false, &json_return);
  AutomationMsg_SendJSONRequest::WriteReplyParams(
      reply_message, json_return, true);
  Send(reply_message);
}

// Sample json input: { "command": "GetPluginsInfo" }
// Refer chrome/test/pyautolib/plugins_info.py for sample json output.
void AutomationProvider::GetPluginsInfo(Browser* browser,
                                        DictionaryValue* args,
                                        IPC::Message* reply_message) {
  std::vector<WebPluginInfo> plugins;
  NPAPI::PluginList::Singleton()->GetPlugins(false, &plugins);
  ListValue* items = new ListValue;
  for (std::vector<WebPluginInfo>::const_iterator it = plugins.begin();
       it != plugins.end();
       ++it) {
    DictionaryValue* item = new DictionaryValue;
    item->SetString("name", it->name);
    item->SetString("path", it->path.value());
    item->SetString("version", it->version);
    item->SetString("desc", it->desc);
    item->SetBoolean("enabled", it->enabled);
    // Add info about mime types.
    ListValue* mime_types = new ListValue();
    for (std::vector<WebPluginMimeType>::const_iterator type_it =
             it->mime_types.begin();
         type_it != it->mime_types.end();
         ++type_it) {
      DictionaryValue* mime_type = new DictionaryValue();
      mime_type->SetString("mimeType", type_it->mime_type);
      mime_type->SetString("description", type_it->description);

      ListValue* file_extensions = new ListValue();
      for (std::vector<std::string>::const_iterator ext_it =
               type_it->file_extensions.begin();
           ext_it != type_it->file_extensions.end();
           ++ext_it) {
        file_extensions->Append(new StringValue(*ext_it));
      }
      mime_type->Set("fileExtensions", file_extensions);

      mime_types->Append(mime_type);
    }
    item->Set("mimeTypes", mime_types);
    items->Append(item);
  }
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  return_value->Set("plugins", items);  // return_value owns items.

  AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
}

// Sample json input:
//    { "command": "EnablePlugin",
//      "path": "/Library/Internet Plug-Ins/Flash Player.plugin" }
void AutomationProvider::EnablePlugin(Browser* browser,
                                      DictionaryValue* args,
                                      IPC::Message* reply_message) {
  FilePath::StringType path;
  AutomationJSONReply reply(this, reply_message);
  if (!args->GetString("path", &path)) {
    reply.SendError("path not specified.");
    return;
  } else if (!NPAPI::PluginList::Singleton()->EnablePlugin(FilePath(path))) {
    reply.SendError(StringPrintf("Could not enable plugin for path %s.",
                                 path.c_str()));
    return;
  }
  reply.SendSuccess(NULL);
}

// Sample json input:
//    { "command": "DisablePlugin",
//      "path": "/Library/Internet Plug-Ins/Flash Player.plugin" }
void AutomationProvider::DisablePlugin(Browser* browser,
                                       DictionaryValue* args,
                                       IPC::Message* reply_message) {
  FilePath::StringType path;
  AutomationJSONReply reply(this, reply_message);
  if (!args->GetString("path", &path)) {
    reply.SendError("path not specified.");
    return;
  } else if (!NPAPI::PluginList::Singleton()->DisablePlugin(FilePath(path))) {
    reply.SendError(StringPrintf("Could not disable plugin for path %s.",
                                 path.c_str()));
    return;
  }
  reply.SendSuccess(NULL);
}

// Sample json input:
//    { "command": "SaveTabContents",
//      "tab_index": 0,
//      "filename": <a full pathname> }
// Sample json output:
//    {}
void AutomationProvider::SaveTabContents(Browser* browser,
                                         DictionaryValue* args,
                                         IPC::Message* reply_message) {
  int tab_index = 0;
  FilePath::StringType filename;
  FilePath::StringType parent_directory;
  TabContents* tab_contents = NULL;

  if (!args->GetInteger("tab_index", &tab_index) ||
      !args->GetString("filename", &filename)) {
    AutomationJSONReply(this, reply_message).SendError(
        "tab_index or filename param missing");
    return;
  } else {
    tab_contents = browser->GetTabContentsAt(tab_index);
    if (!tab_contents) {
      AutomationJSONReply(this, reply_message).SendError(
          "no tab at tab_index");
      return;
    }
  }
  // We're doing a SAVE_AS_ONLY_HTML so the the directory path isn't
  // used.  Nevertheless, SavePackage requires it be valid.  Sigh.
  parent_directory = FilePath(filename).DirName().value();
  if (!tab_contents->SavePage(FilePath(filename), FilePath(parent_directory),
                              SavePackage::SAVE_AS_ONLY_HTML)) {
    AutomationJSONReply(this, reply_message).SendError(
        "Could not initiate SavePage");
    return;
  }
  // The observer will delete itself when done.
  new SavePackageNotificationObserver(tab_contents->save_package(),
                                      this, reply_message);
}

// Refer to ImportSettings() in chrome/test/pyautolib/pyauto.py for sample
// json input.
// Sample json output: "{}"
void AutomationProvider::ImportSettings(Browser* browser,
                                        DictionaryValue* args,
                                        IPC::Message* reply_message) {
  // Map from the json string passed over to the import item masks.
  std::map<std::string, ImportItem> string_to_import_item;
  string_to_import_item["HISTORY"] = importer::HISTORY;
  string_to_import_item["FAVORITES"] = importer::FAVORITES;
  string_to_import_item["COOKIES"] = importer::COOKIES;
  string_to_import_item["PASSWORDS"] = importer::PASSWORDS;
  string_to_import_item["SEARCH_ENGINES"] = importer::SEARCH_ENGINES;
  string_to_import_item["HOME_PAGE"] = importer::HOME_PAGE;
  string_to_import_item["ALL"] = importer::ALL;

  string16 browser_name;
  int import_items = 0;
  ListValue* import_items_list = NULL;
  bool first_run;

  if (!args->GetString("import_from", &browser_name) ||
      !args->GetBoolean("first_run", &first_run) ||
      !args->GetList("import_items", &import_items_list)) {
    AutomationJSONReply(this, reply_message).SendError(
        "Incorrect type for one or more of the arguments.");
    return;
  }

  int num_items = import_items_list->GetSize();
  for (int i = 0; i < num_items; i++) {
    std::string item;
    import_items_list->GetString(i, &item);
    // If the provided string is not part of the map, error out.
    if (!ContainsKey(string_to_import_item, item)) {
      AutomationJSONReply(this, reply_message).SendError(
          "Invalid item string found in import_items.");
      return;
    }
    import_items |= string_to_import_item[item];
  }

  ImporterHost* importer_host = new ImporterHost();
  // Get the correct ProfileInfo based on the browser they user provided.
  importer::ProfileInfo profile_info;
  int num_browsers = importer_host->GetAvailableProfileCount();
  int i = 0;
  for ( ; i < num_browsers; i++) {
    string16 name = WideToUTF16Hack(importer_host->GetSourceProfileNameAt(i));
    if (name == browser_name) {
      profile_info = importer_host->GetSourceProfileInfoAt(i);
      break;
    }
  }
  // If we made it to the end of the loop, then the input was bad.
  if (i == num_browsers) {
    AutomationJSONReply(this, reply_message).SendError(
        "Invalid browser name string found.");
    return;
  }

  Profile* profile = browser->profile();

  importer_host->SetObserver(
      new AutomationProviderImportSettingsObserver(this, reply_message));
  importer_host->StartImportSettings(profile_info, profile, import_items,
                                     new ProfileWriter(profile), first_run);
}

namespace {

// Translates a dictionary password to a PasswordForm struct.
webkit_glue::PasswordForm GetPasswordFormFromDict(
    const DictionaryValue& password_dict) {

  // If the time is specified, change time to the specified time.
  base::Time time = base::Time::Now();
  int it;
  double dt;
  if (password_dict.GetInteger("time", &it))
    time = base::Time::FromTimeT(it);
  else if (password_dict.GetReal("time", &dt))
    time = base::Time::FromDoubleT(dt);

  std::string signon_realm;
  string16 username_value;
  string16 password_value;
  string16 origin_url_text;
  string16 username_element;
  string16 password_element;
  string16 submit_element;
  string16 action_target_text;
  bool blacklist = false;
  string16 old_password_element;
  string16 old_password_value;

  // We don't care if any of these fail - they are either optional or checked
  // before this function is called.
  password_dict.GetString("signon_realm", &signon_realm);
  password_dict.GetString("username_value", &username_value);
  password_dict.GetString("password_value", &password_value);
  password_dict.GetString("origin_url", &origin_url_text);
  password_dict.GetString("username_element", &username_element);
  password_dict.GetString("password_element", &password_element);
  password_dict.GetString("submit_element", &submit_element);
  password_dict.GetString("action_target", &action_target_text);
  password_dict.GetBoolean("blacklist", &blacklist);

  GURL origin_gurl(origin_url_text);
  GURL action_target(action_target_text);

  webkit_glue::PasswordForm password_form;
  password_form.signon_realm = signon_realm;
  password_form.username_value = username_value;
  password_form.password_value = password_value;
  password_form.origin = origin_gurl;
  password_form.username_element = username_element;
  password_form.password_element = password_element;
  password_form.submit_element = submit_element;
  password_form.action = action_target;
  password_form.blacklisted_by_user = blacklist;
  password_form.date_created = time;

  return password_form;
}

} // namespace

// See AddSavedPassword() in chrome/test/functional/pyauto.py for sample json
// input.
// Sample json output: { "password_added": true }
void AutomationProvider::AddSavedPassword(Browser* browser,
                                          DictionaryValue* args,
                                          IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  DictionaryValue* password_dict = NULL;

  if (!args->GetDictionary("password", &password_dict)) {
    reply.SendError("Password must be a dictionary.");
    return;
  }

  // The signon realm is effectively the primary key and must be included.
  // Check here before calling GetPasswordFormFromDict.
  if (!password_dict->HasKey("signon_realm")) {
    reply.SendError("Password must include signon_realm.");
    return;
  }
  webkit_glue::PasswordForm new_password =
      GetPasswordFormFromDict(*password_dict);

  Profile* profile = browser->profile();
  // Use IMPLICIT_ACCESS since new passwords aren't added off the record.
  PasswordStore* password_store =
      profile->GetPasswordStore(Profile::IMPLICIT_ACCESS);

  // Set the return based on whether setting the password succeeded.
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);

  // It will be null if it's accessed in an incognito window.
  if (password_store != NULL) {
    password_store->AddLogin(new_password);
    return_value->SetBoolean("password_added", true);
  } else {
    return_value->SetBoolean("password_added", false);
  }

  reply.SendSuccess(return_value.get());
}

// See RemoveSavedPassword() in chrome/test/functional/pyauto.py for sample
// json input.
// Sample json output: {}
void AutomationProvider::RemoveSavedPassword(Browser* browser,
                                             DictionaryValue* args,
                                             IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  DictionaryValue* password_dict = NULL;

  if (!args->GetDictionary("password", &password_dict)) {
    reply.SendError("Password must be a dictionary.");
    return;
  }

  // The signon realm is effectively the primary key and must be included.
  // Check here before calling GetPasswordFormFromDict.
  if (!password_dict->HasKey("signon_realm")) {
    reply.SendError("Password must include signon_realm.");
    return;
  }
  webkit_glue::PasswordForm to_remove =
      GetPasswordFormFromDict(*password_dict);

  Profile* profile = browser->profile();
  // Use EXPLICIT_ACCESS since passwords can be removed off the record.
  PasswordStore* password_store =
      profile->GetPasswordStore(Profile::EXPLICIT_ACCESS);

  password_store->RemoveLogin(to_remove);
  reply.SendSuccess(NULL);
}

// Sample json input: { "command": "GetSavedPasswords" }
// Refer to GetSavedPasswords() in chrome/test/pyautolib/pyauto.py for sample
// json output.
void AutomationProvider::GetSavedPasswords(Browser* browser,
                                           DictionaryValue* args,
                                           IPC::Message* reply_message) {
  Profile* profile = browser->profile();
  // Use EXPLICIT_ACCESS since saved passwords can be retreived off the record.
  PasswordStore* password_store =
      profile->GetPasswordStore(Profile::EXPLICIT_ACCESS);
  password_store->GetAutofillableLogins(
      new AutomationProviderGetPasswordsObserver(this, reply_message));
  // Observer deletes itself after returning.
}

// Refer to ClearBrowsingData() in chrome/test/pyautolib/pyauto.py for sample
// json input.
// Sample json output: {}
void AutomationProvider::ClearBrowsingData(Browser* browser,
                                           DictionaryValue* args,
                                           IPC::Message* reply_message) {
  std::map<std::string, BrowsingDataRemover::TimePeriod> string_to_time_period;
  string_to_time_period["LAST_HOUR"] = BrowsingDataRemover::LAST_HOUR;
  string_to_time_period["LAST_DAY"] = BrowsingDataRemover::LAST_DAY;
  string_to_time_period["LAST_WEEK"] = BrowsingDataRemover::LAST_WEEK;
  string_to_time_period["FOUR_WEEKS"] = BrowsingDataRemover::FOUR_WEEKS;
  string_to_time_period["EVERYTHING"] = BrowsingDataRemover::EVERYTHING;

  std::map<std::string, int> string_to_mask_value;
  string_to_mask_value["HISTORY"] = BrowsingDataRemover::REMOVE_HISTORY;
  string_to_mask_value["DOWNLOADS"] = BrowsingDataRemover::REMOVE_DOWNLOADS;
  string_to_mask_value["COOKIES"] = BrowsingDataRemover::REMOVE_COOKIES;
  string_to_mask_value["PASSWORDS"] = BrowsingDataRemover::REMOVE_PASSWORDS;
  string_to_mask_value["FORM_DATA"] = BrowsingDataRemover::REMOVE_FORM_DATA;
  string_to_mask_value["CACHE"] = BrowsingDataRemover::REMOVE_CACHE;

  std::string time_period;
  ListValue* to_remove;
  if (!args->GetString("time_period", &time_period) ||
      !args->GetList("to_remove", &to_remove)) {
    AutomationJSONReply(this, reply_message).SendError(
        "time_period must be a string and to_remove a list.");
    return;
  }

  int remove_mask = 0;
  int num_removals = to_remove->GetSize();
  for (int i = 0; i < num_removals; i++) {
    std::string removal;
    to_remove->GetString(i, &removal);
    // If the provided string is not part of the map, then error out.
    if (!ContainsKey(string_to_mask_value, removal)) {
      AutomationJSONReply(this, reply_message).SendError(
          "Invalid browsing data string found in to_remove.");
      return;
    }
    remove_mask |= string_to_mask_value[removal];
  }

  if (!ContainsKey(string_to_time_period, time_period)) {
    AutomationJSONReply(this, reply_message).SendError(
        "Invalid string for time_period.");
    return;
  }

  BrowsingDataRemover* remover = new BrowsingDataRemover(
      profile(), string_to_time_period[time_period], base::Time());

  remover->AddObserver(
      new AutomationProviderBrowsingDataObserver(this, reply_message));
  remover->Remove(remove_mask);
  // BrowsingDataRemover deletes itself using DeleteTask.
  // The observer also deletes itself after sending the reply.
}

namespace {

  // Get the TabContents from a dictionary of arguments.
  TabContents* GetTabContentsFromDict(const Browser* browser,
                                      const DictionaryValue* args,
                                      std::string* error_message) {
    int tab_index;
    if (!args->GetInteger("tab_index", &tab_index)) {
      *error_message = "Must include tab_index.";
      return NULL;
    }

    TabContents* tab_contents = browser->GetTabContentsAt(tab_index);
    if (!tab_contents) {
      *error_message = StringPrintf("No tab at index %d.", tab_index);
      return NULL;
    }
    return tab_contents;
  }

  // Get the TranslateInfoBarDelegate from TabContents.
  TranslateInfoBarDelegate* GetTranslateInfoBarDelegate(
      TabContents* tab_contents) {
    for (int i = 0; i < tab_contents->infobar_delegate_count(); i++) {
      InfoBarDelegate* infobar = tab_contents->GetInfoBarDelegateAt(i);
      if (infobar->AsTranslateInfoBarDelegate())
        return infobar->AsTranslateInfoBarDelegate();
    }
    // No translate infobar.
    return NULL;
  }

}  // namespace

// See GetTranslateInfo() in chrome/test/pyautolib/pyauto.py for sample json
// input and output.
void AutomationProvider::GetTranslateInfo(Browser* browser,
                                          DictionaryValue* args,
                                          IPC::Message* reply_message) {
  std::string error_message;
  TabContents* tab_contents = GetTabContentsFromDict(browser, args,
                                                     &error_message);
  if (!tab_contents) {
    AutomationJSONReply(this, reply_message).SendError(error_message);
    return;
  }

  // Get the translate bar if there is one and pass it to the observer.
  // The observer will check for null and populate the information accordingly.
  TranslateInfoBarDelegate* translate_bar =
      GetTranslateInfoBarDelegate(tab_contents);

  TabLanguageDeterminedObserver* observer = new TabLanguageDeterminedObserver(
      this, reply_message, tab_contents, translate_bar);
  // If the language for the page hasn't been loaded yet, then just make
  // the observer, otherwise call observe directly.
  std::string language = tab_contents->language_state().original_language();
  if (!language.empty()) {
    observer->Observe(NotificationType::TAB_LANGUAGE_DETERMINED,
                      Source<TabContents>(tab_contents),
                      Details<std::string>(&language));
  }
}

// See SelectTranslateOption() in chrome/test/pyautolib/pyauto.py for sample
// json input.
// Sample json output: {}
void AutomationProvider::SelectTranslateOption(Browser* browser,
                                               DictionaryValue* args,
                                               IPC::Message* reply_message) {
  std::string option;
  std::string error_message;
  TabContents* tab_contents = GetTabContentsFromDict(browser, args,
                                                     &error_message);
  if (!tab_contents) {
    AutomationJSONReply(this, reply_message).SendError(error_message);
    return;
  }

  TranslateInfoBarDelegate* translate_bar =
      GetTranslateInfoBarDelegate(tab_contents);
  if (!translate_bar) {
    AutomationJSONReply(this, reply_message)
        .SendError("There is no translate bar open.");
    return;
  }

  if (!args->GetString("option", &option)) {
    AutomationJSONReply(this, reply_message).SendError("Must include option");
    return;
  }

  if (option == "translate_page") {
    // Make a new notification observer which will send the reply.
    new PageTranslatedObserver(this, reply_message, tab_contents);
    translate_bar->Translate();
    return;
  } else if (option == "set_target_language") {
    string16 target_language;
    if (!args->GetString("target_language", &target_language)) {
       AutomationJSONReply(this, reply_message).
           SendError("Must include target_language string.");
      return;
    }
    // Get the target language index based off of the language name.
    int target_language_index = -1;
    for (int i = 0; i < translate_bar->GetLanguageCount(); i++) {
      if (translate_bar->GetLanguageDisplayableNameAt(i) == target_language) {
        target_language_index = i;
        break;
      }
    }
    if (target_language_index == -1) {
       AutomationJSONReply(this, reply_message)
           .SendError("Invalid target language string.");
       return;
    }
    // If the page has already been translated it will be translated again to
    // the new language. The observer will wait until the page has been
    // translated to reply.
    if (translate_bar->type() == TranslateInfoBarDelegate::AFTER_TRANSLATE) {
      new PageTranslatedObserver(this, reply_message, tab_contents);
      translate_bar->SetTargetLanguage(target_language_index);
      return;
    }
    // Otherwise just send the reply back immediately.
    translate_bar->SetTargetLanguage(target_language_index);
    scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
    return_value->SetBoolean("translation_success", true);
    AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
    return;
  } else if (option == "click_always_translate_lang_button") {
    if (!translate_bar->ShouldShowAlwaysTranslateButton()) {
      AutomationJSONReply(this, reply_message)
          .SendError("Always translate button not showing.");
      return;
    }
    // Clicking 'Always Translate' triggers a translation. The observer will
    // wait until the translation is complete before sending the reply.
    new PageTranslatedObserver(this, reply_message, tab_contents);
    translate_bar->AlwaysTranslatePageLanguage();
    return;
  }

  AutomationJSONReply reply(this, reply_message);
  if (option == "never_translate_language") {
    if (translate_bar->IsLanguageBlacklisted()) {
      reply.SendError("The language was already blacklisted.");
      return;
    }
    translate_bar->ToggleLanguageBlacklist();
    reply.SendSuccess(NULL);
  } else if (option == "never_translate_site") {
    if (translate_bar->IsSiteBlacklisted()) {
      reply.SendError("The site was already blacklisted.");
      return;
    }
    translate_bar->ToggleSiteBlacklist();
    reply.SendSuccess(NULL);
  } else if (option == "toggle_always_translate") {
    translate_bar->ToggleAlwaysTranslate();
    reply.SendSuccess(NULL);
  } else if (option == "revert_translation") {
    translate_bar->RevertTranslation();
    reply.SendSuccess(NULL);
  } else if (option == "click_never_translate_lang_button") {
    if (!translate_bar->ShouldShowNeverTranslateButton()) {
      reply.SendError("Always translate button not showing.");
      return;
    }
    translate_bar->NeverTranslatePageLanguage();
    reply.SendSuccess(NULL);
  } else if (option == "decline_translation") {
    // This is the function called when an infobar is dismissed or when the
    // user clicks the 'Nope' translate button.
    translate_bar->TranslationDeclined();
    tab_contents->RemoveInfoBar(translate_bar);
    reply.SendSuccess(NULL);
  } else {
    reply.SendError("Invalid string found for option.");
  }
}

// Sample json input: { "command": "GetThemeInfo" }
// Refer GetThemeInfo() in chrome/test/pyautolib/pyauto.py for sample output.
void AutomationProvider::GetThemeInfo(Browser* browser,
                                      DictionaryValue* args,
                                      IPC::Message* reply_message) {
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  Extension* theme = browser->profile()->GetTheme();
  if (theme) {
    return_value->SetString("name", theme->name());
    return_value->Set("images", theme->GetThemeImages()->DeepCopy());
    return_value->Set("colors", theme->GetThemeColors()->DeepCopy());
    return_value->Set("tints", theme->GetThemeTints()->DeepCopy());
  }
  AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
}

// Sample json input: { "command": "GetExtensionsInfo" }
// See GetExtensionsInfo() in chrome/test/pyautolib/pyauto.py for sample json
// output.
void AutomationProvider::GetExtensionsInfo(Browser* browser,
                                           DictionaryValue* args,
                                           IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  ExtensionsService* service = profile()->GetExtensionsService();
  if (!service) {
    reply.SendError("No extensions service.");
  }
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  ListValue* extensions_values = new ListValue;
  const ExtensionList* extensions = service->extensions();
  for (ExtensionList::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    const Extension* extension = *it;
    DictionaryValue* extension_value = new DictionaryValue;
    extension_value->SetString("id", extension->id());
    extension_value->SetString("version", extension->VersionString());
    extension_value->SetString("name", extension->name());
    extension_value->SetString("public_key", extension->public_key());
    extension_value->SetString("description", extension->description());
    extension_value->SetString("background_url",
                               extension->background_url().spec());
    extension_value->SetString("options_url",
                               extension->options_url().spec());
    extensions_values->Append(extension_value);
  }
  return_value->Set("extensions", extensions_values);
  reply.SendSuccess(return_value.get());
}

// See UninstallExtensionById() in chrome/test/pyautolib/pyauto.py for sample
// json input.
// Sample json output: {}
void AutomationProvider::UninstallExtensionById(Browser* browser,
                                                DictionaryValue* args,
                                                IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  std::string id;
  if (!args->GetString("id", &id)) {
    reply.SendError("Must include string id.");
    return;
  }
  ExtensionsService* service = profile()->GetExtensionsService();
  if (!service) {
    reply.SendError("No extensions service.");
    return;
  }
  ExtensionUnloadNotificationObserver observer;
  service->UninstallExtension(id, false);
  reply.SendSuccess(NULL);
}

// Sample json input:
//    { "command": "GetAutoFillProfile" }
// Refer to GetAutoFillProfile() in chrome/test/pyautolib/pyauto.py for sample
// json output.
void AutomationProvider::GetAutoFillProfile(Browser* browser,
                                            DictionaryValue* args,
                                            IPC::Message* reply_message) {
  // Get the AutoFillProfiles currently in the database.
  int tab_index = 0;
  args->GetInteger("tab_index", &tab_index);
  TabContents* tab_contents = browser->GetTabContentsAt(tab_index);
  AutomationJSONReply reply(this, reply_message);

  if (tab_contents) {
    PersonalDataManager* pdm = tab_contents->profile()->GetOriginalProfile()
        ->GetPersonalDataManager();
    if (pdm) {
      std::vector<AutoFillProfile*> autofill_profiles = pdm->profiles();
      std::vector<CreditCard*> credit_cards = pdm->credit_cards();

      ListValue* profiles = GetListFromAutoFillProfiles(autofill_profiles);
      ListValue* cards = GetListFromCreditCards(credit_cards);

      scoped_ptr<DictionaryValue> return_value(new DictionaryValue);

      return_value->Set("profiles", profiles);
      return_value->Set("credit_cards", cards);
      reply.SendSuccess(return_value.get());
    } else {
      reply.SendError("No PersonalDataManager.");
      return;
    }
  } else {
    reply.SendError("No tab at that index.");
    return;
  }
}

// Refer to FillAutoFillProfile() in chrome/test/pyautolib/pyauto.py for sample
// json input.
// Sample json output: {}
void AutomationProvider::FillAutoFillProfile(Browser* browser,
                                             DictionaryValue* args,
                                             IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  ListValue* profiles = NULL;
  ListValue* cards = NULL;
  args->GetList("profiles", &profiles);
  args->GetList("credit_cards", &cards);
  std::string error_mesg;

  std::vector<AutoFillProfile> autofill_profiles;
  std::vector<CreditCard> credit_cards;
  // Create an AutoFillProfile for each of the dictionary profiles.
  if (profiles) {
    autofill_profiles = GetAutoFillProfilesFromList(*profiles, &error_mesg);
  }
  // Create a CreditCard for each of the dictionary values.
  if (cards) {
    credit_cards = GetCreditCardsFromList(*cards, &error_mesg);
  }
  if (!error_mesg.empty()) {
    reply.SendError(error_mesg);
    return;
  }

  // Save the AutoFillProfiles.
  int tab_index = 0;
  args->GetInteger("tab_index", &tab_index);
  TabContents* tab_contents = browser->GetTabContentsAt(tab_index);

  if (tab_contents) {
    PersonalDataManager* pdm = tab_contents->profile()
        ->GetPersonalDataManager();
    if (pdm) {
      pdm->OnAutoFillDialogApply(profiles? &autofill_profiles : NULL,
                                 cards? &credit_cards : NULL);
    } else {
      reply.SendError("No PersonalDataManager.");
      return;
    }
  } else {
    reply.SendError("No tab at that index.");
    return;
  }
  reply.SendSuccess(NULL);
}

/* static */
ListValue* AutomationProvider::GetListFromAutoFillProfiles(
    std::vector<AutoFillProfile*> autofill_profiles) {
  ListValue* profiles = new ListValue;

  std::map<AutoFillFieldType, std::wstring> autofill_type_to_string
      = GetAutoFillFieldToStringMap();

  // For each AutoFillProfile, transform it to a dictionary object to return.
  for (std::vector<AutoFillProfile*>::iterator it = autofill_profiles.begin();
       it != autofill_profiles.end(); ++it) {
    AutoFillProfile* profile = *it;
    DictionaryValue* profile_info = new DictionaryValue;
    profile_info->SetString("label", profile->Label());
    // For each of the types, if it has a value, add it to the dictionary.
    for (std::map<AutoFillFieldType, std::wstring>::iterator
         type_it = autofill_type_to_string.begin();
         type_it != autofill_type_to_string.end(); ++type_it) {
      string16 value = profile->GetFieldText(AutoFillType(type_it->first));
      if (value.length()) {  // If there was something stored for that value.
        profile_info->SetString(WideToUTF8(type_it->second), value);
      }
    }
    profiles->Append(profile_info);
  }
  return profiles;
}

/* static */
ListValue* AutomationProvider::GetListFromCreditCards(
    std::vector<CreditCard*> credit_cards) {
  ListValue* cards = new ListValue;

  std::map<AutoFillFieldType, std::wstring> credit_card_type_to_string =
      GetCreditCardFieldToStringMap();

  // For each AutoFillProfile, transform it to a dictionary object to return.
  for (std::vector<CreditCard*>::iterator it = credit_cards.begin();
       it != credit_cards.end(); ++it) {
    CreditCard* card = *it;
    DictionaryValue* card_info = new DictionaryValue;
    card_info->SetString("label", card->Label());
    // For each of the types, if it has a value, add it to the dictionary.
    for (std::map<AutoFillFieldType, std::wstring>::iterator type_it =
        credit_card_type_to_string.begin();
        type_it != credit_card_type_to_string.end(); ++type_it) {
      string16 value = card->GetFieldText(AutoFillType(type_it->first));
      // If there was something stored for that value.
      if (value.length()) {
        card_info->SetString(WideToUTF8(type_it->second), value);
      }
    }
    cards->Append(card_info);
  }
  return cards;
}

/* static */
std::vector<AutoFillProfile> AutomationProvider::GetAutoFillProfilesFromList(
    const ListValue& profiles, std::string* error_message) {
  std::vector<AutoFillProfile> autofill_profiles;
  DictionaryValue* profile_info = NULL;
  string16 profile_label;
  string16 current_value;

  std::map<AutoFillFieldType, std::wstring> autofill_type_to_string =
      GetAutoFillFieldToStringMap();

  int num_profiles = profiles.GetSize();
  for (int i = 0; i < num_profiles; i++) {
    profiles.GetDictionary(i, &profile_info);
    profile_info->GetString("label", &profile_label);
    // Choose an id of 0 so that a unique id will be created.
    AutoFillProfile profile(profile_label, 0);
    // Loop through the possible profile types and add those provided.
    for (std::map<AutoFillFieldType, std::wstring>::iterator type_it =
         autofill_type_to_string.begin();
         type_it != autofill_type_to_string.end(); ++type_it) {
      if (profile_info->HasKey(WideToUTF8(type_it->second))) {
        if (profile_info->GetString(WideToUTF8(type_it->second),
                                    &current_value)) {
          profile.SetInfo(AutoFillType(type_it->first), current_value);
        } else {
          *error_message= "All values must be strings";
          break;
        }
      }
    }
    autofill_profiles.push_back(profile);
  }
  return autofill_profiles;
}

/* static */
std::vector<CreditCard> AutomationProvider::GetCreditCardsFromList(
    const ListValue& cards, std::string* error_message) {
  std::vector<CreditCard> credit_cards;
  DictionaryValue* card_info = NULL;
  string16 card_label;
  string16 current_value;

  std::map<AutoFillFieldType, std::wstring> credit_card_type_to_string =
      GetCreditCardFieldToStringMap();

  int num_credit_cards = cards.GetSize();
  for (int i = 0; i < num_credit_cards; i++) {
    cards.GetDictionary(i, &card_info);
    card_info->GetString("label", &card_label);
    CreditCard card(card_label, 0);
    // Loop through the possible credit card fields and add those provided.
    for (std::map<AutoFillFieldType, std::wstring>::iterator type_it =
        credit_card_type_to_string.begin();
        type_it != credit_card_type_to_string.end(); ++type_it) {
      if (card_info->HasKey(WideToUTF8(type_it->second))) {
        if (card_info->GetString(WideToUTF8(type_it->second), &current_value)) {
          card.SetInfo(AutoFillType(type_it->first), current_value);
        } else {
          *error_message= "All values must be strings";
          break;
        }
      }
    }
    credit_cards.push_back(card);
  }
  return credit_cards;
}

/* static */
std::map<AutoFillFieldType, std::wstring>
    AutomationProvider::GetAutoFillFieldToStringMap() {
  std::map<AutoFillFieldType, std::wstring> autofill_type_to_string;
  autofill_type_to_string[NAME_FIRST] = L"NAME_FIRST";
  autofill_type_to_string[NAME_MIDDLE] = L"NAME_MIDDLE";
  autofill_type_to_string[NAME_LAST] = L"NAME_LAST";
  autofill_type_to_string[COMPANY_NAME] = L"COMPANY_NAME";
  autofill_type_to_string[EMAIL_ADDRESS] = L"EMAIL_ADDRESS";
  autofill_type_to_string[ADDRESS_HOME_LINE1] = L"ADDRESS_HOME_LINE1";
  autofill_type_to_string[ADDRESS_HOME_LINE2] = L"ADDRESS_HOME_LINE2";
  autofill_type_to_string[ADDRESS_HOME_CITY] = L"ADDRESS_HOME_CITY";
  autofill_type_to_string[ADDRESS_HOME_STATE] = L"ADDRESS_HOME_STATE";
  autofill_type_to_string[ADDRESS_HOME_ZIP] = L"ADDRESS_HOME_ZIP";
  autofill_type_to_string[ADDRESS_HOME_COUNTRY] = L"ADDRESS_HOME_COUNTRY";
  autofill_type_to_string[PHONE_HOME_NUMBER] = L"PHONE_HOME_NUMBER";
  autofill_type_to_string[PHONE_FAX_NUMBER] = L"PHONE_FAX_NUMBER";
  autofill_type_to_string[NAME_FIRST] = L"NAME_FIRST";
  return autofill_type_to_string;
}

/* static */
std::map<AutoFillFieldType, std::wstring>
    AutomationProvider::GetCreditCardFieldToStringMap() {
  std::map<AutoFillFieldType, std::wstring> credit_card_type_to_string;
  credit_card_type_to_string[CREDIT_CARD_NAME] = L"CREDIT_CARD_NAME";
  credit_card_type_to_string[CREDIT_CARD_NUMBER] = L"CREDIT_CARD_NUMBER";
  credit_card_type_to_string[CREDIT_CARD_EXP_MONTH] = L"CREDIT_CARD_EXP_MONTH";
  credit_card_type_to_string[CREDIT_CARD_EXP_4_DIGIT_YEAR] =
      L"CREDIT_CARD_EXP_4_DIGIT_YEAR";
  return credit_card_type_to_string;
}

void AutomationProvider::SendJSONRequest(int handle,
                                         std::string json_request,
                                         IPC::Message* reply_message) {
  Browser* browser = NULL;
  scoped_ptr<Value> values;

  // Basic error checking.
  if (browser_tracker_->ContainsHandle(handle)) {
    browser = browser_tracker_->GetResource(handle);
  }
  if (!browser) {
    AutomationJSONReply(this, reply_message).SendError("no browser object");
    return;
  }
  base::JSONReader reader;
  std::string error;
  values.reset(reader.ReadAndReturnError(json_request, true, NULL, &error));
  if (!error.empty()) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }

  // Make sure input is a dict with a string command.
  std::string command;
  DictionaryValue* dict_value = NULL;
  if (values->GetType() != Value::TYPE_DICTIONARY) {
    AutomationJSONReply(this, reply_message).SendError("not a dict");
    return;
  }
  // Ownership remains with "values" variable.
  dict_value = static_cast<DictionaryValue*>(values.get());
  if (!dict_value->GetStringASCII(std::string("command"), &command)) {
    AutomationJSONReply(this, reply_message).SendError(
        "no command key in dict or not a string command");
    return;
  }

  // Map json commands to their handlers.
  std::map<std::string, JsonHandler> handler_map;
  handler_map["DisablePlugin"] = &AutomationProvider::DisablePlugin;
  handler_map["EnablePlugin"] = &AutomationProvider::EnablePlugin;
  handler_map["GetPluginsInfo"] = &AutomationProvider::GetPluginsInfo;

  handler_map["GetBrowserInfo"] = &AutomationProvider::GetBrowserInfo;

  handler_map["GetNavigationInfo"] = &AutomationProvider::GetNavigationInfo;

  handler_map["PerformActionOnInfobar"] =
      &AutomationProvider::PerformActionOnInfobar;

  handler_map["GetHistoryInfo"] = &AutomationProvider::GetHistoryInfo;
  handler_map["AddHistoryItem"] = &AutomationProvider::AddHistoryItem;

  handler_map["GetOmniboxInfo"] = &AutomationProvider::GetOmniboxInfo;
  handler_map["SetOmniboxText"] = &AutomationProvider::SetOmniboxText;
  handler_map["OmniboxAcceptInput"] = &AutomationProvider::OmniboxAcceptInput;
  handler_map["OmniboxMovePopupSelection"] =
      &AutomationProvider::OmniboxMovePopupSelection;

  handler_map["GetPrefsInfo"] = &AutomationProvider::GetPrefsInfo;
  handler_map["SetPrefs"] = &AutomationProvider::SetPrefs;

  handler_map["SetWindowDimensions"] = &AutomationProvider::SetWindowDimensions;

  handler_map["GetDownloadsInfo"] = &AutomationProvider::GetDownloadsInfo;
  handler_map["WaitForAllDownloadsToComplete"] =
      &AutomationProvider::WaitForDownloadsToComplete;
  handler_map["PerformActionOnDownload"] =
      &AutomationProvider::PerformActionOnDownload;

  handler_map["GetInitialLoadTimes"] = &AutomationProvider::GetInitialLoadTimes;

  handler_map["SaveTabContents"] = &AutomationProvider::SaveTabContents;

  handler_map["ImportSettings"] = &AutomationProvider::ImportSettings;

  handler_map["AddSavedPassword"] = &AutomationProvider::AddSavedPassword;
  handler_map["RemoveSavedPassword"] =
      &AutomationProvider::RemoveSavedPassword;
  handler_map["GetSavedPasswords"] = &AutomationProvider::GetSavedPasswords;

  handler_map["ClearBrowsingData"] = &AutomationProvider::ClearBrowsingData;

  // SetTheme() implemented using InstallExtension().
  handler_map["GetThemeInfo"] = &AutomationProvider::GetThemeInfo;

  // InstallExtension() present in pyauto.py.
  handler_map["GetExtensionsInfo"] = &AutomationProvider::GetExtensionsInfo;
  handler_map["UninstallExtensionById"] =
      &AutomationProvider::UninstallExtensionById;

  handler_map["SelectTranslateOption"] =
      &AutomationProvider::SelectTranslateOption;
  handler_map["GetTranslateInfo"] =  &AutomationProvider::GetTranslateInfo;

  handler_map["GetAutoFillProfile"] = &AutomationProvider::GetAutoFillProfile;
  handler_map["FillAutoFillProfile"] = &AutomationProvider::FillAutoFillProfile;

  if (handler_map.find(std::string(command)) != handler_map.end()) {
    (this->*handler_map[command])(browser, dict_value, reply_message);
  } else {
    std::string error_string = "Unknown command. Options: ";
    for (std::map<std::string, JsonHandler>::const_iterator it =
         handler_map.begin(); it != handler_map.end(); ++it) {
      error_string += it->first + ", ";
    }
    AutomationJSONReply(this, reply_message).SendError(error_string);
  }
}

class SetProxyConfigTask : public Task {
 public:
  SetProxyConfigTask(URLRequestContextGetter* request_context_getter,
                     const std::string& new_proxy_config)
      : request_context_getter_(request_context_getter),
        proxy_config_(new_proxy_config) {}
  virtual void Run() {
    // First, deserialize the JSON string. If this fails, log and bail.
    JSONStringValueSerializer deserializer(proxy_config_);
    std::string error_msg;
    scoped_ptr<Value> root(deserializer.Deserialize(NULL, &error_msg));
    if (!root.get() || root->GetType() != Value::TYPE_DICTIONARY) {
      DLOG(WARNING) << "Received bad JSON string for ProxyConfig: "
                    << error_msg;
      return;
    }

    scoped_ptr<DictionaryValue> dict(
        static_cast<DictionaryValue*>(root.release()));
    // Now put together a proxy configuration from the deserialized string.
    net::ProxyConfig pc;
    PopulateProxyConfig(*dict.get(), &pc);

    net::ProxyService* proxy_service =
        request_context_getter_->GetURLRequestContext()->proxy_service();
    DCHECK(proxy_service);
    scoped_ptr<net::ProxyConfigService> proxy_config_service(
        new net::ProxyConfigServiceFixed(pc));
    proxy_service->ResetConfigService(proxy_config_service.release());
  }

  void PopulateProxyConfig(const DictionaryValue& dict, net::ProxyConfig* pc) {
    DCHECK(pc);
    bool no_proxy = false;
    if (dict.GetBoolean(automation::kJSONProxyNoProxy, &no_proxy)) {
      // Make no changes to the ProxyConfig.
      return;
    }
    bool auto_config;
    if (dict.GetBoolean(automation::kJSONProxyAutoconfig, &auto_config)) {
      pc->set_auto_detect(true);
    }
    std::string pac_url;
    if (dict.GetString(automation::kJSONProxyPacUrl, &pac_url)) {
      pc->set_pac_url(GURL(pac_url));
    }
    std::string proxy_bypass_list;
    if (dict.GetString(automation::kJSONProxyBypassList, &proxy_bypass_list)) {
      pc->proxy_rules().bypass_rules.ParseFromString(proxy_bypass_list);
    }
    std::string proxy_server;
    if (dict.GetString(automation::kJSONProxyServer, &proxy_server)) {
      pc->proxy_rules().ParseFromString(proxy_server);
    }
  }

 private:
  scoped_refptr<URLRequestContextGetter> request_context_getter_;
  std::string proxy_config_;
};


void AutomationProvider::SetProxyConfig(const std::string& new_proxy_config) {
  URLRequestContextGetter* context_getter = Profile::GetDefaultRequestContext();
  if (!context_getter) {
    FilePath user_data_dir;
    PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    DCHECK(profile_manager);
    Profile* profile = profile_manager->GetDefaultProfile(user_data_dir);
    DCHECK(profile);
    context_getter = profile->GetRequestContext();
  }
  DCHECK(context_getter);

  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      new SetProxyConfigTask(context_getter, new_proxy_config));
}

TabContents* AutomationProvider::GetTabContentsForHandle(
    int handle, NavigationController** tab) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* nav_controller = tab_tracker_->GetResource(handle);
    if (tab)
      *tab = nav_controller;
    return nav_controller->tab_contents();
  }
  return NULL;
}

// Gets the current used encoding name of the page in the specified tab.
void AutomationProvider::GetPageCurrentEncoding(
    int tab_handle, std::string* current_encoding) {
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* nav = tab_tracker_->GetResource(tab_handle);
    Browser* browser = FindAndActivateTab(nav);
    DCHECK(browser);

    if (browser->command_updater()->IsCommandEnabled(IDC_ENCODING_MENU))
      *current_encoding = nav->tab_contents()->encoding();
  }
}

// Gets the current used encoding name of the page in the specified tab.
void AutomationProvider::OverrideEncoding(int tab_handle,
                                          const std::string& encoding_name,
                                          bool* success) {
  *success = false;
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* nav = tab_tracker_->GetResource(tab_handle);
    if (!nav)
      return;
    Browser* browser = FindAndActivateTab(nav);

    // If the browser has UI, simulate what a user would do.
    // Activate the tab and then click the encoding menu.
    if (browser &&
        browser->command_updater()->IsCommandEnabled(IDC_ENCODING_MENU)) {
      int selected_encoding_id =
          CharacterEncoding::GetCommandIdByCanonicalEncodingName(encoding_name);
      if (selected_encoding_id) {
        browser->OverrideEncoding(selected_encoding_id);
        *success = true;
      }
    } else {
      // There is no UI, Chrome probably runs as Chrome-Frame mode.
      // Try to get TabContents and call its override_encoding method.
      TabContents* contents = nav->tab_contents();
      if (!contents)
        return;
      const std::string selected_encoding =
          CharacterEncoding::GetCanonicalEncodingNameByAliasName(encoding_name);
      if (selected_encoding.empty())
        return;
      contents->SetOverrideEncoding(selected_encoding);
    }
  }
}

void AutomationProvider::SelectAll(int tab_handle) {
  RenderViewHost* view = GetViewForTab(tab_handle);
  if (!view) {
    NOTREACHED();
    return;
  }

  view->SelectAll();
}

void AutomationProvider::Cut(int tab_handle) {
  RenderViewHost* view = GetViewForTab(tab_handle);
  if (!view) {
    NOTREACHED();
    return;
  }

  view->Cut();
}

void AutomationProvider::Copy(int tab_handle) {
  RenderViewHost* view = GetViewForTab(tab_handle);
  if (!view) {
    NOTREACHED();
    return;
  }

  view->Copy();
}

void AutomationProvider::Paste(int tab_handle) {
  RenderViewHost* view = GetViewForTab(tab_handle);
  if (!view) {
    NOTREACHED();
    return;
  }

  view->Paste();
}

void AutomationProvider::ReloadAsync(int tab_handle) {
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* tab = tab_tracker_->GetResource(tab_handle);
    if (!tab) {
      NOTREACHED();
      return;
    }

    const bool check_for_repost = true;
    tab->Reload(check_for_repost);
  }
}

void AutomationProvider::StopAsync(int tab_handle) {
  RenderViewHost* view = GetViewForTab(tab_handle);
  if (!view) {
    // We tolerate StopAsync being called even before a view has been created.
    // So just log a warning instead of a NOTREACHED().
    DLOG(WARNING) << "StopAsync: no view for handle " << tab_handle;
    return;
  }

  view->Stop();
}

void AutomationProvider::OnSetPageFontSize(int tab_handle,
                                           int font_size) {
  AutomationPageFontSize automation_font_size =
      static_cast<AutomationPageFontSize>(font_size);

  if (automation_font_size < SMALLEST_FONT ||
      automation_font_size > LARGEST_FONT) {
      DLOG(ERROR) << "Invalid font size specified : "
                  << font_size;
      return;
  }

  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* tab = tab_tracker_->GetResource(tab_handle);
    DCHECK(tab != NULL);
    if (tab && tab->tab_contents()) {
      DCHECK(tab->tab_contents()->profile() != NULL);
      tab->tab_contents()->profile()->GetPrefs()->SetInteger(
          prefs::kWebKitDefaultFontSize, font_size);
    }
  }
}

void AutomationProvider::RemoveBrowsingData(int remove_mask) {
  BrowsingDataRemover* remover;
  remover = new BrowsingDataRemover(profile(),
      BrowsingDataRemover::EVERYTHING,  // All time periods.
      base::Time());
  remover->Remove(remove_mask);
  // BrowsingDataRemover deletes itself.
}

RenderViewHost* AutomationProvider::GetViewForTab(int tab_handle) {
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* tab = tab_tracker_->GetResource(tab_handle);
    if (!tab) {
      NOTREACHED();
      return NULL;
    }

    TabContents* tab_contents = tab->tab_contents();
    if (!tab_contents) {
      NOTREACHED();
      return NULL;
    }

    RenderViewHost* view_host = tab_contents->render_view_host();
    return view_host;
  }

  return NULL;
}

void AutomationProvider::InstallExtension(const FilePath& crx_path,
                                          IPC::Message* reply_message) {
  ExtensionsService* service = profile_->GetExtensionsService();
  if (service) {
    // The observer will delete itself when done.
    new ExtensionInstallNotificationObserver(this,
                                             AutomationMsg_InstallExtension::ID,
                                             reply_message);

    const FilePath& install_dir = service->install_directory();
    scoped_refptr<CrxInstaller> installer(
        new CrxInstaller(install_dir,
                         service,
                         NULL));  // silent install, no UI
    installer->set_allow_privilege_increase(true);
    installer->InstallCrx(crx_path);
  } else {
    AutomationMsg_InstallExtension::WriteReplyParams(
        reply_message, AUTOMATION_MSG_EXTENSION_INSTALL_FAILED);
    Send(reply_message);
  }
}

void AutomationProvider::LoadExpandedExtension(
    const FilePath& extension_dir,
    IPC::Message* reply_message) {
  if (profile_->GetExtensionsService()) {
    // The observer will delete itself when done.
    new ExtensionInstallNotificationObserver(
        this,
        AutomationMsg_LoadExpandedExtension::ID,
        reply_message);

    profile_->GetExtensionsService()->LoadExtension(extension_dir);
  } else {
    AutomationMsg_LoadExpandedExtension::WriteReplyParams(
        reply_message, AUTOMATION_MSG_EXTENSION_INSTALL_FAILED);
    Send(reply_message);
  }
}

void AutomationProvider::GetEnabledExtensions(
    std::vector<FilePath>* result) {
  ExtensionsService* service = profile_->GetExtensionsService();
  DCHECK(service);
  if (service->extensions_enabled()) {
    const ExtensionList* extensions = service->extensions();
    DCHECK(extensions);
    for (size_t i = 0; i < extensions->size(); ++i) {
      Extension* extension = (*extensions)[i];
      DCHECK(extension);
      if (extension->location() == Extension::INTERNAL ||
          extension->location() == Extension::LOAD) {
        result->push_back(extension->path());
      }
    }
  }
}

void AutomationProvider::WaitForExtensionTestResult(
    IPC::Message* reply_message) {
  DCHECK(reply_message_ == NULL);
  reply_message_ = reply_message;
  // Call MaybeSendResult, because the result might have come in before
  // we were waiting on it.
  extension_test_result_observer_->MaybeSendResult();
}

void AutomationProvider::InstallExtensionAndGetHandle(
    const FilePath& crx_path, bool with_ui, IPC::Message* reply_message) {
  ExtensionsService* service = profile_->GetExtensionsService();
  ExtensionProcessManager* manager = profile_->GetExtensionProcessManager();
  if (service && manager) {
    // The observer will delete itself when done.
    new ExtensionReadyNotificationObserver(
        manager,
        this,
        AutomationMsg_InstallExtensionAndGetHandle::ID,
        reply_message);

    ExtensionInstallUI* client =
        (with_ui ? new ExtensionInstallUI(profile_) : NULL);
    scoped_refptr<CrxInstaller> installer(
        new CrxInstaller(service->install_directory(),
                         service,
                         client));
    installer->set_allow_privilege_increase(true);
    installer->InstallCrx(crx_path);
  } else {
    AutomationMsg_InstallExtensionAndGetHandle::WriteReplyParams(
        reply_message, 0);
    Send(reply_message);
  }
}

void AutomationProvider::UninstallExtension(int extension_handle,
                                            bool* success) {
  *success = false;
  Extension* extension = GetExtension(extension_handle);
  ExtensionsService* service = profile_->GetExtensionsService();
  if (extension && service) {
    ExtensionUnloadNotificationObserver observer;
    service->UninstallExtension(extension->id(), false);
    // The extension unload notification should have been sent synchronously
    // with the uninstall. Just to be safe, check that it was received.
    *success = observer.did_receive_unload_notification();
  }
}

void AutomationProvider::EnableExtension(int extension_handle,
                                         IPC::Message* reply_message) {
  Extension* extension = GetDisabledExtension(extension_handle);
  ExtensionsService* service = profile_->GetExtensionsService();
  ExtensionProcessManager* manager = profile_->GetExtensionProcessManager();
  // Only enable if this extension is disabled.
  if (extension && service && manager) {
    // The observer will delete itself when done.
    new ExtensionReadyNotificationObserver(
        manager,
        this,
        AutomationMsg_EnableExtension::ID,
        reply_message);
    service->EnableExtension(extension->id());
  } else {
    AutomationMsg_EnableExtension::WriteReplyParams(reply_message, false);
    Send(reply_message);
  }
}

void AutomationProvider::DisableExtension(int extension_handle,
                                          bool* success) {
  *success = false;
  Extension* extension = GetEnabledExtension(extension_handle);
  ExtensionsService* service = profile_->GetExtensionsService();
  if (extension && service) {
    ExtensionUnloadNotificationObserver observer;
    service->DisableExtension(extension->id());
    // The extension unload notification should have been sent synchronously
    // with the disable. Just to be safe, check that it was received.
    *success = observer.did_receive_unload_notification();
  }
}

void AutomationProvider::ExecuteExtensionActionInActiveTabAsync(
    int extension_handle, int browser_handle,
    IPC::Message* reply_message) {
  bool success = false;
  Extension* extension = GetEnabledExtension(extension_handle);
  ExtensionsService* service = profile_->GetExtensionsService();
  ExtensionMessageService* message_service =
      profile_->GetExtensionMessageService();
  Browser* browser = browser_tracker_->GetResource(browser_handle);
  if (extension && service && message_service && browser) {
    int tab_id = ExtensionTabUtil::GetTabId(browser->GetSelectedTabContents());
    if (extension->page_action()) {
      ExtensionBrowserEventRouter::GetInstance()->PageActionExecuted(
          browser->profile(), extension->id(), "action", tab_id, "", 1);
      success = true;
    } else if (extension->browser_action()) {
      ExtensionBrowserEventRouter::GetInstance()->BrowserActionExecuted(
          browser->profile(), extension->id(), browser);
      success = true;
    }
  }
  AutomationMsg_ExecuteExtensionActionInActiveTabAsync::WriteReplyParams(
      reply_message, success);
  Send(reply_message);
}

void AutomationProvider::MoveExtensionBrowserAction(
    int extension_handle, int index, bool* success) {
  *success = false;
  Extension* extension = GetEnabledExtension(extension_handle);
  ExtensionsService* service = profile_->GetExtensionsService();
  if (extension && service) {
    ExtensionToolbarModel* toolbar = service->toolbar_model();
    if (toolbar) {
      if (index >= 0 && index < static_cast<int>(toolbar->size())) {
        toolbar->MoveBrowserAction(extension, index);
        *success = true;
      } else {
        DLOG(WARNING) << "Attempted to move browser action to invalid index.";
      }
    }
  }
}

void AutomationProvider::GetExtensionProperty(
    int extension_handle,
    AutomationMsg_ExtensionProperty type,
    bool* success,
    std::string* value) {
  *success = false;
  Extension* extension = GetExtension(extension_handle);
  ExtensionsService* service = profile_->GetExtensionsService();
  if (extension && service) {
    ExtensionToolbarModel* toolbar = service->toolbar_model();
    int found_index = -1;
    int index = 0;
    switch (type) {
      case AUTOMATION_MSG_EXTENSION_ID:
        *value = extension->id();
        *success = true;
        break;
      case AUTOMATION_MSG_EXTENSION_NAME:
        *value = extension->name();
        *success = true;
        break;
      case AUTOMATION_MSG_EXTENSION_VERSION:
        *value = extension->VersionString();
        *success = true;
        break;
      case AUTOMATION_MSG_EXTENSION_BROWSER_ACTION_INDEX:
        if (toolbar) {
          for (ExtensionList::const_iterator iter = toolbar->begin();
               iter != toolbar->end(); iter++) {
            // Skip this extension if we are in incognito mode
            // and it is not incognito-enabled.
            if (profile_->IsOffTheRecord() &&
                !service->IsIncognitoEnabled(*iter))
              continue;
            if (*iter == extension) {
              found_index = index;
              break;
            }
            index++;
          }
          *value = base::IntToString(found_index);
          *success = true;
        }
        break;
      default:
        LOG(WARNING) << "Trying to get undefined extension property";
        break;
    }
  }
}

void AutomationProvider::SaveAsAsync(int tab_handle) {
  NavigationController* tab = NULL;
  TabContents* tab_contents = GetTabContentsForHandle(tab_handle, &tab);
  if (tab_contents)
    tab_contents->OnSavePage();
}

void AutomationProvider::SetContentSetting(
    int handle,
    const std::string& host,
    ContentSettingsType content_type,
    ContentSetting setting,
    bool* success) {
  *success = false;
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    HostContentSettingsMap* map =
        browser->profile()->GetHostContentSettingsMap();
    if (host.empty()) {
      map->SetDefaultContentSetting(content_type, setting);
    } else {
      map->SetContentSetting(HostContentSettingsMap::Pattern(host),
                             content_type, "", setting);
    }
    *success = true;
  }
}

void AutomationProvider::ResetToDefaultTheme() {
  profile_->ClearTheme();
}
