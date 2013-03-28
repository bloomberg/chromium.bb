// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions Debugger API.

#include "chrome/browser/extensions/api/debugger/debugger_api.h"

#include <map>
#include <set>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/debugger/debugger_api_constants.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/error_utils.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/glue/webkit_glue.h"

using content::DevToolsAgentHost;
using content::DevToolsClientHost;
using content::DevToolsManager;
using content::RenderProcessHost;
using content::RenderViewHost;
using content::RenderWidgetHost;
using content::WebContents;
using extensions::ErrorUtils;

namespace keys = debugger_api_constants;
namespace Attach = extensions::api::debugger::Attach;
namespace Detach = extensions::api::debugger::Detach;
namespace OnDetach = extensions::api::debugger::OnDetach;
namespace OnEvent = extensions::api::debugger::OnEvent;
namespace SendCommand = extensions::api::debugger::SendCommand;

class ExtensionDevToolsClientHost;

class ExtensionDevToolsInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates an extension dev tools delegate and adds it to |infobar_service|.
  // Returns a pointer to the delegate if it was successfully added.
  static ExtensionDevToolsInfoBarDelegate* Create(
      WebContents* web_contents,
      const std::string& client_name);

  // Associates DevToolsClientHost with this infobar delegate.
  void AttachClientHost(ExtensionDevToolsClientHost* client_host);

  // Notifies infobar delegate that associated DevToolsClientHost will be
  // destroyed.
  void DiscardClientHost();

 private:
  ExtensionDevToolsInfoBarDelegate(InfoBarService* infobar_service,
                                   const std::string& client_name);
  virtual ~ExtensionDevToolsInfoBarDelegate();

  // ConfirmInfoBarDelegate:
  virtual int GetButtons() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual bool ShouldExpireInternal(
      const content::LoadCommittedDetails& details) const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;
  virtual void InfoBarDismissed() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

  std::string client_name_;
  ExtensionDevToolsClientHost* client_host_;
  DISALLOW_COPY_AND_ASSIGN(ExtensionDevToolsInfoBarDelegate);
};

class ExtensionDevToolsClientHost : public DevToolsClientHost,
                                    public content::NotificationObserver {
 public:
  ExtensionDevToolsClientHost(
      WebContents* web_contents,
      const std::string& extension_id,
      const std::string& extension_name,
      const Debuggee& debuggee,
      ExtensionDevToolsInfoBarDelegate* infobar_delegate);

  virtual ~ExtensionDevToolsClientHost();

  bool MatchesContentsAndExtensionId(WebContents* web_contents,
                                     const std::string& extension_id);
  void Close();
  void SendMessageToBackend(DebuggerSendCommandFunction* function,
                            const std::string& method,
                            SendCommand::Params::CommandParams* command_params);

  // Marks connection as to-be-terminated by the user.
  void MarkAsDismissed();

  // DevToolsClientHost interface
  virtual void InspectedContentsClosing() OVERRIDE;
  virtual void DispatchOnInspectorFrontend(const std::string& message) OVERRIDE;
  virtual void ReplacedWithAnotherClient() OVERRIDE;

 private:
  void SendDetachedEvent();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  WebContents* web_contents_;
  std::string extension_id_;
  Debuggee debuggee_;
  content::NotificationRegistrar registrar_;
  int last_request_id_;
  typedef std::map<int, scoped_refptr<DebuggerSendCommandFunction> >
      PendingRequests;
  PendingRequests pending_requests_;
  ExtensionDevToolsInfoBarDelegate* infobar_delegate_;
  OnDetach::Reason detach_reason_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionDevToolsClientHost);
};

namespace {

class AttachedClientHosts {
 public:
  AttachedClientHosts() {}

  // Returns the singleton instance of this class
  static AttachedClientHosts* GetInstance() {
    return Singleton<AttachedClientHosts>::get();
  }

  void Add(ExtensionDevToolsClientHost* client_host) {
    client_hosts_.insert(client_host);
  }

  void Remove(ExtensionDevToolsClientHost* client_host) {
    client_hosts_.erase(client_host);
  }

