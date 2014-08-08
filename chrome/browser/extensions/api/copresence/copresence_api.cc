// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/copresence/copresence_api.h"

#include "base/lazy_instance.h"
#include "base/memory/linked_ptr.h"
#include "chrome/browser/copresence/chrome_whispernet_client.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/api/copresence.h"
#include "components/copresence/proto/data.pb.h"
#include "components/copresence/proto/enums.pb.h"
#include "components/copresence/proto/rpcs.pb.h"
#include "components/copresence/public/copresence_client.h"
#include "components/copresence/public/whispernet_client.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"

namespace extensions {

namespace {

base::LazyInstance<BrowserContextKeyedAPIFactory<CopresenceService> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

const char kInvalidOperationsMessage[] =
    "Invalid operation in operations array.";
const char kShuttingDownMessage[] = "Shutting down.";

}  // namespace

// CopresenceService implementation:

CopresenceService::CopresenceService(content::BrowserContext* context)
    : is_shutting_down_(false), browser_context_(context) {
}

CopresenceService::~CopresenceService() {
}

copresence::CopresenceClient* CopresenceService::client() {
  if (!client_ && !is_shutting_down_)
    client_.reset(new copresence::CopresenceClient(this));
  return client_.get();
}

copresence::WhispernetClient* CopresenceService::whispernet_client() {
  if (!whispernet_client_ && !is_shutting_down_)
    whispernet_client_.reset(new ChromeWhispernetClient(browser_context_));
  return whispernet_client_.get();
}

void CopresenceService::Shutdown() {
  is_shutting_down_ = true;
  client_.reset();
  whispernet_client_.reset();
}

// static
BrowserContextKeyedAPIFactory<CopresenceService>*
CopresenceService::GetFactoryInstance() {
  return g_factory.Pointer();
}

void CopresenceService::HandleMessages(
    const std::string& /* app_id */,
    const std::string& subscription_id,
    const std::vector<copresence::Message>& messages) {
  // TODO(ckehoe): Once the server starts sending back the app ids associated
  // with subscriptions, use that instead of the apps_by_subs registry.
  std::string app_id = apps_by_subscription_id_[subscription_id];

  if (app_id.empty()) {
    LOG(ERROR) << "Skipping message from unrecognized subscription "
               << subscription_id;
    return;
  }

  int message_count = messages.size();
  std::vector<linked_ptr<api::copresence::Message> > api_messages(
      message_count);

  for (int m = 0; m < message_count; ++m) {
    api_messages[m].reset(new api::copresence::Message);
    api_messages[m]->type = messages[m].type().type();
    api_messages[m]->payload = messages[m].payload();
    DVLOG(2) << "Dispatching message of type " << api_messages[m]->type << ":\n"
             << api_messages[m]->payload;
  }

  // Send the messages to the client app.
  scoped_ptr<Event> event(
      new Event(api::copresence::OnMessagesReceived::kEventName,
                api::copresence::OnMessagesReceived::Create(subscription_id,
                                                            api_messages),
                browser_context_));
  EventRouter::Get(browser_context_)
      ->DispatchEventToExtension(app_id, event.Pass());
  DVLOG(2) << "Passed " << api_messages.size() << " messages to app \""
           << app_id << "\" for subscription \"" << subscription_id << "\"";
}

net::URLRequestContextGetter* CopresenceService::GetRequestContext() const {
  return browser_context_->GetRequestContext();
}

const std::string CopresenceService::GetPlatformVersionString() const {
  return chrome::VersionInfo().CreateVersionString();
}

copresence::WhispernetClient* CopresenceService::GetWhispernetClient() {
  return whispernet_client();
}

template <>
void
BrowserContextKeyedAPIFactory<CopresenceService>::DeclareFactoryDependencies() {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

// CopresenceExecuteFunction implementation:
ExtensionFunction::ResponseAction CopresenceExecuteFunction::Run() {
  scoped_ptr<api::copresence::Execute::Params> params(
      api::copresence::Execute::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  CopresenceService* service =
      CopresenceService::GetFactoryInstance()->Get(browser_context());

  // This can only happen if we're shutting down. In all other cases, if we
  // don't have a client, we'll create one.
  if (!service->client())
    return RespondNow(Error(kShuttingDownMessage));

  // Each execute will correspond to one ReportRequest protocol buffer.
  copresence::ReportRequest request;
  if (!PrepareReportRequestProto(params->operations,
                                 extension_id(),
                                 &service->apps_by_subscription_id(),
                                 &request)) {
    return RespondNow(Error(kInvalidOperationsMessage));
  }

  service->client()->ExecuteReportRequest(
      request,
      extension_id(),
      base::Bind(&CopresenceExecuteFunction::SendResult, this));
  return RespondLater();
}

void CopresenceExecuteFunction::SendResult(
    copresence::CopresenceStatus status) {
  api::copresence::ExecuteStatus api_status =
      (status == copresence::SUCCESS) ? api::copresence::EXECUTE_STATUS_SUCCESS
                                      : api::copresence::EXECUTE_STATUS_FAILED;
  Respond(ArgumentList(api::copresence::Execute::Results::Create(api_status)));
}

// CopresenceSetApiKeyFunction implementation:
ExtensionFunction::ResponseAction CopresenceSetApiKeyFunction::Run() {
  scoped_ptr<api::copresence::SetApiKey::Params> params(
      api::copresence::SetApiKey::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // TODO(rkc): Use the API key set by this function for this app.
  // http://crbug.com/400617.
  return RespondNow(NoArguments());
}

}  // namespace extensions
