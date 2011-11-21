// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_provider.h"

#include <set>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/file_path.h"
#include "base/json/json_reader.h"
#include "base/json/json_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/task.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/automation/automation_browser_tracker.h"
#include "chrome/browser/automation/automation_extension_tracker.h"
#include "chrome/browser/automation/automation_provider_list.h"
#include "chrome/browser/automation/automation_provider_observers.h"
#include "chrome/browser/automation/automation_resource_message_filter.h"
#include "chrome/browser/automation/automation_tab_tracker.h"
#include "chrome/browser/automation/automation_window_tracker.h"
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_storage.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data_remover.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/dom_operation_notification_details.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_browser_event_router.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/user_script_master.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog_queue.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_notification_details.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/login/login_prompt.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/automation_constants.h"
#include "chrome/common/automation_messages.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/browser/debugger/devtools_manager.h"
#include "content/browser/download/download_item.h"
#include "content/browser/download/save_package.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/ssl/ssl_manager.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/public/browser/browser_thread.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFindOptions.h"
#include "webkit/glue/password_form.h"

#if defined(OS_WIN) && !defined(USE_AURA)
#include "chrome/browser/external_tab_container_win.h"
#endif  // defined(OS_WIN)

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#endif  // defined(OS_CHROMEOS)

using content::BrowserThread;
using WebKit::WebFindOptions;
using base::Time;

AutomationProvider::AutomationProvider(Profile* profile)
    : profile_(profile),
      reply_message_(NULL),
      reinitialize_on_channel_error_(
          CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kAutomationReinitializeOnChannelError)),
      is_connected_(false),
      initial_tab_loads_complete_(false),
      network_library_initialized_(true),
      login_webui_ready_(true) {
  TRACE_EVENT_BEGIN_ETW("AutomationProvider::AutomationProvider", 0, "");

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  browser_tracker_.reset(new AutomationBrowserTracker(this));
  extension_tracker_.reset(new AutomationExtensionTracker(this));
  tab_tracker_.reset(new AutomationTabTracker(this));
  window_tracker_.reset(new AutomationWindowTracker(this));
  new_tab_ui_load_observer_.reset(new NewTabUILoadObserver(this, profile));
  metric_event_duration_observer_.reset(new MetricEventDurationObserver());
  extension_test_result_observer_.reset(
      new ExtensionTestResultNotificationObserver(this));

  TRACE_EVENT_END_ETW("AutomationProvider::AutomationProvider", 0, "");
}

AutomationProvider::~AutomationProvider() {
  if (channel_.get())
    channel_->Close();
}

bool AutomationProvider::InitializeChannel(const std::string& channel_id) {
  TRACE_EVENT_BEGIN_ETW("AutomationProvider::InitializeChannel", 0, "");

  channel_id_ = channel_id;
  std::string effective_channel_id = channel_id;

  // If the channel_id starts with kNamedInterfacePrefix, create a named IPC
  // server and listen on it, else connect as client to an existing IPC server
  bool use_named_interface =
      channel_id.find(automation::kNamedInterfacePrefix) == 0;
  if (use_named_interface) {
    effective_channel_id = channel_id.substr(
        strlen(automation::kNamedInterfacePrefix));
    if (effective_channel_id.length() <= 0)
      return false;

    reinitialize_on_channel_error_ = true;
  }

  if (!automation_resource_message_filter_.get()) {
    automation_resource_message_filter_ = new AutomationResourceMessageFilter;
  }

  channel_.reset(new IPC::ChannelProxy(
      effective_channel_id,
      GetChannelMode(use_named_interface),
      this,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO)));
  channel_->AddFilter(automation_resource_message_filter_);

#if defined(OS_CHROMEOS)
  // Wait for webui login to be ready.
  // Observer will delete itself.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kLoginManager) &&
      !chromeos::UserManager::Get()->user_is_logged_in()) {
    login_webui_ready_ = false;
    new LoginWebuiReadyObserver(this);
  }

  // Wait for the network manager to initialize.
  // The observer will delete itself when done.
  network_library_initialized_ = false;
  NetworkManagerInitObserver* observer = new NetworkManagerInitObserver(this);
  if (!observer->Init())
    delete observer;
#endif

  TRACE_EVENT_END_ETW("AutomationProvider::InitializeChannel", 0, "");

  return true;
}