  ExtensionDevToolsClientHost* Lookup(WebContents* contents) {
    for (std::set<DevToolsClientHost*>::iterator it = client_hosts_.begin();
         it != client_hosts_.end(); ++it) {
      DevToolsAgentHost* agent_host =
          DevToolsManager::GetInstance()->GetDevToolsAgentHostFor(*it);
      if (!agent_host)
        continue;
      content::RenderViewHost* rvh = agent_host->GetRenderViewHost();
      if (rvh && WebContents::FromRenderViewHost(rvh) == contents)
        return static_cast<ExtensionDevToolsClientHost*>(*it);
    }
    return NULL;
  }

 private:
  std::set<DevToolsClientHost*> client_hosts_;
};

static extensions::ExtensionHost* GetExtensionBackgroundHost(
    WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile)
    return NULL;

  extensions::ExtensionHost* extension_host =
      extensions::ExtensionSystem::Get(profile)->process_manager()->
          GetBackgroundHostForExtension(web_contents->GetURL().host());

  if (extension_host && extension_host->host_contents() == web_contents)
    return extension_host;

  return NULL;
}

static const char kTargetIdField[]  = "id";
static const char kTargetTypeField[]  = "type";
static const char kTargetTitleField[]  = "title";
static const char kTargetAttachedField[]  = "attached";
static const char kTargetUrlField[]  = "url";
static const char kTargetFaviconUrlField[] = "faviconUrl";

static const char kTargetTypePage[]  = "page";
static const char kTargetTypeBackgroundPage[]  = "background_page";

static base::Value* SerializePageInfo(RenderViewHost* rvh) {
  WebContents* web_contents = WebContents::FromRenderViewHost(rvh);
  if (!web_contents)
    return NULL;

  DevToolsAgentHost* agent_host = DevToolsAgentHost::GetOrCreateFor(rvh);

  base::DictionaryValue* dictionary = new base::DictionaryValue();

  dictionary->SetString(kTargetIdField, agent_host->GetId());
  dictionary->SetBoolean(kTargetAttachedField,
      !!DevToolsManager::GetInstance()->GetDevToolsClientHostFor(agent_host));
  dictionary->SetString(kTargetUrlField, web_contents->GetURL().spec());


  extensions::ExtensionHost* extension_host =
      GetExtensionBackgroundHost(web_contents);
  if (extension_host) {
    // This RenderViewHost belongs to a background page.
    dictionary->SetString(kTargetTypeField, kTargetTypeBackgroundPage);
    dictionary->SetString(kTargetTitleField,
        extension_host->extension()->name());
  } else {
    // This RenderViewHost belongs to a regular page.
    dictionary->SetString(kTargetTypeField, kTargetTypePage);
    dictionary->SetString(kTargetTitleField,
        UTF16ToUTF8(net::EscapeForHTML(web_contents->GetTitle())));

    content::NavigationController& controller = web_contents->GetController();
    content::NavigationEntry* entry = controller.GetActiveEntry();
    if (entry != NULL && entry->GetURL().is_valid()) {
      dictionary->SetString(kTargetFaviconUrlField,
          entry->GetFavicon().url.spec());
    }
  }

  return dictionary;
}

}  // namespace

static void CopyDebuggee(Debuggee & dst, const Debuggee& src) {
  if (src.tab_id)
    dst.tab_id.reset(new int(*src.tab_id));
  if (src.extension_id)
    dst.extension_id.reset(new std::string(*src.extension_id));
  if (src.target_id)
    dst.target_id.reset(new std::string(*src.target_id));
}

ExtensionDevToolsClientHost::ExtensionDevToolsClientHost(
    WebContents* web_contents,
    const std::string& extension_id,
    const std::string& extension_name,
    const Debuggee& debuggee,
    ExtensionDevToolsInfoBarDelegate* infobar_delegate)
    : web_contents_(web_contents),
      extension_id_(extension_id),
      last_request_id_(0),
      infobar_delegate_(infobar_delegate),
      detach_reason_(OnDetach::REASON_TARGET_CLOSED) {
  CopyDebuggee(debuggee_, debuggee);

  AttachedClientHosts::GetInstance()->Add(this);

  // Detach from debugger when extension unloads.
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile));

  // Attach to debugger and tell it we are ready.
  scoped_refptr<DevToolsAgentHost> agent(DevToolsAgentHost::GetOrCreateFor(
      web_contents_->GetRenderViewHost()));
  DevToolsManager::GetInstance()->RegisterDevToolsClientHostFor(agent, this);

  if (infobar_delegate_) {
    infobar_delegate_->AttachClientHost(this);
    registrar_.Add(this,
                   chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
                   content::Source<InfoBarService>(infobar_delegate_->owner()));
  }
}

