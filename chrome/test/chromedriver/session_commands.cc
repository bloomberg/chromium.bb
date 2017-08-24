// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/session_commands.h"

#include <list>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/logging.h"  // For CHECK macros.
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/test/chromedriver/basic_types.h"
#include "chrome/test/chromedriver/capabilities.h"
#include "chrome/test/chromedriver/chrome/automation_extension.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/chrome.h"
#include "chrome/test/chromedriver/chrome/chrome_android_impl.h"
#include "chrome/test/chromedriver/chrome/chrome_desktop_impl.h"
#include "chrome/test/chromedriver/chrome/device_manager.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/geoposition.h"
#include "chrome/test/chromedriver/chrome/javascript_dialog_manager.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/web_view.h"
#include "chrome/test/chromedriver/chrome_launcher.h"
#include "chrome/test/chromedriver/command_listener.h"
#include "chrome/test/chromedriver/logging.h"
#include "chrome/test/chromedriver/net/url_request_context_getter.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/util.h"
#include "chrome/test/chromedriver/version.h"

namespace {

// The minimium chrome build no that supports window management devtools
// commands.
const int kBrowserWindowDevtoolsBuildNo = 3076;

const int kWifiMask = 0x2;
const int k4GMask = 0x8;
const int k3GMask = 0x10;
const int k2GMask = 0x20;

const int kAirplaneModeLatency = 0;
const int kAirplaneModeThroughput = 0;
const int kWifiLatency = 2;
const int kWifiThroughput = 30720 * 1024;
const int k4GLatency = 20;
const int k4GThroughput = 4096 * 1024;
const int k3GLatency = 100;
const int k3GThroughput = 750 * 1024;
const int k2GLatency = 300;
const int k2GThroughput = 250 * 1024;

const char kWindowHandlePrefix[] = "CDwindow-";

std::string WebViewIdToWindowHandle(const std::string& web_view_id) {
  return kWindowHandlePrefix + web_view_id;
}

bool WindowHandleToWebViewId(const std::string& window_handle,
                             std::string* web_view_id) {
  if (!base::StartsWith(window_handle, kWindowHandlePrefix,
                        base::CompareCase::SENSITIVE)) {
    return false;
  }
  *web_view_id = window_handle.substr(sizeof(kWindowHandlePrefix) - 1);
  return true;
}

Status EvaluateScriptAndIgnoreResult(Session* session, std::string expression) {
  WebView* web_view = nullptr;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;
  if (web_view->GetJavaScriptDialogManager()->IsDialogOpen())
    return Status(kUnexpectedAlertOpen);
  std::string frame_id = session->GetCurrentFrameId();
  std::unique_ptr<base::Value> result;
  return web_view->EvaluateScript(frame_id, expression, &result);
}

}  // namespace

InitSessionParams::InitSessionParams(
    scoped_refptr<URLRequestContextGetter> context_getter,
    const SyncWebSocketFactory& socket_factory,
    DeviceManager* device_manager,
    PortServer* port_server,
    PortManager* port_manager)
    : context_getter(context_getter),
      socket_factory(socket_factory),
      device_manager(device_manager),
      port_server(port_server),
      port_manager(port_manager) {}

InitSessionParams::InitSessionParams(const InitSessionParams& other) = default;

InitSessionParams::~InitSessionParams() {}

