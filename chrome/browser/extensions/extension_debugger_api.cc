// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implements the Chrome Extensions Debugger API.

#include "chrome/browser/extensions/extension_debugger_api.h"

#include <map>
#include <set>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/singleton.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_debugger_api_constants.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "content/public/browser/devtools_agent_host_registry.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_view_host_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/glue/webkit_glue.h"

using content::DevToolsAgentHost;
using content::DevToolsAgentHostRegistry;
using content::DevToolsClientHost;
using content::DevToolsManager;

namespace keys = extension_debugger_api_constants;

using content::WebContents;

class ExtensionDevToolsInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  ExtensionDevToolsInfoBarDelegate(
      InfoBarTabHelper* infobar_helper,
      const std::string& client_name);
  virtual ~ExtensionDevToolsInfoBarDelegate();

 private:
  // ConfirmInfoBarDelegate:
  virtual bool ShouldExpire(
      const content::LoadCommittedDetails& details) const OVERRIDE;
  virtual int GetButtons() const OVERRIDE;
  virtual Type GetInfoBarType() const OVERRIDE;
  virtual string16 GetMessageText() const OVERRIDE;

  std::string client_name_;
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
                            Value* params);

  // DevToolsClientHost interface
  virtual void InspectedContentsClosing() OVERRIDE;
  virtual void DispatchOnInspectorFrontend(const std::string& message) OVERRIDE;
  virtual void ContentsReplaced(WebContents* web_contents) OVERRIDE;
  virtual void FrameNavigating(const std::string& url) OVERRIDE {}

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

  DISALLOW_COPY_AND_ASSIGN(ExtensionDevToolsClientHost);
};

namespace {

static Value* CreateDebuggeeId(int tab_id) {
  DictionaryValue* debuggeeId = new DictionaryValue();
  debuggeeId->SetInteger(keys::kTabIdKey, tab_id);
  return debuggeeId;
}

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
      content::RenderViewHost* rvh =
          DevToolsAgentHostRegistry::GetRenderViewHost(agent_host);
      if (rvh && rvh->GetDelegate() &&
          rvh->GetDelegate()->GetAsWebContents() == contents)
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
      infobar_delegate_(NULL) {
  AttachedClientHosts::GetInstance()->Add(this);

  // Detach from debugger when extension unloads.
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile));

  // Attach to debugger and tell it we are ready.
  DevToolsAgentHost* agent = DevToolsAgentHostRegistry::GetDevToolsAgentHost(
      web_contents_->GetRenderViewHost());
  DevToolsManager::GetInstance()->RegisterDevToolsClientHostFor(agent, this);

  InfoBarTabHelper* infobar_helper =
      TabContentsWrapper::GetCurrentWrapperForContents(web_contents_)->
          infobar_tab_helper();
  infobar_delegate_ =
      new ExtensionDevToolsInfoBarDelegate(infobar_helper, extension_name);
  if (infobar_helper->AddInfoBar(infobar_delegate_)) {
    registrar_.Add(this, chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
                   content::Source<InfoBarTabHelper>(infobar_helper));
  } else {
    infobar_delegate_ = NULL;
  }
}

ExtensionDevToolsClientHost::~ExtensionDevToolsClientHost() {
  // Ensure calling RemoveInfoBar() below won't result in Observe() trying to
  // Close() us.
  registrar_.RemoveAll();

  if (infobar_delegate_) {
    TabContentsWrapper* wrapper =
        TabContentsWrapper::GetCurrentWrapperForContents(web_contents_);
    InfoBarTabHelper* helper = wrapper->infobar_tab_helper();
    if (helper)
      helper->RemoveInfoBar(infobar_delegate_);
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

void ExtensionDevToolsClientHost::ContentsReplaced(WebContents* web_contents) {
  web_contents_ = web_contents;
}

void ExtensionDevToolsClientHost::Close() {
  DevToolsManager::GetInstance()->ClientHostClosing(this);
  delete this;
}

void ExtensionDevToolsClientHost::SendMessageToBackend(
    SendCommandDebuggerFunction* function,
    const std::string& method,
    Value* params) {
  DictionaryValue protocol_request;
  int request_id = ++last_request_id_;
  pending_requests_[request_id] = function;
  protocol_request.SetInteger("id", request_id);
  protocol_request.SetString("method", method);
  if (params)
    protocol_request.Set("params", params->DeepCopy());

  std::string json_args;
  base::JSONWriter::Write(&protocol_request, &json_args);
  DevToolsManager::GetInstance()->DispatchOnInspectorBackend(this, json_args);
}

void ExtensionDevToolsClientHost::SendDetachedEvent() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  if (profile != NULL && profile->GetExtensionEventRouter()) {
    ListValue args;
    args.Append(CreateDebuggeeId(tab_id_));

    std::string json_args;
    base::JSONWriter::Write(&args, &json_args);

    profile->GetExtensionEventRouter()->DispatchEventToExtension(
        extension_id_, keys::kOnDetach, json_args, profile, GURL());
  }
}

void ExtensionDevToolsClientHost::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_EXTENSION_UNLOADED) {
    std::string id =
        content::Details<UnloadedExtensionInfo>(details)->extension->id();
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
  if (profile == NULL || !profile->GetExtensionEventRouter())
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

    ListValue args;
    args.Append(CreateDebuggeeId(tab_id_));
    args.Append(Value::CreateStringValue(method_name));
    Value* params_value;
    if (dictionary->Get("params", &params_value))
      args.Append(params_value->DeepCopy());

    std::string json_args;
    base::JSONWriter::Write(&args, &json_args);

    profile->GetExtensionEventRouter()->DispatchEventToExtension(
        extension_id_, keys::kOnEvent, json_args, profile, GURL());
  } else {
    SendCommandDebuggerFunction* function = pending_requests_[id];
    if (!function)
      return;

    function->SendResponseBody(dictionary);
    pending_requests_.erase(id);
  }
}

