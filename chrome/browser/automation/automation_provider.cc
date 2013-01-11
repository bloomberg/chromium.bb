// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/automation_provider.h"

#include <set>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/file_path.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/automation/automation_browser_tracker.h"
#include "chrome/browser/automation/automation_provider_list.h"
#include "chrome/browser/automation/automation_provider_observers.h"
#include "chrome/browser/automation/automation_resource_message_filter.h"
#include "chrome/browser/automation/automation_tab_tracker.h"
#include "chrome/browser/automation/automation_window_tracker.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_storage.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog_queue.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/find_bar/find_notification_details.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/login/login_prompt.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/common/automation_constants.h"
#include "chrome/common/automation_messages.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/trace_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFindOptions.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#endif  // defined(OS_CHROMEOS)

using WebKit::WebFindOptions;
using base::Time;
using content::BrowserThread;
using content::DownloadItem;
using content::NavigationController;
using content::RenderViewHost;
using content::TraceController;
using content::WebContents;

namespace {

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

void SetProxyConfigCallback(
    const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
    const std::string& proxy_config) {
  // First, deserialize the JSON string. If this fails, log and bail.
  JSONStringValueSerializer deserializer(proxy_config);
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
      request_context_getter->GetURLRequestContext()->proxy_service();
  DCHECK(proxy_service);
  scoped_ptr<net::ProxyConfigService> proxy_config_service(
      new net::ProxyConfigServiceFixed(pc));
  proxy_service->ResetConfigService(proxy_config_service.release());
}

}  // namespace

AutomationProvider::AutomationProvider(Profile* profile)
    : profile_(profile),
      reply_message_(NULL),
      reinitialize_on_channel_error_(
          CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kAutomationReinitializeOnChannelError)),
      use_initial_load_observers_(true),
      is_connected_(false),
      initial_tab_loads_complete_(false),
      network_library_initialized_(true),
      login_webui_ready_(true) {
  TRACE_EVENT_BEGIN_ETW("AutomationProvider::AutomationProvider", 0, "");

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  browser_tracker_.reset(new AutomationBrowserTracker(this));
  tab_tracker_.reset(new AutomationTabTracker(this));
  window_tracker_.reset(new AutomationWindowTracker(this));
  new_tab_ui_load_observer_.reset(new NewTabUILoadObserver(this, profile));
  metric_event_duration_observer_.reset(new MetricEventDurationObserver());

  TRACE_EVENT_END_ETW("AutomationProvider::AutomationProvider", 0, "");
}

AutomationProvider::~AutomationProvider() {
  if (channel_.get())
    channel_->Close();
}

void AutomationProvider::set_profile(Profile* profile) {
  profile_ = profile;
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
  if (use_initial_load_observers_) {
    // Wait for webui login to be ready.
    // Observer will delete itself.
    if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kLoginManager) &&
        !chromeos::UserManager::Get()->IsUserLoggedIn()) {
      login_webui_ready_ = false;
      new OOBEWebuiReadyObserver(this);
    }

    // Wait for the network manager to initialize.
    // The observer will delete itself when done.
    network_library_initialized_ = false;
    NetworkManagerInitObserver* observer = new NetworkManagerInitObserver(this);
    if (!observer->Init())
      delete observer;
  }
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
  VLOG(2) << "SetExpectedTabCount:" << expected_tabs;
  if (expected_tabs == 0)
    OnInitialTabLoadsComplete();
  else
    initial_load_observer_.reset(new InitialLoadObserver(expected_tabs, this));
}

void AutomationProvider::OnInitialTabLoadsComplete() {
  initial_tab_loads_complete_ = true;
  VLOG(2) << "OnInitialTabLoadsComplete";
  SendInitialLoadMessage();
}

void AutomationProvider::OnNetworkLibraryInit() {
  network_library_initialized_ = true;
  VLOG(2) << "OnNetworkLibraryInit";
  SendInitialLoadMessage();
}

void AutomationProvider::OnOOBEWebuiReady() {
  login_webui_ready_ = true;
  VLOG(2) << "OnOOBEWebuiReady";
  SendInitialLoadMessage();
}

void AutomationProvider::SendInitialLoadMessage() {
  if (is_connected_ && initial_tab_loads_complete_ &&
      network_library_initialized_ && login_webui_ready_) {
    VLOG(2) << "Initial loads complete; sending initial loads message.";
    Send(new AutomationMsg_InitialLoadsComplete());
  }
}