namespace {

std::unique_ptr<base::DictionaryValue> CreateCapabilities(
    Session* session,
    const Capabilities& capabilities) {
  std::unique_ptr<base::DictionaryValue> caps(new base::DictionaryValue());
  caps->SetString("browserName", "chrome");
  caps->SetString("version",
                  session->chrome->GetBrowserInfo()->browser_version);
  caps->SetString("chrome.chromedriverVersion", kChromeDriverVersion);
  caps->SetString("platform", session->chrome->GetOperatingSystemName());
  caps->SetString("pageLoadStrategy", session->chrome->page_load_strategy());
  caps->SetBoolean("javascriptEnabled", true);
  caps->SetBoolean("takesScreenshot", true);
  caps->SetBoolean("takesHeapSnapshot", true);
  caps->SetBoolean("handlesAlerts", true);
  caps->SetBoolean("databaseEnabled", false);
  caps->SetBoolean("locationContextEnabled", true);
  caps->SetBoolean("mobileEmulationEnabled",
                   session->chrome->IsMobileEmulationEnabled());
  caps->SetBoolean("applicationCacheEnabled", false);
  caps->SetBoolean("browserConnectionEnabled", false);
  caps->SetBoolean("cssSelectorsEnabled", true);
  caps->SetBoolean("webStorageEnabled", true);
  caps->SetBoolean("rotatable", false);
  caps->SetBoolean("acceptSslCerts", true);
  caps->SetBoolean("nativeEvents", true);
  caps->SetBoolean("hasTouchScreen", session->chrome->HasTouchScreen());
  caps->SetString("unexpectedAlertBehaviour",
                  session->unexpected_alert_behaviour);

  // add setWindowRect based on whether we are desktop/android/remote
  if (capabilities.IsAndroid() || capabilities.IsRemoteBrowser()) {
    caps->SetBoolean("setWindowRect", false);
  } else {
    caps->SetBoolean("setWindowRect", true);
  }

  ChromeDesktopImpl* desktop = NULL;
  Status status = session->chrome->GetAsDesktop(&desktop);
  if (status.IsOk()) {
    caps->SetString("chrome.userDataDir",
                    desktop->command().GetSwitchValueNative("user-data-dir"));
    caps->SetBoolean("networkConnectionEnabled",
                     desktop->IsNetworkConnectionEnabled());
  }

  return caps;
}

Status CheckSessionCreated(Session* session) {
  WebView* web_view = NULL;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return Status(kSessionNotCreatedException, status);

  status = web_view->ConnectIfNecessary();
  if (status.IsError())
    return Status(kSessionNotCreatedException, status);

  base::ListValue args;
  std::unique_ptr<base::Value> result(new base::Value(0));
  status = web_view->CallFunction(session->GetCurrentFrameId(),
                                  "function(s) { return 1; }", args, &result);
  if (status.IsError())
    return Status(kSessionNotCreatedException, status);

  int response;
  if (!result->GetAsInteger(&response) || response != 1) {
    return Status(kSessionNotCreatedException,
                  "unexpected response from browser");
  }

  return Status(kOk);
}

Status InitSessionHelper(const InitSessionParams& bound_params,
                         Session* session,
                         const base::DictionaryValue& params,
                         std::unique_ptr<base::Value>* value) {
  session->driver_log.reset(
      new WebDriverLog(WebDriverLog::kDriverType, Log::kAll));
  const base::DictionaryValue* desired_caps;
  base::DictionaryValue merged_caps;

  bool w3c_capability = false;
  if (params.GetDictionary("capabilities.alwaysMatch", &desired_caps) &&
      (desired_caps->GetBoolean("goog:chromeOptions.w3c", &w3c_capability) ||
       desired_caps->GetBoolean("chromeOptions.w3c", &w3c_capability)) &&
      w3c_capability) {
    session->w3c_compliant = true;
    // TODO(johnchen): Handle capabilities.firstMatch. Currently, we're just
    // merging, not validating or matching as per the spec.
    const base::ListValue* first_match_list;
    std::unique_ptr<base::ListValue> tmp_list;
    if (!(params.GetList("capabilities.firstMatch", &first_match_list))) {
      // if no firstMatch, make first_match_list a list with an empty dictionary
      tmp_list = std::unique_ptr<base::ListValue>(new base::ListValue());
      std::unique_ptr<base::DictionaryValue> inner(new base::DictionaryValue());
      tmp_list->Append(std::move(inner));
      first_match_list = tmp_list.get();
    }
    for (size_t i = 0; i < first_match_list->GetSize(); ++i) {
      const base::DictionaryValue* first_match;
      if (!first_match_list->GetDictionary(i, &first_match)) {
        continue;
      }
      if (!MergeCapabilities(desired_caps, first_match, &merged_caps)) {
        return Status(kSessionNotCreatedException, "Invalid capabilities");
      }
      if (MatchCapabilities(&merged_caps)) {
        // If a match is found, we want to use these matched setcapabilities.
        desired_caps = &merged_caps;
        break;
      }
    }
  } else if (params.GetDictionary("capabilities.desiredCapabilities",
                                  &desired_caps) &&
             (desired_caps->GetBoolean("goog:chromeOptions.w3c",
                                       &w3c_capability) ||
              desired_caps->GetBoolean("chromeOptions.w3c", &w3c_capability)) &&
             w3c_capability) {
    // TODO(johnchen): Remove when clients stop using this.
    session->w3c_compliant = true;
  } else if (!params.GetDictionary("desiredCapabilities", &desired_caps)) {
    return Status(kSessionNotCreatedException,
                  "Missing or invalid capabilities");
  }

  Capabilities capabilities;
  Status status = capabilities.Parse(*desired_caps);
  if (status.IsError())
    return status;

  desired_caps->GetString("unexpectedAlertBehaviour",
                           &session->unexpected_alert_behaviour);

  Log::Level driver_level = Log::kWarning;
  if (capabilities.logging_prefs.count(WebDriverLog::kDriverType))
    driver_level = capabilities.logging_prefs[WebDriverLog::kDriverType];
  session->driver_log->set_min_level(driver_level);

  // Create Log's and DevToolsEventListener's for ones that are DevTools-based.
  // Session will own the Log's, Chrome will own the listeners.
  // Also create |CommandListener|s for the appropriate logs.
  std::vector<std::unique_ptr<DevToolsEventListener>> devtools_event_listeners;
  std::vector<std::unique_ptr<CommandListener>> command_listeners;
  status = CreateLogs(capabilities,
                      session,
                      &session->devtools_logs,
                      &devtools_event_listeners,
                      &command_listeners);
  if (status.IsError())
    return status;

  // |session| will own the |CommandListener|s.
  session->command_listeners.swap(command_listeners);

  status =
      LaunchChrome(bound_params.context_getter.get(),
                   bound_params.socket_factory, bound_params.device_manager,
                   bound_params.port_server, bound_params.port_manager,
                   capabilities, std::move(devtools_event_listeners),
                   &session->chrome, session->w3c_compliant);
  if (status.IsError())
    return status;

  status = session->chrome->GetWebViewIdForFirstTab(&session->window,
                                                    session->w3c_compliant);
  if (status.IsError())
    return status;
  session->detach = capabilities.detach;
  session->force_devtools_screenshot = capabilities.force_devtools_screenshot;
  session->capabilities = CreateCapabilities(session, capabilities);

  if (w3c_capability) {
    base::DictionaryValue body;
    body.SetDictionary("capabilities", std::move(session->capabilities));
    body.SetString("sessionId", session->id);
    value->reset(body.DeepCopy());
  } else {
    value->reset(session->capabilities->DeepCopy());
  }
  return CheckSessionCreated(session);
}

}  // namespace

