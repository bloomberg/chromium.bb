// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/inspect_ui.h"

#include "base/prefs/pref_service.h"
#include "base/stl_util.h"
#include "chrome/browser/devtools/devtools_target_impl.h"
#include "chrome/browser/devtools/devtools_targets_ui.h"
#include "chrome/browser/devtools/devtools_ui_bindings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace {

const char kInitUICommand[]  = "init-ui";
const char kInspectCommand[]  = "inspect";
const char kActivateCommand[]  = "activate";
const char kCloseCommand[]  = "close";
const char kReloadCommand[]  = "reload";
const char kOpenCommand[]  = "open";
const char kInspectBrowser[] = "inspect-browser";
const char kLocalHost[] = "localhost";

const char kDiscoverUsbDevicesEnabledCommand[] =
    "set-discover-usb-devices-enabled";
const char kPortForwardingEnabledCommand[] =
    "set-port-forwarding-enabled";
const char kPortForwardingConfigCommand[] = "set-port-forwarding-config";

const char kPortForwardingDefaultPort[] = "8080";
const char kPortForwardingDefaultLocation[] = "localhost:8080";

class InspectMessageHandler : public WebUIMessageHandler {
 public:
  explicit InspectMessageHandler(InspectUI* inspect_ui)
      : inspect_ui_(inspect_ui) {}
  virtual ~InspectMessageHandler() {}

 private:
  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  void HandleInitUICommand(const base::ListValue* args);
  void HandleInspectCommand(const base::ListValue* args);
  void HandleActivateCommand(const base::ListValue* args);
  void HandleCloseCommand(const base::ListValue* args);
  void HandleReloadCommand(const base::ListValue* args);
  void HandleOpenCommand(const base::ListValue* args);
  void HandleInspectBrowserCommand(const base::ListValue* args);
  void HandleBooleanPrefChanged(const char* pref_name,
                                const base::ListValue* args);
  void HandlePortForwardingConfigCommand(const base::ListValue* args);

  InspectUI* inspect_ui_;

  DISALLOW_COPY_AND_ASSIGN(InspectMessageHandler);
};

void InspectMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(kInitUICommand,
      base::Bind(&InspectMessageHandler::HandleInitUICommand,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kInspectCommand,
      base::Bind(&InspectMessageHandler::HandleInspectCommand,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kActivateCommand,
      base::Bind(&InspectMessageHandler::HandleActivateCommand,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kCloseCommand,
      base::Bind(&InspectMessageHandler::HandleCloseCommand,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kDiscoverUsbDevicesEnabledCommand,
      base::Bind(&InspectMessageHandler::HandleBooleanPrefChanged,
                  base::Unretained(this),
                  &prefs::kDevToolsDiscoverUsbDevicesEnabled[0]));
  web_ui()->RegisterMessageCallback(kPortForwardingEnabledCommand,
      base::Bind(&InspectMessageHandler::HandleBooleanPrefChanged,
                 base::Unretained(this),
                 &prefs::kDevToolsPortForwardingEnabled[0]));
  web_ui()->RegisterMessageCallback(kPortForwardingConfigCommand,
      base::Bind(&InspectMessageHandler::HandlePortForwardingConfigCommand,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kReloadCommand,
      base::Bind(&InspectMessageHandler::HandleReloadCommand,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kOpenCommand,
      base::Bind(&InspectMessageHandler::HandleOpenCommand,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kInspectBrowser,
      base::Bind(&InspectMessageHandler::HandleInspectBrowserCommand,
                 base::Unretained(this)));
}

void InspectMessageHandler::HandleInitUICommand(const base::ListValue*) {
  inspect_ui_->InitUI();
}

static bool ParseStringArgs(const base::ListValue* args,
                            std::string* arg0,
                            std::string* arg1,
                            std::string* arg2 = 0) {
  int arg_size = args->GetSize();
  return (!arg0 || (arg_size > 0 && args->GetString(0, arg0))) &&
         (!arg1 || (arg_size > 1 && args->GetString(1, arg1))) &&
         (!arg2 || (arg_size > 2 && args->GetString(2, arg2)));
}

void InspectMessageHandler::HandleInspectCommand(const base::ListValue* args) {
  std::string source;
  std::string id;
  if (ParseStringArgs(args, &source, &id))
    inspect_ui_->Inspect(source, id);
}

void InspectMessageHandler::HandleActivateCommand(const base::ListValue* args) {
  std::string source;
  std::string id;
  if (ParseStringArgs(args, &source, &id))
    inspect_ui_->Activate(source, id);
}

void InspectMessageHandler::HandleCloseCommand(const base::ListValue* args) {
  std::string source;
  std::string id;
  if (ParseStringArgs(args, &source, &id))
    inspect_ui_->Close(source, id);
}

void InspectMessageHandler::HandleReloadCommand(const base::ListValue* args) {
  std::string source;
  std::string id;
  if (ParseStringArgs(args, &source, &id))
    inspect_ui_->Reload(source, id);
}

void InspectMessageHandler::HandleOpenCommand(const base::ListValue* args) {
  std::string source_id;
  std::string browser_id;
  std::string url;
  if (ParseStringArgs(args, &source_id, &browser_id, &url))
    inspect_ui_->Open(source_id, browser_id, url);
}

void InspectMessageHandler::HandleInspectBrowserCommand(
    const base::ListValue* args) {
  std::string source_id;
  std::string browser_id;
  std::string front_end;
  if (ParseStringArgs(args, &source_id, &browser_id, &front_end)) {
    inspect_ui_->InspectBrowserWithCustomFrontend(
        source_id, browser_id, GURL(front_end));
  }
}

void InspectMessageHandler::HandleBooleanPrefChanged(
    const char* pref_name,
    const base::ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!profile)
    return;

  bool enabled;
  if (args->GetSize() == 1 && args->GetBoolean(0, &enabled))
    profile->GetPrefs()->SetBoolean(pref_name, enabled);
}

void InspectMessageHandler::HandlePortForwardingConfigCommand(
    const base::ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (!profile)
    return;

  const base::DictionaryValue* dict_src;
  if (args->GetSize() == 1 && args->GetDictionary(0, &dict_src))
    profile->GetPrefs()->Set(prefs::kDevToolsPortForwardingConfig, *dict_src);
}

}  // namespace

InspectUI::InspectUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new InspectMessageHandler(this));
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateInspectUIHTMLSource());

  // Set up the chrome://theme/ source.
  ThemeSource* theme = new ThemeSource(profile);
  content::URLDataSource::Add(profile, theme);
}

InspectUI::~InspectUI() {
  StopListeningNotifications();
}

void InspectUI::InitUI() {
  SetPortForwardingDefaults();
  StartListeningNotifications();
  UpdateDiscoverUsbDevicesEnabled();
  UpdatePortForwardingEnabled();
  UpdatePortForwardingConfig();
}

void InspectUI::Inspect(const std::string& source_id,
                        const std::string& target_id) {
  DevToolsTargetImpl* target = FindTarget(source_id, target_id);
  if (target)
    target->Inspect(Profile::FromWebUI(web_ui()));
}

void InspectUI::Activate(const std::string& source_id,
                         const std::string& target_id) {
  DevToolsTargetImpl* target = FindTarget(source_id, target_id);
  if (target)
    target->Activate();
}

void InspectUI::Close(const std::string& source_id,
                      const std::string& target_id) {
  DevToolsTargetImpl* target = FindTarget(source_id, target_id);
  if (target)
    target->Close();
}

void InspectUI::Reload(const std::string& source_id,
                       const std::string& target_id) {
  DevToolsTargetImpl* target = FindTarget(source_id, target_id);
  if (target)
    target->Reload();
}

static void NoOp(DevToolsTargetImpl*) {}

void InspectUI::Open(const std::string& source_id,
                     const std::string& browser_id,
                     const std::string& url) {
  DevToolsTargetsUIHandler* handler = FindTargetHandler(source_id);
  if (handler)
    handler->Open(browser_id, url, base::Bind(&NoOp));
}

void InspectUI::InspectBrowserWithCustomFrontend(
    const std::string& source_id,
    const std::string& browser_id,
    const GURL& frontend_url) {
  if (!frontend_url.SchemeIs(content::kChromeUIScheme) &&
      !frontend_url.SchemeIs(content::kChromeDevToolsScheme) &&
      frontend_url.host() != kLocalHost) {
    return;
  }

  DevToolsTargetsUIHandler* handler = FindTargetHandler(source_id);
  if (!handler)
    return;

  // Fetch agent host from remote browser.
  scoped_refptr<content::DevToolsAgentHost> agent_host =
      handler->GetBrowserAgentHost(browser_id);
  if (agent_host->IsAttached())
    return;

  // Create web contents for the front-end.
  WebContents* inspect_ui = web_ui()->GetWebContents();
  WebContents* front_end = inspect_ui->GetDelegate()->OpenURLFromTab(
      inspect_ui,
      content::OpenURLParams(GURL(url::kAboutBlankURL),
                             content::Referrer(),
                             NEW_FOREGROUND_TAB,
                             content::PAGE_TRANSITION_AUTO_TOPLEVEL,
                             false));

  // Install devtools bindings.
  DevToolsUIBindings* bindings = new DevToolsUIBindings(front_end,
                                                        frontend_url);
  bindings->AttachTo(agent_host.get());
}

void InspectUI::InspectDevices(Browser* browser) {
  content::RecordAction(base::UserMetricsAction("InspectDevices"));
  chrome::NavigateParams params(chrome::GetSingletonTabNavigateParams(
      browser, GURL(chrome::kChromeUIInspectURL)));
  params.path_behavior = chrome::NavigateParams::IGNORE_AND_NAVIGATE;
  ShowSingletonTabOverwritingNTP(browser, params);
}

void InspectUI::Observe(int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (source == content::Source<WebContents>(web_ui()->GetWebContents()))
    StopListeningNotifications();
}

void InspectUI::StartListeningNotifications() {
  if (!target_handlers_.empty())  // Possible when reloading the page.
    StopListeningNotifications();

  Profile* profile = Profile::FromWebUI(web_ui());

  DevToolsTargetsUIHandler::Callback callback =
      base::Bind(&InspectUI::PopulateTargets, base::Unretained(this));

  AddTargetUIHandler(
      DevToolsTargetsUIHandler::CreateForRenderers(callback));
  AddTargetUIHandler(
      DevToolsTargetsUIHandler::CreateForWorkers(callback));
  if (profile->IsOffTheRecord()) {
    ShowIncognitoWarning();
  } else {
    AddTargetUIHandler(
        DevToolsTargetsUIHandler::CreateForAdb(callback, profile));
  }

  port_status_serializer_.reset(
      new PortForwardingStatusSerializer(
          base::Bind(&InspectUI::PopulatePortStatus, base::Unretained(this)),
          profile));

  notification_registrar_.Add(this,
                              content::NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
                              content::NotificationService::AllSources());

  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(prefs::kDevToolsDiscoverUsbDevicesEnabled,
      base::Bind(&InspectUI::UpdateDiscoverUsbDevicesEnabled,
                 base::Unretained(this)));
  pref_change_registrar_.Add(prefs::kDevToolsPortForwardingEnabled,
      base::Bind(&InspectUI::UpdatePortForwardingEnabled,
                 base::Unretained(this)));
  pref_change_registrar_.Add(prefs::kDevToolsPortForwardingConfig,
      base::Bind(&InspectUI::UpdatePortForwardingConfig,
                 base::Unretained(this)));
}

void InspectUI::StopListeningNotifications() {
  if (target_handlers_.empty())
    return;

  STLDeleteValues(&target_handlers_);

  port_status_serializer_.reset();

  notification_registrar_.RemoveAll();
  pref_change_registrar_.RemoveAll();
}

content::WebUIDataSource* InspectUI::CreateInspectUIHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIInspectHost);
  source->AddResourcePath("inspect.css", IDR_INSPECT_CSS);
  source->AddResourcePath("inspect.js", IDR_INSPECT_JS);
  source->SetDefaultResource(IDR_INSPECT_HTML);
  source->OverrideContentSecurityPolicyFrameSrc(
      "frame-src chrome://serviceworker-internals;");
  serviceworker_webui_.reset(web_ui()->GetWebContents()->CreateWebUI(
      GURL(content::kChromeUIServiceWorkerInternalsURL)));
  serviceworker_webui_->OverrideJavaScriptFrame(
      content::kChromeUIServiceWorkerInternalsHost);
  return source;
}