ExtensionDevToolsClientHost::~ExtensionDevToolsClientHost() {
  // Ensure calling RemoveInfoBar() below won't result in Observe() trying to
  // Close() us.
  registrar_.RemoveAll();

  if (infobar_delegate_) {
    infobar_delegate_->DiscardClientHost();
    if (infobar_delegate_->owner())
      infobar_delegate_->owner()->RemoveInfoBar(infobar_delegate_);
  }
  AttachedClientHosts::GetInstance()->Remove(this);
}

bool ExtensionDevToolsClientHost::MatchesContentsAndExtensionId(
    WebContents* web_contents,
    const std::string& extension_id) {
  return web_contents == web_contents_ && extension_id_ == extension_id;
}

// DevToolsClientHost interface
void ExtensionDevToolsClientHost::InspectedContentsClosing() {
  SendDetachedEvent();
  delete this;
}

void ExtensionDevToolsClientHost::ReplacedWithAnotherClient() {
  detach_reason_ = OnDetach::REASON_REPLACED_WITH_DEVTOOLS;
}

void ExtensionDevToolsClientHost::Close() {
  DevToolsManager::GetInstance()->ClientHostClosing(this);
  delete this;
}

void ExtensionDevToolsClientHost::SendMessageToBackend(
    DebuggerSendCommandFunction* function,
    const std::string& method,
    SendCommand::Params::CommandParams* command_params) {
  DictionaryValue protocol_request;
  int request_id = ++last_request_id_;
  pending_requests_[request_id] = function;
  protocol_request.SetInteger("id", request_id);
  protocol_request.SetString("method", method);
  if (command_params) {
    protocol_request.Set("params",
                         command_params->additional_properties.DeepCopy());
  }

  std::string json_args;
  base::JSONWriter::Write(&protocol_request, &json_args);
  DevToolsManager::GetInstance()->DispatchOnInspectorBackend(this, json_args);
}

void ExtensionDevToolsClientHost::MarkAsDismissed() {
  detach_reason_ = OnDetach::REASON_CANCELED_BY_USER;
}

void ExtensionDevToolsClientHost::SendDetachedEvent() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  if (profile != NULL &&
      extensions::ExtensionSystem::Get(profile)->event_router()) {
    scoped_ptr<base::ListValue> args(OnDetach::Create(debuggee_,
                                                      detach_reason_));
    scoped_ptr<extensions::Event> event(new extensions::Event(
        keys::kOnDetach, args.Pass()));
    event->restrict_to_profile = profile;
    extensions::ExtensionSystem::Get(profile)->event_router()->
        DispatchEventToExtension(extension_id_, event.Pass());
  }
}

void ExtensionDevToolsClientHost::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_EXTENSION_UNLOADED) {
    std::string id =
        content::Details<extensions::UnloadedExtensionInfo>(details)->
            extension->id();
    if (id == extension_id_)
        Close();
  } else {
    DCHECK_EQ(chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED, type);
    if (content::Details<InfoBarRemovedDetails>(details)->first ==
        infobar_delegate_) {
      infobar_delegate_ = NULL;
      SendDetachedEvent();
      Close();
    }
  }
}

void ExtensionDevToolsClientHost::DispatchOnInspectorFrontend(
    const std::string& message) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  if (profile == NULL ||
      !extensions::ExtensionSystem::Get(profile)->event_router())
    return;

  scoped_ptr<Value> result(base::JSONReader::Read(message));
  if (!result->IsType(Value::TYPE_DICTIONARY))
    return;
  DictionaryValue* dictionary = static_cast<DictionaryValue*>(result.get());

  int id;
  if (!dictionary->GetInteger("id", &id)) {
    std::string method_name;
    if (!dictionary->GetString("method", &method_name))
      return;

    OnEvent::Params params;
    DictionaryValue* params_value;
    if (dictionary->GetDictionary("params", &params_value))
      params.additional_properties.Swap(params_value);

    scoped_ptr<ListValue> args(OnEvent::Create(debuggee_, method_name, params));
    scoped_ptr<extensions::Event> event(new extensions::Event(
        keys::kOnEvent, args.Pass()));
    event->restrict_to_profile = profile;
    extensions::ExtensionSystem::Get(profile)->event_router()->
        DispatchEventToExtension(extension_id_, event.Pass());
  } else {
    DebuggerSendCommandFunction* function = pending_requests_[id];
    if (!function)
      return;

    function->SendResponseBody(dictionary);
    pending_requests_.erase(id);
  }
}