bool MergeCapabilities(const base::DictionaryValue* always_match,
                       const base::DictionaryValue* first_match,
                       base::DictionaryValue* merged) {
  CHECK(always_match);
  CHECK(first_match);
  CHECK(merged);
  merged->Clear();

  for (base::DictionaryValue::Iterator it(*first_match); !it.IsAtEnd();
       it.Advance()) {
    if (always_match->HasKey(it.key())) {
      // firstMatch cannot have the same |keys| as alwaysMatch.
      return false;
    }
  }

  // merge the capabilities together since guarenteed no key collisions
  merged->MergeDictionary(always_match);
  merged->MergeDictionary(first_match);
  return true;
}

bool MatchCapabilities(base::DictionaryValue* capabilities) {
  // attempt to match the capabilities requested to the actual capabilities
  // reject if they don't match
  if (capabilities->HasKey("browserName")) {
    std::string name;
    capabilities->GetString("browserName", &name);
    if (name != "chrome") {
      return false;
    }
  }
  return true;
}

Status ExecuteInitSession(const InitSessionParams& bound_params,
                          Session* session,
                          const base::DictionaryValue& params,
                          std::unique_ptr<base::Value>* value) {
  Status status = InitSessionHelper(bound_params, session, params, value);
  if (status.IsError()) {
    session->quit = true;
    if (session->chrome != NULL)
      session->chrome->Quit();
  }
  return status;
}

Status ExecuteQuit(bool allow_detach,
                   Session* session,
                   const base::DictionaryValue& params,
                   std::unique_ptr<base::Value>* value) {
  session->quit = true;
  if (allow_detach && session->detach)
    return Status(kOk);
  else
    return session->chrome->Quit();
}

Status ExecuteGetSessionCapabilities(Session* session,
                                     const base::DictionaryValue& params,
                                     std::unique_ptr<base::Value>* value) {
  value->reset(session->capabilities->DeepCopy());
  return Status(kOk);
}

Status ExecuteGetCurrentWindowHandle(Session* session,
                                     const base::DictionaryValue& params,
                                     std::unique_ptr<base::Value>* value) {
  WebView* web_view = NULL;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;

  value->reset(new base::Value(WebViewIdToWindowHandle(web_view->GetId())));
  return Status(kOk);
}

