// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions Debugger API.

#include "chrome/browser/extensions/api/debugger/debugger_api.h"

#include <map>
#include <set>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/extensions/api/debugger/debugger_api_constants.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/api/debugger.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "extensions/common/error_utils.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/glue/webkit_glue.h"

using content::DevToolsAgentHost;
using content::DevToolsClientHost;
using content::DevToolsManager;
using content::WebContents;
using extensions::api::debugger::Debuggee;
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
      InfoBarService* infobar_service,
      const std::string& client_name,
      ExtensionDevToolsClientHost* client_host);

  // Notifies infobar delegate that associated DevToolsClientHost will be
  // destroyed.
  void DiscardClientHost();

 private:
  ExtensionDevToolsInfoBarDelegate(
      InfoBarService* infobar_service,
      const std::string& client_name,
      ExtensionDevToolsClientHost* client_host);
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
  ExtensionDevToolsClientHost(WebContents* web_contents,
                              const std::string& extension_id,
                              const std::string& extension_name,
                              int tab_id);

  ~ExtensionDevToolsClientHost();

  bool MatchesContentsAndExtensionId(WebContents* web_contents,
                                     const std::string& extension_id);
  void Close();
  void SendMessageToBackend(SendCommandDebuggerFunction* function,
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
                       const content::NotificationDetails& details);

  WebContents* web_contents_;
  std::string extension_id_;
  int tab_id_;
  content::NotificationRegistrar registrar_;
  int last_request_id_;
  typedef std::map<int, scoped_refptr<SendCommandDebuggerFunction> >
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

}  // namespace

ExtensionDevToolsClientHost::ExtensionDevToolsClientHost(
    WebContents* web_contents,
    const std::string& extension_id,
    const std::string& extension_name,
    int tab_id)
    : web_contents_(web_contents),
      extension_id_(extension_id),
      tab_id_(tab_id),
      last_request_id_(0),
      infobar_delegate_(NULL),
      detach_reason_(OnDetach::REASON_TARGET_CLOSED) {
  AttachedClientHosts::GetInstance()->Add(this);

  // Detach from debugger when extension unloads.
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile));

  // Attach to debugger and tell it we are ready.
  scoped_refptr<DevToolsAgentHost> agent(DevToolsAgentHost::GetFor(
      web_contents_->GetRenderViewHost()));
  DevToolsManager::GetInstance()->RegisterDevToolsClientHostFor(agent, this);

  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents_);
  infobar_delegate_ = ExtensionDevToolsInfoBarDelegate::Create(infobar_service,
                                                               extension_name,
                                                               this);
  if (infobar_delegate_) {
    registrar_.Add(this, chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
                   content::Source<InfoBarService>(infobar_service));
  }
}

