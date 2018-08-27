// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_proxying_url_loader_factory_manager.h"

#include "base/no_destructor.h"
#include "build/buildflag.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_proxying_url_loader_factory.h"
#include "chrome/browser/signin/header_modification_delegate_impl.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/buildflags/buildflags.h"
#include "google_apis/gaia/gaia_auth_util.h"

using content::BrowserThread;

namespace signin {

// static
ProxyingURLLoaderFactoryManager*
ProxyingURLLoaderFactoryManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<ProxyingURLLoaderFactoryManager*>(
      GetInstance().GetServiceForBrowserContext(profile, true));
}

// static
ProxyingURLLoaderFactoryManagerFactory&
ProxyingURLLoaderFactoryManagerFactory::GetInstance() {
  static base::NoDestructor<ProxyingURLLoaderFactoryManagerFactory> instance;
  return *instance;
}

ProxyingURLLoaderFactoryManagerFactory::ProxyingURLLoaderFactoryManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "ChromeSigninProxyingURLLoaderFactoryManagerFactory",
          BrowserContextDependencyManager::GetInstance()) {}

ProxyingURLLoaderFactoryManagerFactory::
    ~ProxyingURLLoaderFactoryManagerFactory() {
  // The only instance of this class should be owned by base::NoDestructor.
  NOTREACHED();
}

KeyedService* ProxyingURLLoaderFactoryManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  Profile* profile = static_cast<Profile*>(browser_context);
  return new ProxyingURLLoaderFactoryManager(profile);
}

ProxyingURLLoaderFactoryManager::ProxyingURLLoaderFactoryManager(
    Profile* profile)
    : profile_(profile), weak_factory_(this) {}

ProxyingURLLoaderFactoryManager::~ProxyingURLLoaderFactoryManager() = default;

bool ProxyingURLLoaderFactoryManager::MaybeProxyURLLoaderFactory(
    content::RenderFrameHost* render_frame_host,
    bool is_navigation,
    const GURL& url,
    network::mojom::URLLoaderFactoryRequest* factory_request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!profile_->IsOffTheRecord());

  // Navigation requests are handled using signin::URLLoaderThrottle.
  if (is_navigation)
    return false;

  // This proxy should only be installed for subresource requests from a frame
  // that is rendering the GAIA signon realm.
  if (!render_frame_host || !gaia::IsGaiaSignonRealm(url.GetOrigin())) {
    return false;
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Account consistency requires the AccountReconcilor, which is only
  // attached to the main request context.
  // Note: InlineLoginUI uses an isolated request context and thus bypasses
  // the account consistency flow here. See http://crbug.com/428396
  if (extensions::WebViewRendererState::GetInstance()->IsGuest(
          render_frame_host->GetProcess()->GetID())) {
    return false;
  }
#endif

  auto proxied_request = std::move(*factory_request);
  network::mojom::URLLoaderFactoryPtrInfo target_factory_info;
  *factory_request = mojo::MakeRequest(&target_factory_info);

  auto web_contents_getter =
      base::BindRepeating(&content::WebContents::FromFrameTreeNodeId,
                          render_frame_host->GetFrameTreeNodeId());

  std::unique_ptr<ProxyingURLLoaderFactory, BrowserThread::DeleteOnIOThread>
      proxy(new ProxyingURLLoaderFactory());
  auto* proxy_weak = proxy.get();
  auto delegate = std::make_unique<signin::HeaderModificationDelegateImpl>(
      profile_->GetResourceContext());
  proxies_.emplace(std::move(proxy));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(
          &ProxyingURLLoaderFactory::StartProxying,
          base::Unretained(proxy_weak), std::move(delegate),
          std::move(web_contents_getter), std::move(proxied_request),
          std::move(target_factory_info),
          base::BindOnce(&ProxyingURLLoaderFactoryManager::RemoveProxy,
                         weak_factory_.GetWeakPtr(), proxy_weak)));
  return true;
}

void ProxyingURLLoaderFactoryManager::RemoveProxy(
    ProxyingURLLoaderFactory* proxy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = proxies_.find(proxy);
  DCHECK(it != proxies_.end());
  proxies_.erase(it);
}

}  // namespace signin