Status ExecuteLaunchApp(Session* session,
                        const base::DictionaryValue& params,
                        std::unique_ptr<base::Value>* value) {
  std::string id;
  if (!params.GetString("id", &id))
    return Status(kUnknownError, "'id' must be a string");

  ChromeDesktopImpl* desktop = NULL;
  Status status = session->chrome->GetAsDesktop(&desktop);
  if (status.IsError())
    return status;

  AutomationExtension* extension = NULL;
  status = desktop->GetAutomationExtension(&extension, session->w3c_compliant);
  if (status.IsError())
    return status;

  return extension->LaunchApp(id);
}

Status ExecuteClose(Session* session,
                    const base::DictionaryValue& params,
                    std::unique_ptr<base::Value>* value) {
  std::list<std::string> web_view_ids;
  Status status = session->chrome->GetWebViewIds(&web_view_ids,
                                                 session->w3c_compliant);
  if (status.IsError())
    return status;
  bool is_last_web_view = web_view_ids.size() == 1u;
  web_view_ids.clear();

  WebView* web_view = NULL;
  status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;

  status = session->chrome->CloseWebView(web_view->GetId());
  if (status.IsError())
    return status;

  status = session->chrome->GetWebViewIds(&web_view_ids,
                                          session->w3c_compliant);
  if ((status.code() == kChromeNotReachable && is_last_web_view) ||
      (status.IsOk() && web_view_ids.empty())) {
    // If no window is open, close is the equivalent of calling "quit".
    session->quit = true;
    return session->chrome->Quit();
  }

  return status;
}

Status ExecuteGetWindowHandles(Session* session,
                               const base::DictionaryValue& params,
                               std::unique_ptr<base::Value>* value) {
  std::list<std::string> web_view_ids;
  Status status = session->chrome->GetWebViewIds(&web_view_ids,
                                                 session->w3c_compliant);
  if (status.IsError())
    return status;
  std::unique_ptr<base::ListValue> window_ids(new base::ListValue());
  for (std::list<std::string>::const_iterator it = web_view_ids.begin();
       it != web_view_ids.end(); ++it) {
    window_ids->AppendString(WebViewIdToWindowHandle(*it));
  }
  *value = std::move(window_ids);
  return Status(kOk);
}

Status ExecuteSwitchToWindow(Session* session,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value) {
  std::string name;
  if (!params.GetString("name", &name))
    return Status(kUnknownError, "'name' must be a string");

  std::list<std::string> web_view_ids;
  Status status = session->chrome->GetWebViewIds(&web_view_ids,
                                                 session->w3c_compliant);
  if (status.IsError())
    return status;

  std::string web_view_id;
  bool found = false;
  if (WindowHandleToWebViewId(name, &web_view_id)) {
    // Check if any web_view matches |web_view_id|.
    for (std::list<std::string>::const_iterator it = web_view_ids.begin();
         it != web_view_ids.end(); ++it) {
      if (*it == web_view_id) {
        found = true;
        break;
      }
    }
  } else {
    // Check if any of the tab window names match |name|.
    const char* kGetWindowNameScript = "function() { return window.name; }";
    base::ListValue args;
    for (std::list<std::string>::const_iterator it = web_view_ids.begin();
         it != web_view_ids.end(); ++it) {
      std::unique_ptr<base::Value> result;
      WebView* web_view;
      status = session->chrome->GetWebViewById(*it, &web_view);
      if (status.IsError())
        return status;
      status = web_view->ConnectIfNecessary();
      if (status.IsError())
        return status;
      status = web_view->CallFunction(
          std::string(), kGetWindowNameScript, args, &result);
      if (status.IsError())
        return status;
      std::string window_name;
      if (!result->GetAsString(&window_name))
        return Status(kUnknownError, "failed to get window name");
      if (window_name == name) {
        web_view_id = *it;
        found = true;
        break;
      }
    }
  }

  if (!found)
    return Status(kNoSuchWindow);

  if (session->overridden_geoposition) {
    WebView* web_view;
    Status status = session->chrome->GetWebViewById(web_view_id, &web_view);
    if (status.IsError())
      return status;
    status = web_view->ConnectIfNecessary();
    if (status.IsError())
      return status;
    status = web_view->OverrideGeolocation(*session->overridden_geoposition);
    if (status.IsError())
      return status;
  }

  if (session->overridden_network_conditions) {
    WebView* web_view;
    Status status = session->chrome->GetWebViewById(web_view_id, &web_view);
    if (status.IsError())
      return status;
    status = web_view->ConnectIfNecessary();
    if (status.IsError())
      return status;
    status = web_view->OverrideNetworkConditions(
        *session->overridden_network_conditions);
    if (status.IsError())
      return status;
  }

  status = session->chrome->ActivateWebView(web_view_id);
  if (status.IsError())
    return status;
  session->window = web_view_id;
  session->SwitchToTopFrame();
  session->mouse_position = WebPoint(0, 0);
  return Status(kOk);
}

