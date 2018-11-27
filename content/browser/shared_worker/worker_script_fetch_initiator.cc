// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_worker/worker_script_fetch_initiator.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/field_trial_params.h"
#include "base/task/post_task.h"
#include "content/browser/appcache/appcache_navigation_handle.h"
#include "content/browser/appcache/appcache_navigation_handle_core.h"
#include "content/browser/file_url_loader_factory.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/shared_worker/worker_script_fetcher.h"
#include "content/browser/shared_worker/worker_script_loader.h"
#include "content/browser/shared_worker/worker_script_loader_factory.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_constants_internal.h"
#include "content/common/navigation_subresource_loader_params.h"
#include "content/common/service_worker/service_worker_provider.mojom.h"
#include "content/common/url_loader_factory_bundle.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/renderer_preferences.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/network/loader_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"
#include "url/origin.h"

namespace content {

void WorkerScriptFetchInitiator::Start(
    int process_id,
    const GURL& script_url,
    const url::Origin& request_initiator,
    ResourceType resource_type,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    AppCacheNavigationHandleCore* appcache_handle_core,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    StoragePartitionImpl* storage_partition,
    CompletionCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(blink::ServiceWorkerUtils::IsServicificationEnabled());
  DCHECK(storage_partition);

  // TODO(nhiroki): Support DedicatedWorker (https://crbug.com/906991).
  DCHECK_EQ(RESOURCE_TYPE_SHARED_WORKER, resource_type);

  bool constructor_uses_file_url =
      request_initiator.scheme() == url::kFileScheme;

  // Set up the factory bundle for non-NetworkService URLs, e.g.,
  // chrome-extension:// URLs. One factory bundle is consumed by the browser
  // for WorkerScriptLoaderFactory, and one is sent to the renderer for
  // subresource loading.
  std::unique_ptr<URLLoaderFactoryBundleInfo> factory_bundle_for_browser =
      CreateFactoryBundle(process_id, storage_partition,
                          constructor_uses_file_url);
  std::unique_ptr<URLLoaderFactoryBundleInfo> subresource_loader_factories =
      CreateFactoryBundle(process_id, storage_partition,
                          constructor_uses_file_url);

  // NetworkService (PlzWorker):
  // Create a resource request for initiating shared worker script fetch from
  // the browser process.
  std::unique_ptr<network::ResourceRequest> resource_request;
  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    // TODO(nhiroki): Populate more fields like referrer.
    // (https://crbug.com/715632)
    resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = script_url;
    resource_request->request_initiator = request_initiator;
    resource_request->resource_type = resource_type;

    AddAdditionalRequestHeaders(resource_request.get(),
                                storage_partition->browser_context());
  }

  // Bounce to the IO thread to setup service worker and appcache support in
  // case the request for the worker script will need to be intercepted by them.
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &WorkerScriptFetchInitiator::CreateScriptLoaderOnIO, process_id,
          std::move(resource_request),
          storage_partition->url_loader_factory_getter(),
          std::move(factory_bundle_for_browser),
          std::move(subresource_loader_factories),
          std::move(service_worker_context), appcache_handle_core,
          blob_url_loader_factory ? blob_url_loader_factory->Clone() : nullptr,
          std::move(callback)));
}

std::unique_ptr<URLLoaderFactoryBundleInfo>
WorkerScriptFetchInitiator::CreateFactoryBundle(
    int process_id,
    StoragePartitionImpl* storage_partition,
    bool file_support) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(blink::ServiceWorkerUtils::IsServicificationEnabled());

  ContentBrowserClient::NonNetworkURLLoaderFactoryMap non_network_factories;
  GetContentClient()
      ->browser()
      ->RegisterNonNetworkSubresourceURLLoaderFactories(
          process_id, MSG_ROUTING_NONE, &non_network_factories);

  auto factory_bundle = std::make_unique<URLLoaderFactoryBundleInfo>();
  for (auto& pair : non_network_factories) {
    const std::string& scheme = pair.first;
    std::unique_ptr<network::mojom::URLLoaderFactory> factory =
        std::move(pair.second);

    network::mojom::URLLoaderFactoryPtr factory_ptr;
    mojo::MakeStrongBinding(std::move(factory),
                            mojo::MakeRequest(&factory_ptr));
    factory_bundle->scheme_specific_factory_infos().emplace(
        scheme, factory_ptr.PassInterface());
  }

  if (file_support) {
    auto file_factory = std::make_unique<FileURLLoaderFactory>(
        storage_partition->browser_context()->GetPath(),
        base::CreateSequencedTaskRunnerWithTraits(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));
    network::mojom::URLLoaderFactoryPtr file_factory_ptr;
    mojo::MakeStrongBinding(std::move(file_factory),
                            mojo::MakeRequest(&file_factory_ptr));
    factory_bundle->scheme_specific_factory_infos().emplace(
        url::kFileScheme, file_factory_ptr.PassInterface());
  }

  // Use RenderProcessHost's network factory as the default factory if
  // NetworkService is off. If NetworkService is on the default factory is
  // set in CreateScriptLoaderOnIO().
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    // Using an opaque origin here should be safe - the URLLoaderFactory created
    // for such origin shouldn't have any special privileges.  Additionally, the
    // origin should not be inspected at all in the legacy, non-NetworkService
    // path.
    const url::Origin kSafeOrigin = url::Origin();

    network::mojom::URLLoaderFactoryPtr default_factory;
    RenderProcessHost::FromID(process_id)
        ->CreateURLLoaderFactory(kSafeOrigin, nullptr /* header_client */,
                                 mojo::MakeRequest(&default_factory));
    factory_bundle->default_factory_info() = default_factory.PassInterface();
  }

  return factory_bundle;
}