void AutomationProvider::DisableInitialLoadObservers() {
  use_initial_load_observers_ = false;
  OnInitialTabLoadsComplete();
  OnNetworkLibraryInit();
  OnOOBEWebuiReady();
}

int AutomationProvider::GetIndexForNavigationController(
    const NavigationController* controller, const Browser* parent) const {
  DCHECK(parent);
  return parent->tab_strip_model()->GetIndexOfWebContents(
      controller->GetWebContents());
}

// TODO(phajdan.jr): move to TestingAutomationProvider.
DictionaryValue* AutomationProvider::GetDictionaryFromDownloadItem(
    const DownloadItem* download, bool incognito) {
  std::map<DownloadItem::DownloadState, std::string> state_to_string;
  state_to_string[DownloadItem::IN_PROGRESS] = std::string("IN_PROGRESS");
  state_to_string[DownloadItem::CANCELLED] = std::string("CANCELLED");
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
  dl_item_value->SetBoolean("is_otr", incognito);
  dl_item_value->SetString("state", state_to_string[download->GetState()]);
  dl_item_value->SetString("safety_state",
                           safety_state_to_string[download->GetSafetyState()]);
  dl_item_value->SetInteger("PercentComplete", download->PercentComplete());

  return dl_item_value;
}

void AutomationProvider::OnChannelConnected(int pid) {
  is_connected_ = true;

  // Send a hello message with our current automation protocol version.
  VLOG(2) << "Testing channel connected, sending hello message";
  channel_->Send(new AutomationMsg_Hello(GetProtocolVersion()));

  SendInitialLoadMessage();
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
    const scoped_refptr<base::RefCountedString>& trace_fragment) {
  tracing_data_.trace_output.push_back(trace_fragment->data());
}

bool AutomationProvider::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  bool deserialize_success = true;
  IPC_BEGIN_MESSAGE_MAP_EX(AutomationProvider, message, deserialize_success)
    IPC_MESSAGE_HANDLER(AutomationMsg_HandleUnused, HandleUnused)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetProxyConfig, SetProxyConfig)
    IPC_MESSAGE_HANDLER(AutomationMsg_PrintAsync, PrintAsync)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_Find, HandleFindRequest)
    IPC_MESSAGE_HANDLER(AutomationMsg_OverrideEncoding, OverrideEncoding)
    IPC_MESSAGE_HANDLER(AutomationMsg_SelectAll, SelectAll)
    IPC_MESSAGE_HANDLER(AutomationMsg_Cut, Cut)
    IPC_MESSAGE_HANDLER(AutomationMsg_Copy, Copy)
    IPC_MESSAGE_HANDLER(AutomationMsg_Paste, Paste)
    IPC_MESSAGE_HANDLER(AutomationMsg_KeyPress, KeyPress)
    IPC_MESSAGE_HANDLER(AutomationMsg_ReloadAsync, ReloadAsync)
    IPC_MESSAGE_HANDLER(AutomationMsg_StopAsync, StopAsync)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetPageFontSize, OnSetPageFontSize)
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
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  if (!handled)
    OnUnhandledMessage(message);
  if (!deserialize_success)
    OnMessageDeserializationFailure();
  return handled;
}

void AutomationProvider::OnUnhandledMessage(const IPC::Message& message) {
  // We should not hang here. Print a message to indicate what's going on,
  // and disconnect the channel to notify the caller about the error
  // in a way it can't ignore, and make any further attempts to send
  // messages fail fast.
  LOG(ERROR) << "AutomationProvider received a message it can't handle. "
             << "Message type: " << message.type()
             << ", routing ID: " << message.routing_id() << ". "
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
  content::WebContentsDelegate* d = controller->GetWebContents()->GetDelegate();
  if (d)
    d->ActivateContents(controller->GetWebContents());
  return chrome::FindBrowserWithWebContents(controller->GetWebContents());
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
  WebContents* web_contents = nav->GetWebContents();

  SendFindRequest(web_contents,
                  false,
                  params.search_string,
                  params.forward,
                  params.match_case,
                  params.find_next,
                  reply_message);
}