Status ExecuteSetTimeout(Session* session,
                         const base::DictionaryValue& params,
                         std::unique_ptr<base::Value>* value) {
  double ms_double;
  if (!params.GetDouble("ms", &ms_double))
    return Status(kUnknownError, "'ms' must be a double");
  std::string type;
  if (!params.GetString("type", &type))
    return Status(kUnknownError, "'type' must be a string");

  base::TimeDelta timeout =
      base::TimeDelta::FromMilliseconds(static_cast<int>(ms_double));
  // TODO(frankf): implicit and script timeout should be cleared
  // if negative timeout is specified.
  if (type == "implicit") {
    session->implicit_wait = timeout;
  } else if (type == "script") {
    session->script_timeout = timeout;
  } else if (type == "page load") {
    session->page_load_timeout =
        ((timeout < base::TimeDelta()) ? Session::kDefaultPageLoadTimeout
                                       : timeout);
  } else {
    return Status(kUnknownError, "unknown type of timeout:" + type);
  }
  return Status(kOk);
}

Status ExecuteGetTimeouts(Session* session,
                          const base::DictionaryValue& params,
                          std::unique_ptr<base::Value>* value) {
  base::DictionaryValue timeouts;
  timeouts.SetInteger("script", session->script_timeout.InMilliseconds());
  timeouts.SetInteger("pageLoad", session->page_load_timeout.InMilliseconds());
  timeouts.SetInteger("implicit", session->implicit_wait.InMilliseconds());

  value->reset(timeouts.DeepCopy());
  return Status(kOk);
}

Status ExecuteSetScriptTimeout(Session* session,
                               const base::DictionaryValue& params,
                               std::unique_ptr<base::Value>* value) {
  double ms;
  if (!params.GetDouble("ms", &ms) || ms < 0)
    return Status(kUnknownError, "'ms' must be a non-negative number");
  session->script_timeout =
      base::TimeDelta::FromMilliseconds(static_cast<int>(ms));
  return Status(kOk);
}

Status ExecuteImplicitlyWait(Session* session,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value) {
  double ms;
  if (!params.GetDouble("ms", &ms) || ms < 0)
    return Status(kUnknownError, "'ms' must be a non-negative number");
  session->implicit_wait =
      base::TimeDelta::FromMilliseconds(static_cast<int>(ms));
  return Status(kOk);
}

Status ExecuteIsLoading(Session* session,
                        const base::DictionaryValue& params,
                        std::unique_ptr<base::Value>* value) {
  WebView* web_view = NULL;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;

  status = web_view->ConnectIfNecessary();
  if (status.IsError())
    return status;

  bool is_pending;
  status = web_view->IsPendingNavigation(
      session->GetCurrentFrameId(), nullptr, &is_pending);
  if (status.IsError())
    return status;
  value->reset(new base::Value(is_pending));
  return Status(kOk);
}

Status ExecuteGetLocation(Session* session,
                          const base::DictionaryValue& params,
                          std::unique_ptr<base::Value>* value) {
  if (!session->overridden_geoposition) {
    return Status(kUnknownError,
                  "Location must be set before it can be retrieved");
  }
  base::DictionaryValue location;
  location.SetDouble("latitude", session->overridden_geoposition->latitude);
  location.SetDouble("longitude", session->overridden_geoposition->longitude);
  location.SetDouble("accuracy", session->overridden_geoposition->accuracy);
  // Set a dummy altitude to make WebDriver clients happy.
  // https://code.google.com/p/chromedriver/issues/detail?id=281
  location.SetDouble("altitude", 0);
  value->reset(location.DeepCopy());
  return Status(kOk);
}

Status ExecuteGetNetworkConnection(Session* session,
                                   const base::DictionaryValue& params,
                                   std::unique_ptr<base::Value>* value) {
  ChromeDesktopImpl* desktop = nullptr;
  Status status = session->chrome->GetAsDesktop(&desktop);
  if (status.IsError())
    return status;
  if (!desktop->IsNetworkConnectionEnabled())
    return Status(kUnknownError, "network connection must be enabled");

  int connection_type = 0;
  connection_type = desktop->GetNetworkConnection();

  value->reset(new base::Value(connection_type));
  return Status(kOk);
}