ExtensionDevToolsInfoBarDelegate::ExtensionDevToolsInfoBarDelegate(
    InfoBarTabHelper* infobar_helper,
    const std::string& client_name)
    : ConfirmInfoBarDelegate(infobar_helper),
      client_name_(client_name) {
}

ExtensionDevToolsInfoBarDelegate::~ExtensionDevToolsInfoBarDelegate() {
}

bool ExtensionDevToolsInfoBarDelegate::ShouldExpire(
    const content::LoadCommittedDetails& details) const {
  return false;
}

int ExtensionDevToolsInfoBarDelegate::GetButtons() const {
  return BUTTON_CANCEL;
}

InfoBarDelegate::Type ExtensionDevToolsInfoBarDelegate::GetInfoBarType() const {
  return WARNING_TYPE;
}

string16 ExtensionDevToolsInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_DEV_TOOLS_INFOBAR_LABEL,
                                    UTF8ToUTF16(client_name_));
}

DebuggerFunction::DebuggerFunction()
    : contents_(0),
      tab_id_(0),
      client_host_(0) {
}

bool DebuggerFunction::InitTabContents() {
  Value* debuggee;
  EXTENSION_FUNCTION_VALIDATE(args_->Get(0, &debuggee));

  DictionaryValue* dict = static_cast<DictionaryValue*>(debuggee);
  EXTENSION_FUNCTION_VALIDATE(dict->GetInteger(keys::kTabIdKey, &tab_id_));

  // Find the TabContentsWrapper that contains this tab id.
  contents_ = NULL;
  TabContentsWrapper* wrapper = NULL;
  bool result = ExtensionTabUtil::GetTabById(
      tab_id_, profile(), include_incognito(), NULL, NULL, &wrapper, NULL);
  if (!result || !wrapper) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kNoTabError,
        base::IntToString(tab_id_));
    return false;
  }
  contents_ = wrapper->web_contents();

  if (content::GetContentClient()->HasWebUIScheme(
          contents_->GetURL())) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kAttachToWebUIError,
        contents_->GetURL().scheme());
    return false;
  }

  return true;
}

bool DebuggerFunction::InitClientHost() {
  if (!InitTabContents())
    return false;

  // Don't fetch rvh from the contents since it'll be wrong upon navigation.
  client_host_ = AttachedClientHosts::GetInstance()->Lookup(contents_);

  if (!client_host_ ||
      !client_host_->MatchesContentsAndExtensionId(contents_,
                                                   GetExtension()->id())) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kNotAttachedError,
        base::IntToString(tab_id_));
    return false;
  }
  return true;
}

AttachDebuggerFunction::AttachDebuggerFunction() {}

AttachDebuggerFunction::~AttachDebuggerFunction() {}

bool AttachDebuggerFunction::RunImpl() {
  if (!InitTabContents())
    return false;

  std::string version;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &version));

  if (!webkit_glue::IsInspectorProtocolVersionSupported(version)) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        keys::kProtocolVersionNotSupportedError,
        version);
    return false;
  }

  DevToolsAgentHost* agent = DevToolsAgentHostRegistry::GetDevToolsAgentHost(
      contents_->GetRenderViewHost());
  DevToolsClientHost* client_host = DevToolsManager::GetInstance()->
      GetDevToolsClientHostFor(agent);

  if (client_host != NULL) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
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
  if (!InitClientHost())
    return false;

  client_host_->Close();
  SendResponse(true);
  return true;
}

SendCommandDebuggerFunction::SendCommandDebuggerFunction() {}

SendCommandDebuggerFunction::~SendCommandDebuggerFunction() {}

bool SendCommandDebuggerFunction::RunImpl() {

  if (!InitClientHost())
    return false;

  std::string method;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &method));

  Value *params;
  if (!args_->Get(2, &params))
    params = NULL;

  client_host_->SendMessageToBackend(this, method, params);
  return true;
}

void SendCommandDebuggerFunction::SendResponseBody(
    DictionaryValue* dictionary) {
  Value* error_body;
  if (dictionary->Get("error", &error_body)) {
    base::JSONWriter::Write(error_body, &error_);
    SendResponse(false);
    return;
  }

  Value* result_body;
  if (dictionary->Get("result", &result_body))
    result_.reset(result_body->DeepCopy());
  else
    result_.reset(new DictionaryValue());
  SendResponse(true);
}