ExtensionDevToolsClientHost::~ExtensionDevToolsClientHost() {
  // Ensure calling RemoveInfoBar() below won't result in Observe() trying to
  // Close() us.
  registrar_.RemoveAll();

  if (infobar_delegate_) {
    infobar_delegate_->DiscardClientHost();
    InfoBarService* infobar_service =
        InfoBarService::FromWebContents(web_contents_);
    if (infobar_service)
      infobar_service->RemoveInfoBar(infobar_delegate_);
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
    SendCommandDebuggerFunction* function,
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
    Debuggee debuggee;
    debuggee.tab_id = tab_id_;
    scoped_ptr<base::ListValue> args(OnDetach::Create(debuggee,
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

    Debuggee debuggee;
    debuggee.tab_id = tab_id_;

    OnEvent::Params params;
    DictionaryValue* params_value;
    if (dictionary->GetDictionary("params", &params_value))
      params.additional_properties.Swap(params_value);

    scoped_ptr<ListValue> args(OnEvent::Create(debuggee, method_name, params));
    scoped_ptr<extensions::Event> event(new extensions::Event(
        keys::kOnEvent, args.Pass()));
    event->restrict_to_profile = profile;
    extensions::ExtensionSystem::Get(profile)->event_router()->
        DispatchEventToExtension(extension_id_, event.Pass());
  } else {
    SendCommandDebuggerFunction* function = pending_requests_[id];
    if (!function)
      return;

    function->SendResponseBody(dictionary);
    pending_requests_.erase(id);
  }
}

// static
ExtensionDevToolsInfoBarDelegate* ExtensionDevToolsInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    const std::string& client_name,
    ExtensionDevToolsClientHost* client_host) {
  return static_cast<ExtensionDevToolsInfoBarDelegate*>(
      infobar_service->AddInfoBar(scoped_ptr<InfoBarDelegate>(
          new ExtensionDevToolsInfoBarDelegate(infobar_service, client_name,
                                               client_host))));
}

void ExtensionDevToolsInfoBarDelegate::DiscardClientHost() {
  client_host_ = NULL;
}

ExtensionDevToolsInfoBarDelegate::ExtensionDevToolsInfoBarDelegate(
    InfoBarService* infobar_service,
    const std::string& client_name,
    ExtensionDevToolsClientHost* client_host)
    : ConfirmInfoBarDelegate(infobar_service),
      client_name_(client_name),
      client_host_(client_host) {
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
      tab_id_(0),
      client_host_(0) {
}

bool DebuggerFunction::InitWebContents() {
  // Find the WebContents that contains this tab id.
  contents_ = NULL;
  WebContents* web_contents = NULL;
  bool result = ExtensionTabUtil::GetTabById(
      tab_id_, profile(), include_incognito(), NULL, NULL, &web_contents, NULL);
  if (!result || !web_contents) {
    error_ = ErrorUtils::FormatErrorMessage(
        keys::kNoTabError,
        base::IntToString(tab_id_));
    return false;
  }
  contents_ = web_contents;

  if (content::GetContentClient()->HasWebUIScheme(
          contents_->GetURL())) {
    error_ = ErrorUtils::FormatErrorMessage(
        keys::kAttachToWebUIError,
        contents_->GetURL().scheme());
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
    error_ = ErrorUtils::FormatErrorMessage(
        keys::kNotAttachedError,
        base::IntToString(tab_id_));
    return false;
  }
  return true;
}

AttachDebuggerFunction::AttachDebuggerFunction() {}

AttachDebuggerFunction::~AttachDebuggerFunction() {}

bool AttachDebuggerFunction::RunImpl() {
  scoped_ptr<Attach::Params> params(Attach::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  tab_id_ = params->target.tab_id;
  if (!InitWebContents())
    return false;

  if (!webkit_glue::IsInspectorProtocolVersionSupported(
      params->required_version)) {
    error_ = ErrorUtils::FormatErrorMessage(
        keys::kProtocolVersionNotSupportedError,
        params->required_version);
    return false;
  }

  scoped_refptr<DevToolsAgentHost> agent(DevToolsAgentHost::GetFor(
      contents_->GetRenderViewHost()));
  DevToolsClientHost* client_host = DevToolsManager::GetInstance()->
      GetDevToolsClientHostFor(agent);

  if (client_host != NULL) {
    error_ = ErrorUtils::FormatErrorMessage(
        keys::kAlreadyAttachedError,
        base::IntToString(tab_id_));
    return false;
  }

  new ExtensionDevToolsClientHost(contents_,
                                  GetExtension()->id(),
                                  GetExtension()->name(),
                                  tab_id_);
  SendResponse(true);
  return true;
}

DetachDebuggerFunction::DetachDebuggerFunction() {}

DetachDebuggerFunction::~DetachDebuggerFunction() {}

bool DetachDebuggerFunction::RunImpl() {
  scoped_ptr<Detach::Params> params(Detach::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  tab_id_ = params->target.tab_id;
  if (!InitClientHost())
    return false;

  client_host_->Close();
  SendResponse(true);
  return true;
}

SendCommandDebuggerFunction::SendCommandDebuggerFunction() {}

SendCommandDebuggerFunction::~SendCommandDebuggerFunction() {}

bool SendCommandDebuggerFunction::RunImpl() {
  scoped_ptr<SendCommand::Params> params(SendCommand::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  tab_id_ = params->target.tab_id;
  if (!InitClientHost())
    return false;

  client_host_->SendMessageToBackend(this, params->method,
      params->command_params.get());
  return true;
}

void SendCommandDebuggerFunction::SendResponseBody(
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