Status ExecuteGetNetworkConditions(Session* session,
                                   const base::DictionaryValue& params,
                                   std::unique_ptr<base::Value>* value) {
  if (!session->overridden_network_conditions) {
    return Status(kUnknownError,
                  "network conditions must be set before it can be retrieved");
  }
  base::DictionaryValue conditions;
  conditions.SetBoolean("offline",
                        session->overridden_network_conditions->offline);
  conditions.SetInteger("latency",
                        session->overridden_network_conditions->latency);
  conditions.SetInteger(
      "download_throughput",
      session->overridden_network_conditions->download_throughput);
  conditions.SetInteger(
      "upload_throughput",
      session->overridden_network_conditions->upload_throughput);
  value->reset(conditions.DeepCopy());
  return Status(kOk);
}

Status ExecuteSetNetworkConnection(Session* session,
                                   const base::DictionaryValue& params,
                                   std::unique_ptr<base::Value>* value) {
  ChromeDesktopImpl* desktop = nullptr;
  Status status = session->chrome->GetAsDesktop(&desktop);
  if (status.IsError())
    return status;
  if (!desktop->IsNetworkConnectionEnabled())
    return Status(kUnknownError, "network connection must be enabled");

  int connection_type;
  if (!params.GetInteger("parameters.type", &connection_type))
    return Status(kUnknownError, "invalid connection_type");

  desktop->SetNetworkConnection(connection_type);

  std::unique_ptr<NetworkConditions> network_conditions(
      new NetworkConditions());

  if (connection_type & kWifiMask) {
    network_conditions->latency = kWifiLatency;
    network_conditions->upload_throughput = kWifiThroughput;
    network_conditions->download_throughput = kWifiThroughput;
    network_conditions->offline = false;
  } else if (connection_type & k4GMask) {
    network_conditions->latency = k4GLatency;
    network_conditions->upload_throughput = k4GThroughput;
    network_conditions->download_throughput = k4GThroughput;
    network_conditions->offline = false;
  } else if (connection_type & k3GMask) {
    network_conditions->latency = k3GLatency;
    network_conditions->upload_throughput = k3GThroughput;
    network_conditions->download_throughput = k3GThroughput;
    network_conditions->offline = false;
  } else if (connection_type & k2GMask) {
    network_conditions->latency = k2GLatency;
    network_conditions->upload_throughput = k2GThroughput;
    network_conditions->download_throughput = k2GThroughput;
    network_conditions->offline = false;
  } else {
    network_conditions->latency = kAirplaneModeLatency;
    network_conditions->upload_throughput = kAirplaneModeThroughput;
    network_conditions->download_throughput = kAirplaneModeThroughput;
    network_conditions->offline = true;
  }

  session->overridden_network_conditions.reset(
      network_conditions.release());

  // Applies overridden network connection to all WebViews of the session
  // to ensure that network emulation is applied on a per-session basis
  // rather than the just to the current WebView.
  std::list<std::string> web_view_ids;
  status = session->chrome->GetWebViewIds(&web_view_ids,
                                          session->w3c_compliant);
  if (status.IsError())
    return status;

  for (std::string web_view_id : web_view_ids) {
    WebView* web_view;
    status = session->chrome->GetWebViewById(web_view_id, &web_view);
    if (status.IsError())
      return status;
    web_view->OverrideNetworkConditions(
      *session->overridden_network_conditions);
  }

  value->reset(new base::Value(connection_type));
  return Status(kOk);
}

Status ExecuteGetWindowPosition(Session* session,
                                const base::DictionaryValue& params,
                                std::unique_ptr<base::Value>* value) {
  ChromeDesktopImpl* desktop = NULL;
  Status status = session->chrome->GetAsDesktop(&desktop);
  if (status.IsError())
    return status;

  int x, y;

  if (desktop->GetBrowserInfo()->build_no >= kBrowserWindowDevtoolsBuildNo) {
    status = desktop->GetWindowPosition(session->window, &x, &y);
  } else {
    AutomationExtension* extension = NULL;
    status =
        desktop->GetAutomationExtension(&extension, session->w3c_compliant);
    if (status.IsError())
      return status;

    status = extension->GetWindowPosition(&x, &y);
  }
  if (status.IsError())
    return status;

  base::DictionaryValue position;
  position.SetInteger("x", x);
  position.SetInteger("y", y);
  value->reset(position.DeepCopy());
  return Status(kOk);
}