void InspectUI::RenderViewCreated(content::RenderViewHost* render_view_host) {
  serviceworker_webui_->GetController()->RenderViewCreated(render_view_host);
}

void InspectUI::RenderViewReused(content::RenderViewHost* render_view_host) {
  serviceworker_webui_->GetController()->RenderViewReused(render_view_host);
}

bool InspectUI::OverrideHandleWebUIMessage(const GURL& source_url,
                                           const std::string& message,
                                           const base::ListValue& args) {
  if (source_url.SchemeIs(content::kChromeUIScheme) &&
      source_url.host() == content::kChromeUIServiceWorkerInternalsHost) {
    serviceworker_webui_->ProcessWebUIMessage(source_url, message, args);
    return true;
  }
  return false;
}

void InspectUI::UpdateDiscoverUsbDevicesEnabled() {
  web_ui()->CallJavascriptFunction(
      "updateDiscoverUsbDevicesEnabled",
      *GetPrefValue(prefs::kDevToolsDiscoverUsbDevicesEnabled));
}

void InspectUI::UpdatePortForwardingEnabled() {
  web_ui()->CallJavascriptFunction(
      "updatePortForwardingEnabled",
      *GetPrefValue(prefs::kDevToolsPortForwardingEnabled));
}

void InspectUI::UpdatePortForwardingConfig() {
  web_ui()->CallJavascriptFunction(
      "updatePortForwardingConfig",
      *GetPrefValue(prefs::kDevToolsPortForwardingConfig));
}

