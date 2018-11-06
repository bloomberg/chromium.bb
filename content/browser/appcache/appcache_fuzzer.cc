// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "content/browser/appcache/appcache_dispatcher_host.h"
#include "content/browser/appcache/appcache_fuzzer.pb.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_content_client.h"
#include "mojo/core/embedder/embedder.h"
#include "services/network/public/cpp/features.h"
#include "services/network/test/test_url_loader_factory.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"

namespace content {

namespace {

struct Env {
  Env()
      : scoped_task_environment(
            base::test::ScopedTaskEnvironment::MainThreadType::IO),
        thread_bundle(TestBrowserThreadBundle::Options::IO_MAINLOOP) {
    base::CommandLine::Init(0, nullptr);
    logging::SetMinLogLevel(logging::LOG_FATAL);
    mojo::core::Init();
    feature_list.InitWithFeatures({network::features::kNetworkService}, {});
    test_content_client = std::make_unique<TestContentClient>();
    test_content_browser_client = std::make_unique<TestContentBrowserClient>();
    SetContentClient(test_content_client.get());
    SetBrowserClientForTesting(test_content_browser_client.get());
    CHECK(base::i18n::InitializeICU());
  }

  void InitializeAppCacheService(
      network::TestURLLoaderFactory* mock_url_loader_factory) {
    appcache_service = new ChromeAppCacheService(/*proxy=*/nullptr);

    scoped_refptr<URLLoaderFactoryGetter> loader_factory_getter =
        base::MakeRefCounted<URLLoaderFactoryGetter>();
    loader_factory_getter->SetNetworkFactoryForTesting(mock_url_loader_factory);
    appcache_service->set_url_loader_factory_getter(
        loader_factory_getter.get());

    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&ChromeAppCacheService::InitializeOnIOThread,
                       appcache_service, base::FilePath(),
                       /*resource_context=*/nullptr,
                       /*request_context_getter=*/nullptr,
                       /*special_storage_policy=*/nullptr));
    scoped_task_environment.RunUntilIdle();
  }

  base::test::ScopedTaskEnvironment scoped_task_environment;
  TestBrowserThreadBundle thread_bundle;
  base::test::ScopedFeatureList feature_list;
  scoped_refptr<ChromeAppCacheService> appcache_service;
  std::unique_ptr<TestContentClient> test_content_client;
  std::unique_ptr<TestContentBrowserClient> test_content_browser_client;

  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

Env& SingletonEnv() {
  static base::NoDestructor<Env> env;
  return *env;
}

void HeadersToRaw(std::string* headers) {
  std::replace(headers->begin(), headers->end(), '\n', '\0');
  if (!headers->empty())
    headers += '\0';
}

std::string GetUrl(const fuzzing::proto::Url& url) {
  if (url.url_test_case_idx() == fuzzing::proto::EMPTY) {
    return "";
  }

  return "http://localhost/" + std::to_string(url.url_test_case_idx());
}

std::string GetManifest(
    const fuzzing::proto::ManifestResponse& manifest_response) {
  std::string response = "CACHE MANIFEST\n";
  for (const fuzzing::proto::Url& url : manifest_response.urls()) {
    response += GetUrl(url) + "\n";
  }
  return response;
}

void DoRequest(network::TestURLLoaderFactory* factory,
               const fuzzing::proto::Url& url,
               uint32_t code,
               bool do_not_cache,
               const fuzzing::proto::ManifestResponse& manifest_response) {
  network::URLLoaderCompletionStatus status;
  status.error_code = net::OK;

  network::ResourceResponseHead response;

  std::string headers = "HTTP/1.1 ";
  headers += std::to_string(code);
  headers += "\n";

  if (do_not_cache) {
    headers += "Cache-Control: no-cache, no-store, must-revalidate\n";
    headers += "Pragma: no-cache\n";
    headers += "Expires: 0\n";
    headers += "DNT: 1\n";
  }
  HeadersToRaw(&headers);

  response.headers = new net::HttpResponseHeaders(headers);

  // To simplify the fuzzer, we respond to all requests with a manifest.
  // When we're performing a manifest fetch, this data will affect the
  // control flow of the appcache code. Otherwise it's just treated like
  // a blob of data.
  std::string content = GetManifest(manifest_response);
  content += "\n# ";

  factory->SimulateResponseForPendingRequest(
      GURL(GetUrl(url)), status, response, content,
      network::TestURLLoaderFactory::kUrlMatchPrefix);
}

}  // anonymous namespace