Status ExecuteSetWindowPosition(Session* session,
                                const base::DictionaryValue& params,
                                std::unique_ptr<base::Value>* value) {
  double x = 0;
  double y = 0;
  if (!params.GetDouble("x", &x) || !params.GetDouble("y", &y))
    return Status(kUnknownError, "missing or invalid 'x' or 'y'");

  ChromeDesktopImpl* desktop = NULL;
  Status status = session->chrome->GetAsDesktop(&desktop);
  if (status.IsError())
    return status;

  if (desktop->GetBrowserInfo()->build_no >= kBrowserWindowDevtoolsBuildNo) {
    return desktop->SetWindowPosition(session->window, static_cast<int>(x),
                                      static_cast<int>(y));
  }

  AutomationExtension* extension = NULL;
  status = desktop->GetAutomationExtension(&extension, session->w3c_compliant);
  if (status.IsError())
    return status;

  return extension->SetWindowPosition(static_cast<int>(x), static_cast<int>(y));
}

Status ExecuteGetWindowSize(Session* session,
                            const base::DictionaryValue& params,
                            std::unique_ptr<base::Value>* value) {
  ChromeDesktopImpl* desktop = NULL;
  Status status = session->chrome->GetAsDesktop(&desktop);
  if (status.IsError())
    return status;

  int width, height;

  if (desktop->GetBrowserInfo()->build_no >= kBrowserWindowDevtoolsBuildNo) {
    status = desktop->GetWindowSize(session->window, &width, &height);
  } else {
    AutomationExtension* extension = NULL;
    status =
        desktop->GetAutomationExtension(&extension, session->w3c_compliant);
    if (status.IsError())
      return status;

    status = extension->GetWindowSize(&width, &height);
  }
  if (status.IsError())
    return status;

  base::DictionaryValue size;
  size.SetInteger("width", width);
  size.SetInteger("height", height);
  value->reset(size.DeepCopy());
  return Status(kOk);
}

Status ExecuteSetWindowSize(Session* session,
                            const base::DictionaryValue& params,
                            std::unique_ptr<base::Value>* value) {
  double width = 0;
  double height = 0;
  if (!params.GetDouble("width", &width) ||
      !params.GetDouble("height", &height))
    return Status(kUnknownError, "missing or invalid 'width' or 'height'");

  ChromeDesktopImpl* desktop = NULL;
  Status status = session->chrome->GetAsDesktop(&desktop);
  if (status.IsError())
    return status;

  if (desktop->GetBrowserInfo()->build_no >= kBrowserWindowDevtoolsBuildNo) {
    return desktop->SetWindowSize(session->window, static_cast<int>(width),
                                  static_cast<int>(height));
  }

  AutomationExtension* extension = NULL;
  status = desktop->GetAutomationExtension(&extension, session->w3c_compliant);
  if (status.IsError())
    return status;

  return extension->SetWindowSize(
      static_cast<int>(width), static_cast<int>(height));
}

Status ExecuteMaximizeWindow(Session* session,
                             const base::DictionaryValue& params,
                             std::unique_ptr<base::Value>* value) {
  ChromeDesktopImpl* desktop = NULL;
  Status status = session->chrome->GetAsDesktop(&desktop);
  if (status.IsError())
    return status;

  if (desktop->GetBrowserInfo()->build_no >= kBrowserWindowDevtoolsBuildNo)
    return desktop->MaximizeWindow(session->window);

  AutomationExtension* extension = NULL;
  status = desktop->GetAutomationExtension(&extension, session->w3c_compliant);
  if (status.IsError())
    return status;

  return extension->MaximizeWindow();
}

Status ExecuteFullScreenWindow(Session* session,
                               const base::DictionaryValue& params,
                               std::unique_ptr<base::Value>* value) {
  ChromeDesktopImpl* desktop = NULL;
  Status status = session->chrome->GetAsDesktop(&desktop);
  if (status.IsError())
    return status;

  if (desktop->GetBrowserInfo()->build_no >= kBrowserWindowDevtoolsBuildNo)
    return desktop->FullScreenWindow(session->window);

  AutomationExtension* extension = NULL;
  status = desktop->GetAutomationExtension(&extension, session->w3c_compliant);
  if (status.IsError())
    return status;

  return extension->FullScreenWindow();
}

Status ExecuteGetAvailableLogTypes(Session* session,
                                   const base::DictionaryValue& params,
                                   std::unique_ptr<base::Value>* value) {
  std::unique_ptr<base::ListValue> types(new base::ListValue());
  std::vector<WebDriverLog*> logs = session->GetAllLogs();
  for (std::vector<WebDriverLog*>::const_iterator log = logs.begin();
       log != logs.end();
       ++log) {
    types->AppendString((*log)->type());
  }
  *value = std::move(types);
  return Status(kOk);
}