// TODO(nhiroki): Align this function with AddAdditionalRequestHeaders() in
// navigation_request.cc, FrameFetchContext, and WorkerFetchContext.
void WorkerScriptFetchInitiator::AddAdditionalRequestHeaders(
    network::ResourceRequest* resource_request,
    BrowserContext* browser_context) {
  DCHECK(base::FeatureList::IsEnabled(network::features::kNetworkService));

  // TODO(nhiroki): Return early when the request is neither HTTP nor HTTPS
  // (i.e., Blob URL or Data URL). This should be checked by
  // SchemeIsHTTPOrHTTPS(), but currently cross-origin workers on extensions
  // are allowed and the check doesn't work well. See https://crbug.com/867302.

  // Set the "Accept" header.
  resource_request->headers.SetHeaderIfMissing(network::kAcceptHeader,
                                               network::kDefaultAcceptHeader);

  // Set the "DNT" header if necessary.
  RendererPreferences renderer_preferences;
  GetContentClient()->browser()->UpdateRendererPreferencesForWorker(
      browser_context, &renderer_preferences);
  if (renderer_preferences.enable_do_not_track)
    resource_request->headers.SetHeaderIfMissing(kDoNotTrackHeader, "1");

  // Set the "Save-Data" header if necessary.
  if (GetContentClient()->browser()->IsDataSaverEnabled(browser_context) &&
      !base::GetFieldTrialParamByFeatureAsBool(features::kDataSaverHoldback,
                                               "holdback_web", false)) {
    resource_request->headers.SetHeaderIfMissing("Save-Data", "on");
  }

  // Set the "Sec-Metadata" header if necessary.
  if (base::FeatureList::IsEnabled(features::kSecMetadata) ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExperimentalWebPlatformFeatures)) {
    // The worker's origin can be different from the constructor's origin, for
    // example, when the worker created from the extension.
    // TODO(hiroshige): Add DCHECK to make sure the same-originness once the
    // cross-origin workers are deprecated (https://crbug.com/867302).
    std::string site_value = "cross-site";
    if (resource_request->request_initiator->IsSameOriginWith(
            url::Origin::Create(resource_request->url))) {
      site_value = "same-origin";
    }
    std::string value = base::StringPrintf("destination=sharedworker, site=%s",
                                           site_value.c_str());
    resource_request->headers.SetHeaderIfMissing("Sec-Metadata", value);
  }
}

