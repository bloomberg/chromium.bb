// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/assistant_service.h"

#include "base/strings/strcat.h"
#include "components/autofill_assistant/browser/assistant_protocol_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/url_canon_stdstring.h"

namespace {
// TODO(crbug.com/806868): Provide correct server and endpoint.
const char* const kAutofillAssistantServer = "";
const char* const kScriptEndpoint = "";
const char* const kActionEndpoint = "";

net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation("autofill_assistant_service", R"(
        semantics {
          sender: "Autofill Assistant"
          description:
            "Chromium posts requests to autofill assistant server to get
            assistant scripts for a URL."
          trigger:
            "A user opens a URL that could be assisted by autofill
            assistant server."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature can be disabled in settings."
          policy_exception_justification: "Not implemented."
        })");
}  // namespace

namespace autofill_assistant {

AssistantService::AssistantService(content::BrowserContext* context)
    : context_(context) {
  std::string api_key = google_apis::GetAPIKey();

  url::StringPieceReplacements<std::string> script_replacements;
  script_replacements.SetPathStr(kScriptEndpoint);
  script_replacements.SetQueryStr(api_key);
  assistant_script_server_url_ =
      GURL(kAutofillAssistantServer).ReplaceComponents(script_replacements);

  url::StringPieceReplacements<std::string> action_replacements;
  action_replacements.SetPathStr(kActionEndpoint);
  action_replacements.SetQueryStr(api_key);
  assistant_script_action_server_url_ =
      GURL(kAutofillAssistantServer).ReplaceComponents(action_replacements);
}

AssistantService::~AssistantService() {}

void AssistantService::GetAssistantScriptsForUrl(const GURL& url,
                                                 ResponseCallback callback) {
  DCHECK(url.is_valid());

  std::unique_ptr<AssistantLoader> assistant_loader =
      std::make_unique<AssistantLoader>();
  assistant_loader->callback = std::move(callback);
  assistant_loader->loader =
      CreateAndStartLoader(assistant_script_server_url_,
                           AssistantProtocolUtils::CreateGetScriptsRequest(url),
                           assistant_loader.get());
  assistant_loaders_[assistant_loader.get()] = std::move(assistant_loader);
}

void AssistantService::GetAssistantActions(const std::string& script_path,
                                           ResponseCallback callback) {
  DCHECK(!script_path.empty());

  std::unique_ptr<AssistantLoader> assistant_loader =
      std::make_unique<AssistantLoader>();
  assistant_loader->callback = std::move(callback);
  assistant_loader->loader = CreateAndStartLoader(
      assistant_script_action_server_url_,
      AssistantProtocolUtils::CreateInitialScriptActionsRequest(script_path),
      assistant_loader.get());
  assistant_loaders_[assistant_loader.get()] = std::move(assistant_loader);
}

void AssistantService::GetNextAssistantActions(
    const std::string& previous_server_payload,
    ResponseCallback callback) {
  DCHECK(!previous_server_payload.empty());

  std::unique_ptr<AssistantLoader> assistant_loader =
      std::make_unique<AssistantLoader>();
  assistant_loader->callback = std::move(callback);
  assistant_loader->loader = CreateAndStartLoader(
      assistant_script_action_server_url_,
      AssistantProtocolUtils::CreateNextScriptActionsRequest(
          previous_server_payload),
      assistant_loader.get());
  assistant_loaders_[assistant_loader.get()] = std::move(assistant_loader);
}

AssistantService::AssistantLoader::AssistantLoader() {}
AssistantService::AssistantLoader::~AssistantLoader() {}

std::unique_ptr<network::SimpleURLLoader>
AssistantService::CreateAndStartLoader(const GURL& server_url,
                                       const std::string& request,
                                       AssistantLoader* loader) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = server_url;
  resource_request->method = "POST";
  resource_request->fetch_redirect_mode =
      network::mojom::FetchRedirectMode::kError;
  resource_request->load_flags =
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES;
  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  simple_loader->AttachStringForUpload(request, "application/x-protobuffer");
  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      content::BrowserContext::GetDefaultStoragePartition(context_)
          ->GetURLLoaderFactoryForBrowserProcess()
          .get(),
      base::BindOnce(&AssistantService::OnURLLoaderComplete,
                     base::Unretained(this), loader));
  return simple_loader;
}

void AssistantService::OnURLLoaderComplete(
    AssistantLoader* loader,
    std::unique_ptr<std::string> response_body) {
  auto loader_it = assistant_loaders_.find(loader);
  DCHECK(loader_it != assistant_loaders_.end());
  std::unique_ptr<AssistantLoader> assistant_loader =
      std::move(loader_it->second);
  assistant_loaders_.erase(loader_it);
  DCHECK(assistant_loader);

  int response_code = 0;
  if (assistant_loader->loader->ResponseInfo() &&
      assistant_loader->loader->ResponseInfo()->headers) {
    response_code =
        assistant_loader->loader->ResponseInfo()->headers->response_code();
  }
  std::string response_body_str;
  if (assistant_loader->loader->NetError() != net::OK || response_code != 200) {
    LOG(ERROR) << "Communicating with autofill assistant server error NetError="
               << assistant_loader->loader->NetError()
               << " response_code=" << response_code;
    std::move(assistant_loader->callback).Run(false, response_body_str);
    return;
  }

  if (response_body)
    response_body_str = std::move(*response_body);
  std::move(assistant_loader->callback).Run(true, response_body_str);
}

}  // namespace autofill_assistant