Status ExecuteGetLog(Session* session,
                     const base::DictionaryValue& params,
                     std::unique_ptr<base::Value>* value) {
  std::string log_type;
  if (!params.GetString("type", &log_type)) {
    return Status(kUnknownError, "missing or invalid 'type'");
  }

  // Evaluate a JavaScript in the renderer process for the current tab, to flush
  // out any pending logging-related events.
  Status status = EvaluateScriptAndIgnoreResult(session, "1");
  if (status.IsError()) {
    // Sometimes a WebDriver client fetches logs to diagnose an error that has
    // occurred. It's possible that in the case of an error, the renderer is no
    // be longer available, but we should return the logs anyway. So log (but
    // don't fail on) any error that we get while evaluating the script.
    LOG(WARNING) << "Unable to evaluate script: " << status.message();
  }

  std::vector<WebDriverLog*> logs = session->GetAllLogs();
  for (std::vector<WebDriverLog*>::const_iterator log = logs.begin();
       log != logs.end();
       ++log) {
    if (log_type == (*log)->type()) {
      *value = (*log)->GetAndClearEntries();
      return Status(kOk);
    }
  }
  return Status(kUnknownError, "log type '" + log_type + "' not found");
}

Status ExecuteUploadFile(Session* session,
                         const base::DictionaryValue& params,
                         std::unique_ptr<base::Value>* value) {
  std::string base64_zip_data;
  if (!params.GetString("file", &base64_zip_data))
    return Status(kUnknownError, "missing or invalid 'file'");
  std::string zip_data;
  if (!Base64Decode(base64_zip_data, &zip_data))
    return Status(kUnknownError, "unable to decode 'file'");

  if (!session->temp_dir.IsValid()) {
    if (!session->temp_dir.CreateUniqueTempDir())
      return Status(kUnknownError, "unable to create temp dir");
  }
  base::FilePath upload_dir;
  if (!base::CreateTemporaryDirInDir(session->temp_dir.GetPath(),
                                     FILE_PATH_LITERAL("upload"),
                                     &upload_dir)) {
    return Status(kUnknownError, "unable to create temp dir");
  }
  std::string error_msg;
  base::FilePath upload;
  Status status = UnzipSoleFile(upload_dir, zip_data, &upload);
  if (status.IsError())
    return Status(kUnknownError, "unable to unzip 'file'", status);

  value->reset(new base::Value(upload.value()));
  return Status(kOk);
}

Status ExecuteIsAutoReporting(Session* session,
                              const base::DictionaryValue& params,
                              std::unique_ptr<base::Value>* value) {
  value->reset(new base::Value(session->auto_reporting_enabled));
  return Status(kOk);
}

Status ExecuteSetAutoReporting(Session* session,
                               const base::DictionaryValue& params,
                               std::unique_ptr<base::Value>* value) {
  bool enabled;
  if (!params.GetBoolean("enabled", &enabled))
    return Status(kUnknownError, "missing parameter 'enabled'");
  session->auto_reporting_enabled = enabled;
  return Status(kOk);
}

Status ExecuteUnimplementedCommand(Session* session,
                                   const base::DictionaryValue& params,
                                   std::unique_ptr<base::Value>* value) {
  return Status(kUnknownCommand);
}

Status ExecuteGetScreenOrientation(Session* session,
                                   const base::DictionaryValue& params,
                                   std::unique_ptr<base::Value>* value) {
  WebView* web_view = nullptr;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;

  std::string screen_orientation;
  status = web_view->GetScreenOrientation(&screen_orientation);
  if (status.IsError())
    return status;

  base::DictionaryValue orientation_value;
  orientation_value.SetString("orientation", screen_orientation);
  value->reset(orientation_value.DeepCopy());
  return Status(kOk);
}

Status ExecuteSetScreenOrientation(Session* session,
                                   const base::DictionaryValue& params,
                                   std::unique_ptr<base::Value>* value) {
  WebView* web_view = nullptr;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;

  std::string screen_orientation;
  params.GetString("parameters.orientation", &screen_orientation);
  status = web_view->SetScreenOrientation(screen_orientation);
  if (status.IsError())
    return status;
  return Status(kOk);
}

Status ExecuteDeleteScreenOrientation(Session* session,
                                      const base::DictionaryValue& params,
                                      std::unique_ptr<base::Value>* value) {
  WebView* web_view = nullptr;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;
  status = web_view->DeleteScreenOrientation();
  if (status.IsError())
    return status;
  return Status(kOk);
}