DEFINE_BINARY_PROTO_FUZZER(const fuzzing::proto::Session& session) {
  network::TestURLLoaderFactory mock_url_loader_factory;
  SingletonEnv().InitializeAppCacheService(&mock_url_loader_factory);

  // Create a context for mojo::ReportBadMessage.
  mojo::Message message;
  auto dispatch_context =
        std::make_unique<mojo::internal::MessageDispatchContext>(&message);

  mojom::AppCacheBackendPtr host;
  AppCacheDispatcherHost::Create(SingletonEnv().appcache_service.get(),
                                 /*process_id=*/1, mojo::MakeRequest(&host));

  for (const fuzzing::proto::Command& command : session.commands()) {
    switch (command.command_case()) {
      case fuzzing::proto::Command::kRegisterHost: {
        int32_t host_id = command.register_host().host_id();
        host->RegisterHost(host_id);
        break;
      }
      case fuzzing::proto::Command::kUnregisterHost: {
        int32_t host_id = command.unregister_host().host_id();
        host->UnregisterHost(host_id);
        break;
      }
      case fuzzing::proto::Command::kSelectCache: {
        int32_t host_id = command.select_cache().host_id();
        int32_t from_id = command.select_cache().from_id();
        GURL document_url = GURL(GetUrl(command.select_cache().document_url()));
        GURL opt_manifest_url =
            GURL(GetUrl(command.select_cache().opt_manifest_url()));
        host->SelectCache(host_id, document_url, from_id, opt_manifest_url);
        break;
      }
      case fuzzing::proto::Command::kSetSpawningHostId: {
        int32_t host_id = command.set_spawning_host_id().host_id();
        int32_t spawning_host_id =
            command.set_spawning_host_id().spawning_host_id();
        host->SetSpawningHostId(host_id, spawning_host_id);
        break;
      }
      case fuzzing::proto::Command::kSelectCacheForSharedWorker: {
        int32_t host_id = command.select_cache_for_shared_worker().host_id();
        int64_t cache_document_was_loaded_from =
            command.select_cache_for_shared_worker()
                .cache_document_was_loaded_from();
        host->SelectCacheForSharedWorker(host_id,
                                         cache_document_was_loaded_from);
        break;
      }
      case fuzzing::proto::Command::kMarkAsForeignEntry: {
        int32_t host_id = command.mark_as_foreign_entry().host_id();
        GURL url(GetUrl(command.mark_as_foreign_entry().document_url()));
        int64_t cache_document_was_loaded_from =
            command.mark_as_foreign_entry().cache_document_was_loaded_from();
        host->MarkAsForeignEntry(host_id, url, cache_document_was_loaded_from);
        break;
      }
      case fuzzing::proto::Command::kGetStatus: {
        int32_t host_id = command.get_status().host_id();
        host->GetStatus(host_id, base::DoNothing());
        break;
      }
      case fuzzing::proto::Command::kStartUpdate: {
        int32_t host_id = command.start_update().host_id();
        host->StartUpdate(host_id, base::DoNothing());
        break;
      }
      case fuzzing::proto::Command::kSwapCache: {
        int32_t host_id = command.swap_cache().host_id();
        host->SwapCache(host_id, base::DoNothing());
        break;
      }
      case fuzzing::proto::Command::kGetResourceList: {
        int32_t host_id = command.get_resource_list().host_id();
        host->GetResourceList(host_id, base::DoNothing());
        break;
      }
      case fuzzing::proto::Command::kDoRequest: {
        uint32_t code = command.do_request().http_code();
        bool do_not_cache = command.do_request().do_not_cache();
        const fuzzing::proto::ManifestResponse& manifest_response =
            command.do_request().manifest_response();
        DoRequest(&mock_url_loader_factory, command.do_request().url(), code,
                  do_not_cache, manifest_response);
        break;
      }
      case fuzzing::proto::Command::kRunUntilIdle: {
        SingletonEnv().scoped_task_environment.RunUntilIdle();
        break;
      }
      case fuzzing::proto::Command::COMMAND_NOT_SET: {
        break;
      }
    }
  }

  host.reset();
  // TODO(nedwilliamson): Investigate removing this or reinitializing
  // the appcache service as a fuzzer command.
  SingletonEnv().scoped_task_environment.RunUntilIdle();
}

}  // namespace content
