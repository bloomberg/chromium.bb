// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/service.h"

#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/strings/strcat.h"
#include "components/autofill_assistant/browser/protocol_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/url_canon_stdstring.h"

namespace {
// TODO(crbug.com/806868): Provide correct server and endpoint.
const char* const kAutofillAssistantServer = "";
const char* const kScriptEndpoint = "/v1/supportsSite2";
const char* const kActionEndpoint = "/v1/actions2";

net::NetworkTrafficAnnotationTag traffic_annotation =
    net::DefineNetworkTrafficAnnotation("autofill_service", R"(
        semantics {
          sender: "Autofill Assistant"
          description:
            "Chromium posts requests to autofill assistant server to get
            scripts for a URL."
          trigger:
            "Matching URL."
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

namespace switches {
const char* const kAutofillAssistantServerURL = "autofill-assistant-url";
}  // namespace switches

Service::Service(const std::string& api_key, content::BrowserContext* context)
    : context_(context) {
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  GURL service_url(kAutofillAssistantServer);
  if (command_line->HasSwitch(switches::kAutofillAssistantServerURL)) {
    GURL custom_url(command_line->GetSwitchValueASCII(
        switches::kAutofillAssistantServerURL));
    if (custom_url.is_valid()) {
      service_url = custom_url;
    } else {
      LOG(WARNING) << "The following autofill assisstant URL specified in "
                      "the command line is invalid: "
                   << custom_url;
    }
  }

  std::string api_key_query = base::StrCat({"key=", api_key});
  url::StringPieceReplacements<std::string> script_replacements;
  script_replacements.SetPathStr(kScriptEndpoint);
  script_replacements.SetQueryStr(api_key_query);
  script_server_url_ = service_url.ReplaceComponents(script_replacements);

  url::StringPieceReplacements<std::string> action_replacements;
  action_replacements.SetPathStr(kActionEndpoint);
  action_replacements.SetQueryStr(api_key_query);
  script_action_server_url_ =
      service_url.ReplaceComponents(action_replacements);
}

Service::~Service() {}

void Service::GetScriptsForUrl(const GURL& url, ResponseCallback callback) {
  DCHECK(url.is_valid());

  std::unique_ptr<Loader> loader = std::make_unique<Loader>();
  loader->callback = std::move(callback);
  loader->loader = CreateAndStartLoader(
      script_server_url_, ProtocolUtils::CreateGetScriptsRequest(url),
      loader.get());
  loaders_[loader.get()] = std::move(loader);
}

void Service::GetActions(const std::string& script_path,
                         ResponseCallback callback) {
  DCHECK(!script_path.empty());

  std::unique_ptr<Loader> loader = std::make_unique<Loader>();
  loader->callback = std::move(callback);
  loader->loader = CreateAndStartLoader(
      script_action_server_url_,
      ProtocolUtils::CreateInitialScriptActionsRequest(script_path),
      loader.get());
  loaders_[loader.get()] = std::move(loader);
}

void Service::GetNextActions(
    const std::string& previous_server_payload,
    const std::vector<ProcessedActionProto>& processed_actions,
    ResponseCallback callback) {
  DCHECK(!previous_server_payload.empty());

  std::unique_ptr<Loader> loader = std::make_unique<Loader>();
  loader->callback = std::move(callback);
  loader->loader =
      CreateAndStartLoader(script_action_server_url_,
                           ProtocolUtils::CreateNextScriptActionsRequest(
                               previous_server_payload, processed_actions),
                           loader.get());
  loaders_[loader.get()] = std::move(loader);
}

Service::Loader::Loader() {}
Service::Loader::~Loader() {}

std::unique_ptr<::network::SimpleURLLoader> Service::CreateAndStartLoader(
    const GURL& server_url,
    const std::string& request,
    Loader* loader) {
  auto resource_request = std::make_unique<::network::ResourceRequest>();
  resource_request->url = server_url;
  resource_request->method = "POST";
  resource_request->fetch_redirect_mode =
      ::network::mojom::FetchRedirectMode::kError;
  resource_request->load_flags =
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES;
  std::unique_ptr<::network::SimpleURLLoader> simple_loader =
      ::network::SimpleURLLoader::Create(std::move(resource_request),
                                         traffic_annotation);
  simple_loader->AttachStringForUpload(request, "application/x-protobuffer");
#ifdef DEBUG
  simple_loader->SetAllowHttpErrorResults(true);
#endif
  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      content::BrowserContext::GetDefaultStoragePartition(context_)
          ->GetURLLoaderFactoryForBrowserProcess()
          .get(),
      base::BindOnce(&Service::OnURLLoaderComplete, base::Unretained(this),
                     loader));
  return simple_loader;
}

void Service::OnURLLoaderComplete(Loader* loader,
                                  std::unique_ptr<std::string> response_body) {
  auto loader_it = loaders_.find(loader);
  DCHECK(loader_it != loaders_.end());
  std::unique_ptr<Loader> loader_instance = std::move(loader_it->second);
  loaders_.erase(loader_it);
  DCHECK(loader_instance);

  int response_code = 0;
  if (loader_instance->loader->ResponseInfo() &&
      loader_instance->loader->ResponseInfo()->headers) {
    response_code =
        loader_instance->loader->ResponseInfo()->headers->response_code();
  }
  std::string response_body_str;
  if (loader_instance->loader->NetError() != net::OK || response_code != 200) {
    LOG(ERROR) << "Communicating with autofill assistant server error NetError="
               << loader_instance->loader->NetError()
               << " response_code=" << response_code << " message="
               << (response_body == nullptr ? "" : *response_body);
    std::move(loader_instance->callback).Run(false, response_body_str);
    return;
  }

  if (response_body)
    response_body_str = std::move(*response_body);
  std::move(loader_instance->callback).Run(true, response_body_str);
}

}  // namespace autofill_assistant