void AutomationProvider::SendFindRequest(
    WebContents* web_contents,
    bool with_json,
    const string16& search_string,
    bool forward,
    bool match_case,
    bool find_next,
    IPC::Message* reply_message) {
  int request_id = FindInPageNotificationObserver::kFindInPageRequestId;
  FindInPageNotificationObserver* observer =
      new FindInPageNotificationObserver(this,
                                         web_contents,
                                         with_json,
                                         reply_message);
  if (!with_json) {
    find_in_page_observer_.reset(observer);
  }
  FindTabHelper* find_tab_helper = FindTabHelper::FromWebContents(web_contents);
  if (find_tab_helper)
    find_tab_helper->set_current_find_request_id(request_id);

  WebFindOptions options;
  options.forward = forward;
  options.matchCase = match_case;
  options.findNext = find_next;
  web_contents->GetRenderViewHost()->Find(
      FindInPageNotificationObserver::kFindInPageRequestId, search_string,
      options);
}

void AutomationProvider::SetProxyConfig(const std::string& new_proxy_config) {
  net::URLRequestContextGetter* context_getter =
      profile_->GetRequestContext();
  DCHECK(context_getter);

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(SetProxyConfigCallback, make_scoped_refptr(context_getter),
                 new_proxy_config));
}

WebContents* AutomationProvider::GetWebContentsForHandle(
    int handle, NavigationController** tab) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* nav_controller = tab_tracker_->GetResource(handle);
    if (tab)
      *tab = nav_controller;
    return nav_controller->GetWebContents();
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
    if (browser && chrome::IsCommandEnabled(browser, IDC_ENCODING_MENU)) {
      int selected_encoding_id =
          CharacterEncoding::GetCommandIdByCanonicalEncodingName(encoding_name);
      if (selected_encoding_id) {
        browser->OverrideEncoding(selected_encoding_id);
        *success = true;
      }
    } else {
      // There is no UI, Chrome probably runs as Chrome-Frame mode.
      // Try to get WebContents and call its override_encoding method.
      WebContents* contents = nav->GetWebContents();
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



void AutomationProvider::KeyPress(int tab_handle, int key) {
  RenderViewHost* view = GetViewForTab(tab_handle);
  if (!view) {
    NOTREACHED();
    return;
  }

  content::NativeWebKeyboardEvent event;
  event.nativeKeyCode = 0;
  event.windowsKeyCode = key;
  event.setKeyIdentifierFromWindowsKeyCode();
  event.modifiers = 0;
  event.isSystemKey = false;
  event.timeStampSeconds = base::Time::Now().ToDoubleT();
  event.skip_in_browser = true;

  event.text[0] = key;
  event.unmodifiedText[0] = key;
  event.type = WebKit::WebInputEvent::RawKeyDown;
  view->ForwardKeyboardEvent(event);

  event.type = WebKit::WebInputEvent::Char;
  view->ForwardKeyboardEvent(event);

  event.type = WebKit::WebInputEvent::KeyUp;
  event.text[0] = 0;
  event.unmodifiedText[0] = 0;
  view->ForwardKeyboardEvent(event);
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
    if (tab && tab->GetWebContents()) {
      DCHECK(tab->GetWebContents()->GetBrowserContext() != NULL);
      Profile* profile = Profile::FromBrowserContext(
          tab->GetWebContents()->GetBrowserContext());
      profile->GetPrefs()->SetInteger(prefs::kWebKitDefaultFontSize, font_size);
    }
  }
}

void AutomationProvider::RemoveBrowsingData(int remove_mask) {
  BrowsingDataRemover* remover;
  remover = BrowsingDataRemover::CreateForUnboundedRange(profile());
  remover->Remove(remove_mask, BrowsingDataHelper::UNPROTECTED_WEB);
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
      view->GetRoutingID(), cmd, param));
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

    WebContents* web_contents = tab->GetWebContents();
    if (!web_contents) {
      NOTREACHED();
      return NULL;
    }

    RenderViewHost* view_host = web_contents->GetRenderViewHost();
    return view_host;
  }

  return NULL;
}

void AutomationProvider::SaveAsAsync(int tab_handle) {
  NavigationController* tab = NULL;
  WebContents* web_contents = GetWebContentsForHandle(tab_handle, &tab);
  if (web_contents)
    web_contents->OnSavePage();
}