void WorkerScriptFetchInitiator::CreateScriptLoaderOnIO(
    int process_id,
    std::unique_ptr<network::ResourceRequest> resource_request,
    scoped_refptr<URLLoaderFactoryGetter> loader_factory_getter,
    std::unique_ptr<URLLoaderFactoryBundleInfo> factory_bundle_for_browser_info,
    std::unique_ptr<URLLoaderFactoryBundleInfo> subresource_loader_factories,
    scoped_refptr<ServiceWorkerContextWrapper> context,
    AppCacheNavigationHandleCore* appcache_handle_core,
    std::unique_ptr<network::SharedURLLoaderFactoryInfo>
        blob_url_loader_factory_info,
    CompletionCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(blink::ServiceWorkerUtils::IsServicificationEnabled());

  // Set up for service worker.
  auto provider_info = mojom::ServiceWorkerProviderInfoForSharedWorker::New();
  base::WeakPtr<ServiceWorkerProviderHost> host =
      context->PreCreateHostForSharedWorker(process_id, &provider_info);

  // Create the URL loader factory for WorkerScriptLoaderFactory to use to load
  // the main script.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
  if (blob_url_loader_factory_info) {
    // If we have a blob_url_loader_factory just use that directly rather than
    // creating a new URLLoaderFactoryBundle.
    url_loader_factory = network::SharedURLLoaderFactory::Create(
        std::move(blob_url_loader_factory_info));
  } else {
    // Create a factory bundle to use.
    scoped_refptr<URLLoaderFactoryBundle> factory_bundle =
        base::MakeRefCounted<URLLoaderFactoryBundle>(
            std::move(factory_bundle_for_browser_info));

    // Add the default factory to the bundle for browser if NetworkService
    // is on. When NetworkService is off, we already created the default factory
    // in CreateFactoryBundle().
    if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
      // Get the direct network factory from |loader_factory_getter|. When
      // NetworkService is enabled, it returns a factory that doesn't support
      // reconnection to the network service after a crash, but it's OK since
      // it's used for a single shared worker startup.
      network::mojom::URLLoaderFactoryPtr network_factory_ptr;
      loader_factory_getter->CloneNetworkFactory(
          mojo::MakeRequest(&network_factory_ptr));
      factory_bundle->SetDefaultFactory(std::move(network_factory_ptr));
    }

    url_loader_factory = factory_bundle;
  }

  // It's safe for |appcache_handle_core| to be a raw pointer. The core is owned
  // by AppCacheNavigationHandle on the UI thread, which posts a task to delete
  // the core on the IO thread on destruction, which must happen after this
  // task.
  base::WeakPtr<AppCacheHost> appcache_host =
      appcache_handle_core ? appcache_handle_core->host()->GetWeakPtr()
                           : nullptr;

  // NetworkService (PlzWorker):
  // Start loading a shared worker main script.
  if (base::FeatureList::IsEnabled(network::features::kNetworkService)) {
    // TODO(nhiroki): Figure out what we should do in |wc_getter| for loading
    // shared worker's main script.
    base::RepeatingCallback<WebContents*()> wc_getter =
        base::BindRepeating([]() -> WebContents* { return nullptr; });
    std::vector<std::unique_ptr<URLLoaderThrottle>> throttles =
        GetContentClient()->browser()->CreateURLLoaderThrottles(
            *resource_request, context->resource_context(), wc_getter,
            nullptr /* navigation_ui_data */, -1 /* frame_tree_node_id */);

    WorkerScriptFetcher::CreateAndStart(
        std::make_unique<WorkerScriptLoaderFactory>(
            process_id, host, std::move(appcache_host),
            context->resource_context(), std::move(url_loader_factory)),
        std::move(throttles), std::move(resource_request),
        base::BindOnce(WorkerScriptFetchInitiator::DidCreateScriptLoaderOnIO,
                       std::move(callback), std::move(provider_info),
                       nullptr /* main_script_loader_factory */,
                       std::move(subresource_loader_factories)));
    return;
  }

  // Create the WorkerScriptLoaderFactory.
  network::mojom::URLLoaderFactoryAssociatedPtrInfo main_script_loader_factory;
  mojo::MakeStrongAssociatedBinding(
      std::make_unique<WorkerScriptLoaderFactory>(
          process_id, host->AsWeakPtr(), std::move(appcache_host),
          context->resource_context(), std::move(url_loader_factory)),
      mojo::MakeRequest(&main_script_loader_factory));

  DidCreateScriptLoaderOnIO(std::move(callback), std::move(provider_info),
                            std::move(main_script_loader_factory),
                            std::move(subresource_loader_factories),
                            nullptr /* main_script_load_params */,
                            base::nullopt /* subresource_loader_params */,
                            true /* success */);
}

void WorkerScriptFetchInitiator::DidCreateScriptLoaderOnIO(
    CompletionCallback callback,
    mojom::ServiceWorkerProviderInfoForSharedWorkerPtr
        service_worker_provider_info,
    network::mojom::URLLoaderFactoryAssociatedPtrInfo
        main_script_loader_factory,
    std::unique_ptr<URLLoaderFactoryBundleInfo> subresource_loader_factories,
    blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
    base::Optional<SubresourceLoaderParams> subresource_loader_params,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(std::move(callback),
                     std::move(service_worker_provider_info),
                     std::move(main_script_loader_factory),
                     std::move(subresource_loader_factories),
                     std::move(main_script_load_params),
                     std::move(subresource_loader_params), success));
}

}  // namespace content