IPC::Channel::Mode AutomationProvider::GetChannelMode(
    bool use_named_interface) {
  if (use_named_interface)
    return IPC::Channel::MODE_NAMED_SERVER;
  else
    return IPC::Channel::MODE_CLIENT;
}

std::string AutomationProvider::GetProtocolVersion() {
  chrome::VersionInfo version_info;
  return version_info.Version().c_str();
}

void AutomationProvider::SetExpectedTabCount(size_t expected_tabs) {
  if (expected_tabs == 0)
    OnInitialTabLoadsComplete();
  else
    initial_load_observer_.reset(new InitialLoadObserver(expected_tabs, this));
}

void AutomationProvider::OnInitialTabLoadsComplete() {
  initial_tab_loads_complete_ = true;
  VLOG(2) << "OnInitialTabLoadsComplete";
  if (is_connected_ && network_library_initialized_ && login_webui_ready_)
    Send(new AutomationMsg_InitialLoadsComplete());
}

void AutomationProvider::OnNetworkLibraryInit() {
  network_library_initialized_ = true;
  VLOG(2) << "OnNetworkLibraryInit";
  if (is_connected_ && initial_tab_loads_complete_ && login_webui_ready_)
    Send(new AutomationMsg_InitialLoadsComplete());
}

void AutomationProvider::OnLoginWebuiReady() {
  login_webui_ready_ = true;
  VLOG(2) << "OnLoginWebuiReady";
  if (is_connected_ && initial_tab_loads_complete_ &&
      network_library_initialized_)
    Send(new AutomationMsg_InitialLoadsComplete());
}

void AutomationProvider::AddLoginHandler(NavigationController* tab,
                                         LoginHandler* handler) {
  login_handler_map_[tab] = handler;
}

void AutomationProvider::RemoveLoginHandler(NavigationController* tab) {
  DCHECK(login_handler_map_[tab]);
  login_handler_map_.erase(tab);
}

int AutomationProvider::GetIndexForNavigationController(
    const NavigationController* controller, const Browser* parent) const {
  DCHECK(parent);
  return parent->GetIndexOfController(controller);
}

int AutomationProvider::AddExtension(const Extension* extension) {
  DCHECK(extension);
  return extension_tracker_->Add(extension);
}

// TODO(phajdan.jr): move to TestingAutomationProvider.
DictionaryValue* AutomationProvider::GetDictionaryFromDownloadItem(
    const DownloadItem* download) {
  std::map<DownloadItem::DownloadState, std::string> state_to_string;
  state_to_string[DownloadItem::IN_PROGRESS] = std::string("IN_PROGRESS");
  state_to_string[DownloadItem::CANCELLED] = std::string("CANCELLED");
  state_to_string[DownloadItem::REMOVING] = std::string("REMOVING");
  state_to_string[DownloadItem::INTERRUPTED] = std::string("INTERRUPTED");
  state_to_string[DownloadItem::COMPLETE] = std::string("COMPLETE");

  std::map<DownloadItem::SafetyState, std::string> safety_state_to_string;
  safety_state_to_string[DownloadItem::SAFE] = std::string("SAFE");
  safety_state_to_string[DownloadItem::DANGEROUS] = std::string("DANGEROUS");
  safety_state_to_string[DownloadItem::DANGEROUS_BUT_VALIDATED] =
      std::string("DANGEROUS_BUT_VALIDATED");

  DictionaryValue* dl_item_value = new DictionaryValue;
  dl_item_value->SetInteger("id", static_cast<int>(download->GetId()));
  dl_item_value->SetString("url", download->GetURL().spec());
  dl_item_value->SetString("referrer_url", download->GetReferrerUrl().spec());
  dl_item_value->SetString("file_name",
                           download->GetFileNameToReportUser().value());
  dl_item_value->SetString("full_path",
                           download->GetTargetFilePath().value());
  dl_item_value->SetBoolean("is_paused", download->IsPaused());
  dl_item_value->SetBoolean("open_when_complete",
                            download->GetOpenWhenComplete());
  dl_item_value->SetBoolean("is_temporary", download->IsTemporary());
  dl_item_value->SetBoolean("is_otr", download->IsOtr());  // incognito
  dl_item_value->SetString("state", state_to_string[download->GetState()]);
  dl_item_value->SetString("safety_state",
                           safety_state_to_string[download->GetSafetyState()]);
  dl_item_value->SetInteger("PercentComplete", download->PercentComplete());

  return dl_item_value;
}

