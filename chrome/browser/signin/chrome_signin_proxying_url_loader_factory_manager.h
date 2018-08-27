// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_PROXYING_URL_LOADER_FACTORY_MANAGER_H_
#define CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_PROXYING_URL_LOADER_FACTORY_MANAGER_H_

#include "base/containers/unique_ptr_adapters.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

class Profile;

namespace content {
class BrowserContext;
class RenderFrameHost;
}  // namespace content

namespace signin {

class ProxyingURLLoaderFactory;
class ProxyingURLLoaderFactoryManager;

class ProxyingURLLoaderFactoryManagerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  ProxyingURLLoaderFactoryManagerFactory();
  ~ProxyingURLLoaderFactoryManagerFactory() override;

  // Returns the instance of ProxyingURLLoaderFactoryManager associated with
  // this profile (creating one if none exists). Returns nullptr if this profile
  // cannot have a ProxyingURLLoaderFactoryManager (for example, if it is
  // incognito).
  static ProxyingURLLoaderFactoryManager* GetForProfile(Profile* profile);

  // Returns the instance of this factory singleton.
  static ProxyingURLLoaderFactoryManagerFactory& GetInstance();

 private:
  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser_context) const override;

  DISALLOW_COPY_AND_ASSIGN(ProxyingURLLoaderFactoryManagerFactory);
};

// This class owns all of the ProxyingURLLoaderFactory instances associated
// with a particular Profile. This object lives on the UI thread while the
// objects it manages live on the IO thread.
class ProxyingURLLoaderFactoryManager : public KeyedService {
 public:
  explicit ProxyingURLLoaderFactoryManager(Profile* profile);
  ~ProxyingURLLoaderFactoryManager() override;

  // Called when a renderer needs a URLLoaderFactory to give this module the
  // opportunity to install a proxy. This is only done when
  // https://accounts.google.com is loaded in non-incognito mode. Returns true
  // when |factory_request| has been proxied.
  bool MaybeProxyURLLoaderFactory(
      content::RenderFrameHost* render_frame_host,
      bool is_navigation,
      const GURL& url,
      network::mojom::URLLoaderFactoryRequest* factory_request);

 private:
  void RemoveProxy(ProxyingURLLoaderFactory* proxy);

  Profile* const profile_;
  std::set<std::unique_ptr<ProxyingURLLoaderFactory,
                           content::BrowserThread::DeleteOnIOThread>,
           base::UniquePtrComparator>
      proxies_;

  base::WeakPtrFactory<ProxyingURLLoaderFactoryManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProxyingURLLoaderFactoryManager);
};

}  // namespace signin

#endif  // CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_PROXYING_URL_LOADER_FACTORY_MANAGER_H_