// static
ExtensionDevToolsInfoBarDelegate* ExtensionDevToolsInfoBarDelegate::Create(
    WebContents* web_contents,
    const std::string& client_name) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  if (!infobar_service)
    return NULL;
  return static_cast<ExtensionDevToolsInfoBarDelegate*>(
      infobar_service->AddInfoBar(scoped_ptr<InfoBarDelegate>(
          new ExtensionDevToolsInfoBarDelegate(infobar_service, client_name))));
}

void ExtensionDevToolsInfoBarDelegate::AttachClientHost(
    ExtensionDevToolsClientHost* client_host) {
  client_host_ = client_host;
}

void ExtensionDevToolsInfoBarDelegate::DiscardClientHost() {
  client_host_ = NULL;
}

ExtensionDevToolsInfoBarDelegate::ExtensionDevToolsInfoBarDelegate(
    InfoBarService* infobar_service,
    const std::string& client_name)
    : ConfirmInfoBarDelegate(infobar_service),
      client_name_(client_name),
      client_host_(NULL) {
}

ExtensionDevToolsInfoBarDelegate::~ExtensionDevToolsInfoBarDelegate() {
}

int ExtensionDevToolsInfoBarDelegate::GetButtons() const {
  return BUTTON_CANCEL;
}

InfoBarDelegate::Type ExtensionDevToolsInfoBarDelegate::GetInfoBarType() const {
  return WARNING_TYPE;
}

bool ExtensionDevToolsInfoBarDelegate::ShouldExpireInternal(
    const content::LoadCommittedDetails& details) const {
  return false;
}

string16 ExtensionDevToolsInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_DEV_TOOLS_INFOBAR_LABEL,
                                    UTF8ToUTF16(client_name_));
}

void ExtensionDevToolsInfoBarDelegate::InfoBarDismissed() {
  if (client_host_)
    client_host_->MarkAsDismissed();
}

bool ExtensionDevToolsInfoBarDelegate::Cancel() {
  if (client_host_)
    client_host_->MarkAsDismissed();
  return true;
}

DebuggerFunction::DebuggerFunction()
    : contents_(0),
      client_host_(0) {
}

void DebuggerFunction::FormatErrorMessage(const std::string& format) {
  if (debuggee_.tab_id)
    error_ = ErrorUtils::FormatErrorMessage(
      format, keys::kTabTargetType, base::IntToString(*debuggee_.tab_id));
  else if (debuggee_.extension_id)
    error_ = ErrorUtils::FormatErrorMessage(
      format, keys::kBackgroundPageTargetType, *debuggee_.extension_id);
  else
    error_ = ErrorUtils::FormatErrorMessage(
      format, keys::kOpaqueTargetType, *debuggee_.target_id);
}

bool DebuggerFunction::InitWebContents() {
  if (debuggee_.tab_id) {
    WebContents* web_contents = NULL;
    bool result = ExtensionTabUtil::GetTabById(
        *debuggee_.tab_id, profile(), include_incognito(), NULL, NULL,
        &web_contents, NULL);
    if (result && web_contents) {
      if (content::HasWebUIScheme(web_contents->GetURL())) {
        error_ = ErrorUtils::FormatErrorMessage(
            keys::kAttachToWebUIError,
            web_contents->GetURL().scheme());
        return false;
      }
      contents_ = web_contents;
    }
  } else if (debuggee_.extension_id) {
    extensions::ExtensionHost* extension_host =
        extensions::ExtensionSystem::Get(profile())->process_manager()->
            GetBackgroundHostForExtension(*debuggee_.extension_id);
    if (extension_host) {
      contents_ = WebContents::FromRenderViewHost(
          extension_host->render_view_host());
    }
  } else if (debuggee_.target_id) {
    DevToolsAgentHost* agent_host =
        DevToolsAgentHost::GetForId(*debuggee_.target_id);
    if (agent_host) {
      contents_ = WebContents::FromRenderViewHost(
          agent_host->GetRenderViewHost());
    }
  } else {
    error_ = keys::kInvalidTargetError;
    return false;
  }

  if (!contents_) {
    FormatErrorMessage(keys::kNoTargetError);
    return false;
  }
  return true;
}