const Extension* AutomationProvider::GetExtension(int extension_handle) {
  return extension_tracker_->GetResource(extension_handle);
}

const Extension* AutomationProvider::GetEnabledExtension(int extension_handle) {
  const Extension* extension =
      extension_tracker_->GetResource(extension_handle);
  ExtensionService* service = profile_->GetExtensionService();
  if (extension && service &&
      service->GetExtensionById(extension->id(), false))
    return extension;
  return NULL;
}

const Extension* AutomationProvider::GetDisabledExtension(
    int extension_handle) {
  const Extension* extension =
      extension_tracker_->GetResource(extension_handle);
  ExtensionService* service = profile_->GetExtensionService();
  if (extension && service &&
      service->GetExtensionById(extension->id(), true) &&
      !service->GetExtensionById(extension->id(), false))
    return extension;
  return NULL;
}

void AutomationProvider::OnChannelConnected(int pid) {
  is_connected_ = true;
  LOG(INFO) << "Testing channel connected, sending hello message";

  // Send a hello message with our current automation protocol version.
  channel_->Send(new AutomationMsg_Hello(GetProtocolVersion()));
  if (initial_tab_loads_complete_ && network_library_initialized_ &&
      login_webui_ready_)
    Send(new AutomationMsg_InitialLoadsComplete());
}

void AutomationProvider::OnEndTracingComplete() {
  IPC::Message* reply_message = tracing_data_.reply_message.release();
  if (reply_message) {
    AutomationMsg_EndTracing::WriteReplyParams(
        reply_message, tracing_data_.trace_output.size(), true);
    Send(reply_message);
  }
}

void AutomationProvider::OnTraceDataCollected(
    const std::string& trace_fragment) {
  tracing_data_.trace_output.push_back(trace_fragment);
}

bool AutomationProvider::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  bool deserialize_success = true;
  IPC_BEGIN_MESSAGE_MAP_EX(AutomationProvider, message, deserialize_success)
#if !defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_WindowDrag,
                                    WindowSimulateDrag)
#endif  // !defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(AutomationMsg_HandleUnused, HandleUnused)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetProxyConfig, SetProxyConfig)
    IPC_MESSAGE_HANDLER(AutomationMsg_PrintAsync, PrintAsync)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_Find, HandleFindRequest)
    IPC_MESSAGE_HANDLER(AutomationMsg_OverrideEncoding, OverrideEncoding)
    IPC_MESSAGE_HANDLER(AutomationMsg_SelectAll, SelectAll)
    IPC_MESSAGE_HANDLER(AutomationMsg_Cut, Cut)
    IPC_MESSAGE_HANDLER(AutomationMsg_Copy, Copy)
    IPC_MESSAGE_HANDLER(AutomationMsg_Paste, Paste)
    IPC_MESSAGE_HANDLER(AutomationMsg_ReloadAsync, ReloadAsync)
    IPC_MESSAGE_HANDLER(AutomationMsg_StopAsync, StopAsync)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetPageFontSize, OnSetPageFontSize)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_WaitForExtensionTestResult,
                                    WaitForExtensionTestResult)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_InstallExtension,
                                    InstallExtension)
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
    IPC_MESSAGE_HANDLER(AutomationMsg_SaveAsAsync, SaveAsAsync)
    IPC_MESSAGE_HANDLER(AutomationMsg_RemoveBrowsingData, RemoveBrowsingData)
    IPC_MESSAGE_HANDLER(AutomationMsg_JavaScriptStressTestControl,
                        JavaScriptStressTestControl)
    IPC_MESSAGE_HANDLER(AutomationMsg_BeginTracing, BeginTracing)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_EndTracing, EndTracing)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetTracingOutput, GetTracingOutput)
#if defined(OS_WIN) && !defined(USE_AURA)
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
    IPC_MESSAGE_HANDLER(AutomationMsg_HandleMessageFromExternalHost,
                        OnMessageFromExternalHost)
    IPC_MESSAGE_HANDLER(AutomationMsg_BrowserMove, OnBrowserMoved)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_RunUnloadHandlers,
                                    OnRunUnloadHandlers)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetZoomLevel, OnSetZoomLevel)
#endif  // defined(OS_WIN)
    IPC_MESSAGE_UNHANDLED(handled = false; OnUnhandledMessage())
  IPC_END_MESSAGE_MAP_EX()
  if (!deserialize_success)
    OnMessageDeserializationFailure();
  return handled;
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