void InspectUI::SetPortForwardingDefaults() {
  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* prefs = profile->GetPrefs();

  bool default_set;
  if (!GetPrefValue(prefs::kDevToolsPortForwardingDefaultSet)->
      GetAsBoolean(&default_set) || default_set) {
    return;
  }

  // This is the first chrome://inspect invocation on a fresh profile or after
  // upgrade from a version that did not have kDevToolsPortForwardingDefaultSet.
  prefs->SetBoolean(prefs::kDevToolsPortForwardingDefaultSet, true);

  bool enabled;
  const base::DictionaryValue* config;
  if (!GetPrefValue(prefs::kDevToolsPortForwardingEnabled)->
        GetAsBoolean(&enabled) ||
      !GetPrefValue(prefs::kDevToolsPortForwardingConfig)->
        GetAsDictionary(&config)) {
    return;
  }

  // Do nothing if user already took explicit action.
  if (enabled || config->size() != 0)
    return;

  base::DictionaryValue default_config;
  default_config.SetString(
      kPortForwardingDefaultPort, kPortForwardingDefaultLocation);
  prefs->Set(prefs::kDevToolsPortForwardingConfig, default_config);
}

const base::Value* InspectUI::GetPrefValue(const char* name) {
  Profile* profile = Profile::FromWebUI(web_ui());
  return profile->GetPrefs()->FindPreference(name)->GetValue();
}

void InspectUI::AddTargetUIHandler(
    scoped_ptr<DevToolsTargetsUIHandler> handler) {
  DevToolsTargetsUIHandler* handler_ptr = handler.release();
  target_handlers_[handler_ptr->source_id()] = handler_ptr;
}

DevToolsTargetsUIHandler* InspectUI::FindTargetHandler(
    const std::string& source_id) {
  TargetHandlerMap::iterator it = target_handlers_.find(source_id);
     return it != target_handlers_.end() ? it->second : NULL;
}

DevToolsTargetImpl* InspectUI::FindTarget(
    const std::string& source_id, const std::string& target_id) {
  TargetHandlerMap::iterator it = target_handlers_.find(source_id);
  return it != target_handlers_.end() ?
         it->second->GetTarget(target_id) : NULL;
}

void InspectUI::PopulateTargets(const std::string& source,
                                const base::ListValue& targets) {
  web_ui()->CallJavascriptFunction("populateTargets",
                                   base::StringValue(source),
                                   targets);
}

void InspectUI::PopulatePortStatus(const base::Value& status) {
  web_ui()->CallJavascriptFunction("populatePortStatus", status);
}

void InspectUI::ShowIncognitoWarning() {
  web_ui()->CallJavascriptFunction("showIncognitoWarning");
}