bool DebuggerFunction::InitClientHost() {
  if (!InitWebContents())
    return false;

  // Don't fetch rvh from the contents since it'll be wrong upon navigation.
  client_host_ = AttachedClientHosts::GetInstance()->Lookup(contents_);

  if (!client_host_ ||
      !client_host_->MatchesContentsAndExtensionId(contents_,
                                                   GetExtension()->id())) {
    FormatErrorMessage(keys::kNotAttachedError);
    return false;
  }
  return true;
}

DebuggerAttachFunction::DebuggerAttachFunction() {}

DebuggerAttachFunction::~DebuggerAttachFunction() {}

bool DebuggerAttachFunction::RunImpl() {
  scoped_ptr<Attach::Params> params(Attach::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  CopyDebuggee(debuggee_, params->target);
  if (!InitWebContents())
    return false;

  if (!webkit_glue::IsInspectorProtocolVersionSupported(
      params->required_version)) {
    error_ = ErrorUtils::FormatErrorMessage(
        keys::kProtocolVersionNotSupportedError,
        params->required_version);
    return false;
  }

  scoped_refptr<DevToolsAgentHost> agent(DevToolsAgentHost::GetOrCreateFor(
      contents_->GetRenderViewHost()));
  DevToolsClientHost* client_host = DevToolsManager::GetInstance()->
      GetDevToolsClientHostFor(agent);

  if (client_host != NULL) {
    FormatErrorMessage(keys::kAlreadyAttachedError);
    return false;
  }

  ExtensionDevToolsInfoBarDelegate* infobar_delegate = NULL;

  if (!CommandLine::ForCurrentProcess()->
       HasSwitch(switches::kSilentDebuggerExtensionAPI)) {
    // Do not attach to the target if for any reason the infobar cannot be shown
    // for this WebContents instance.
    infobar_delegate = ExtensionDevToolsInfoBarDelegate::Create(
          contents_, GetExtension()->name());
    if (!infobar_delegate) {
      error_ = ErrorUtils::FormatErrorMessage(
          keys::kSilentDebuggingRequired,
          switches::kSilentDebuggerExtensionAPI);
      return false;
    }
  }

  new ExtensionDevToolsClientHost(contents_,
                                  GetExtension()->id(),
                                  GetExtension()->name(),
                                  debuggee_,
                                  infobar_delegate);
  SendResponse(true);
  return true;
}

DebuggerDetachFunction::DebuggerDetachFunction() {}

DebuggerDetachFunction::~DebuggerDetachFunction() {}

bool DebuggerDetachFunction::RunImpl() {
  scoped_ptr<Detach::Params> params(Detach::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  CopyDebuggee(debuggee_, params->target);
  if (!InitClientHost())
    return false;

  client_host_->Close();
  SendResponse(true);
  return true;
}

DebuggerSendCommandFunction::DebuggerSendCommandFunction() {}

DebuggerSendCommandFunction::~DebuggerSendCommandFunction() {}

bool DebuggerSendCommandFunction::RunImpl() {
  scoped_ptr<SendCommand::Params> params(SendCommand::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  CopyDebuggee(debuggee_, params->target);
  if (!InitClientHost())
    return false;

  client_host_->SendMessageToBackend(this, params->method,
      params->command_params.get());
  return true;
}

void DebuggerSendCommandFunction::SendResponseBody(
    DictionaryValue* response) {
  Value* error_body;
  if (response->Get("error", &error_body)) {
    base::JSONWriter::Write(error_body, &error_);
    SendResponse(false);
    return;
  }

  DictionaryValue* result_body;
  SendCommand::Results::Result result;
  if (response->GetDictionary("result", &result_body))
    result.additional_properties.Swap(result_body);

  results_ = SendCommand::Results::Create(result);
  SendResponse(true);
}

DebuggerGetTargetsFunction::DebuggerGetTargetsFunction() {}

DebuggerGetTargetsFunction::~DebuggerGetTargetsFunction() {}

bool DebuggerGetTargetsFunction::RunImpl() {
  base::ListValue* results_list = new ListValue();

  std::vector<RenderViewHost*> rvh_list =
      DevToolsAgentHost::GetValidRenderViewHosts();
  for (std::vector<RenderViewHost*>::iterator it = rvh_list.begin();
       it != rvh_list.end(); ++it) {
    base::Value* value = SerializePageInfo(*it);
    if (value)
      results_list->Append(value);
  }

  SetResult(results_list);
  SendResponse(true);
  return true;
}