void AutomationProvider::OnMessageDeserializationFailure() {
  LOG(ERROR) << "Failed to deserialize IPC message. "
             << "Closing the automation channel.";
  channel_->Close();
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

bool AutomationProvider::ReinitializeChannel() {
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  // Make sure any old channels are cleaned up before starting up a new one.
  channel_.reset();
  return InitializeChannel(channel_id_);
}

void AutomationProvider::OnChannelError() {
  if (reinitialize_on_channel_error_) {
    VLOG(1) << "AutomationProxy disconnected, resetting AutomationProvider.";
    if (ReinitializeChannel())
      return;
    VLOG(1) << "Error reinitializing AutomationProvider channel.";
  }
  VLOG(1) << "AutomationProxy went away, shutting down app.";
  g_browser_process->GetAutomationProviderList()->RemoveProvider(this);
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
    browser->ActivateTabAt(tab_index, true);

  return browser;
}

void AutomationProvider::HandleFindRequest(
    int handle,
    const AutomationMsg_Find_Params& params,
    IPC::Message* reply_message) {
  if (!tab_tracker_->ContainsHandle(handle)) {
    AutomationMsg_Find::WriteReplyParams(reply_message, -1, -1);
    Send(reply_message);
    return;
  }

  NavigationController* nav = tab_tracker_->GetResource(handle);
  TabContents* tab_contents = nav->tab_contents();

  SendFindRequest(tab_contents,
                  false,
                  params.search_string,
                  params.forward,
                  params.match_case,
                  params.find_next,
                  reply_message);
}

void AutomationProvider::SendFindRequest(
    TabContents* tab_contents,
    bool with_json,
    const string16& search_string,
    bool forward,
    bool match_case,
    bool find_next,
    IPC::Message* reply_message) {
  int request_id = FindInPageNotificationObserver::kFindInPageRequestId;
  FindInPageNotificationObserver* observer =
      new FindInPageNotificationObserver(this,
                                         tab_contents,
                                         with_json,
                                         reply_message);
  if (!with_json) {
    find_in_page_observer_.reset(observer);
  }
  TabContentsWrapper* wrapper =
      TabContentsWrapper::GetCurrentWrapperForContents(tab_contents);
  if (wrapper)
    wrapper->find_tab_helper()->set_current_find_request_id(request_id);

  WebFindOptions options;
  options.forward = forward;
  options.matchCase = match_case;
  options.findNext = find_next;
  tab_contents->render_view_host()->Find(
      FindInPageNotificationObserver::kFindInPageRequestId, search_string,
      options);
}

class SetProxyConfigTask : public Task {
 public:
  SetProxyConfigTask(net::URLRequestContextGetter* request_context_getter,
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
    bool pac_mandatory;
    if (dict.GetBoolean(automation::kJSONProxyPacMandatory, &pac_mandatory)) {
      pc->set_pac_mandatory(pac_mandatory);
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
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  std::string proxy_config_;
};


void AutomationProvider::SetProxyConfig(const std::string& new_proxy_config) {
  net::URLRequestContextGetter* context_getter =
      profile_->GetRequestContext();
  DCHECK(context_getter);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
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
      DCHECK(tab->tab_contents()->browser_context() != NULL);
      Profile* profile =
          Profile::FromBrowserContext(tab->tab_contents()->browser_context());
      profile->GetPrefs()->SetInteger(prefs::kWebKitDefaultFontSize, font_size);
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

void AutomationProvider::JavaScriptStressTestControl(int tab_handle,
                                                     int cmd,
                                                     int param) {
  RenderViewHost* view = GetViewForTab(tab_handle);
  if (!view) {
    NOTREACHED();
    return;
  }

  view->Send(new ChromeViewMsg_JavaScriptStressTestControl(
      view->routing_id(), cmd, param));
}

void AutomationProvider::BeginTracing(const std::string& categories,
                                      bool* success) {
  tracing_data_.trace_output.clear();
  *success = TraceController::GetInstance()->BeginTracing(this, categories);
}

void AutomationProvider::EndTracing(IPC::Message* reply_message) {
  bool success = false;
  if (!tracing_data_.reply_message.get())
    success = TraceController::GetInstance()->EndTracingAsync(this);
  if (success) {
    // Defer EndTracing reply until TraceController calls us back with all the
    // events.
    tracing_data_.reply_message.reset(reply_message);
  } else {
    // If failed to call EndTracingAsync, need to reply with failure now.
    AutomationMsg_EndTracing::WriteReplyParams(reply_message, size_t(0), false);
    Send(reply_message);
  }
}

void AutomationProvider::GetTracingOutput(std::string* chunk,
                                          bool* success) {
  // The JSON data is sent back to the test in chunks, because IPC sends will
  // fail if they are too large.
  if (tracing_data_.trace_output.empty()) {
    *chunk = "";
    *success = false;
  } else {
    *chunk = tracing_data_.trace_output.front();
    tracing_data_.trace_output.pop_front();
    *success = true;
  }
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

void AutomationProvider::WaitForExtensionTestResult(
    IPC::Message* reply_message) {
  DCHECK(!reply_message_);
  reply_message_ = reply_message;
  // Call MaybeSendResult, because the result might have come in before
  // we were waiting on it.
  extension_test_result_observer_->MaybeSendResult();
}

void AutomationProvider::InstallExtension(
    const FilePath& extension_path, bool with_ui,
    IPC::Message* reply_message) {
  ExtensionService* service = profile_->GetExtensionService();
  ExtensionProcessManager* manager = profile_->GetExtensionProcessManager();
  if (service && manager) {
    // The observer will delete itself when done.
    new ExtensionReadyNotificationObserver(
        manager,
        service,
        this,
        AutomationMsg_InstallExtension::ID,
        reply_message);

    if (extension_path.MatchesExtension(FILE_PATH_LITERAL(".crx"))) {
      ExtensionInstallUI* client =
          (with_ui ? new ExtensionInstallUI(profile_) : NULL);
      scoped_refptr<CrxInstaller> installer(
          CrxInstaller::Create(service, client));
      if (!with_ui)
        installer->set_allow_silent_install(true);
      installer->set_install_cause(extension_misc::INSTALL_CAUSE_AUTOMATION);
      installer->InstallCrx(extension_path);
    } else {
      scoped_refptr<extensions::UnpackedInstaller> installer(
          extensions::UnpackedInstaller::Create(service));
      installer->set_prompt_for_plugins(with_ui);
      installer->Load(extension_path);
    }
  } else {
    AutomationMsg_InstallExtension::WriteReplyParams(reply_message, 0);
    Send(reply_message);
  }
}

void AutomationProvider::UninstallExtension(int extension_handle,
                                            bool* success) {
  *success = false;
  const Extension* extension = GetExtension(extension_handle);
  ExtensionService* service = profile_->GetExtensionService();
  if (extension && service) {
    ExtensionUnloadNotificationObserver observer;
    service->UninstallExtension(extension->id(), false, NULL);
    // The extension unload notification should have been sent synchronously
    // with the uninstall. Just to be safe, check that it was received.
    *success = observer.did_receive_unload_notification();
  }
}

void AutomationProvider::EnableExtension(int extension_handle,
                                         IPC::Message* reply_message) {
  const Extension* extension = GetDisabledExtension(extension_handle);
  ExtensionService* service = profile_->GetExtensionService();
  ExtensionProcessManager* manager = profile_->GetExtensionProcessManager();
  // Only enable if this extension is disabled.
  if (extension && service && manager) {
    // The observer will delete itself when done.
    new ExtensionReadyNotificationObserver(
        manager,
        service,
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
  const Extension* extension = GetEnabledExtension(extension_handle);
  ExtensionService* service = profile_->GetExtensionService();
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
  const Extension* extension = GetEnabledExtension(extension_handle);
  ExtensionService* service = profile_->GetExtensionService();
  ExtensionMessageService* message_service =
      profile_->GetExtensionMessageService();
  Browser* browser = browser_tracker_->GetResource(browser_handle);
  if (extension && service && message_service && browser) {
    int tab_id = ExtensionTabUtil::GetTabId(browser->GetSelectedTabContents());
    if (extension->page_action()) {
      service->browser_event_router()->PageActionExecuted(
          browser->profile(), extension->id(), "action", tab_id, "", 1);
      success = true;
    } else if (extension->browser_action()) {
      service->browser_event_router()->BrowserActionExecuted(
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
  const Extension* extension = GetEnabledExtension(extension_handle);
  ExtensionService* service = profile_->GetExtensionService();
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
  const Extension* extension = GetExtension(extension_handle);
  ExtensionService* service = profile_->GetExtensionService();
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
                !service->IsIncognitoEnabled((*iter)->id()))
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
