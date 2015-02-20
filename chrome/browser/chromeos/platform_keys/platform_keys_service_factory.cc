// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/platform_keys_service_factory.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/ui/platform_keys_certificate_selector_chromeos.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "net/cert/x509_certificate.h"

namespace chromeos {
namespace {

// This SelectDelegate always selects no certificate.
class NoOpSelectDelegate
    : public chromeos::PlatformKeysService::SelectDelegate {
 public:
  NoOpSelectDelegate() {}

  void Select(const std::string& extension_id,
              const net::CertificateList& certs,
              const CertificateSelectedCallback& callback,
              content::WebContents* web_contents,
              content::BrowserContext* context) override {
    callback.Run(nullptr);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NoOpSelectDelegate);
};

// This delegate selects a certificate by showing the certificate selection
// dialog to the user.
class DefaultSelectDelegate
    : public chromeos::PlatformKeysService::SelectDelegate {
 public:
  DefaultSelectDelegate() : weak_factory_(this) {}
  ~DefaultSelectDelegate() override {}

  void Select(const std::string& extension_id,
              const net::CertificateList& certs,
              const CertificateSelectedCallback& callback,
              content::WebContents* web_contents,
              content::BrowserContext* context) override {
    CHECK(web_contents);
    const extensions::Extension* const extension =
        extensions::ExtensionRegistry::Get(context)->GetExtensionById(
            extension_id, extensions::ExtensionRegistry::ENABLED);
    if (!extension) {
      callback.Run(nullptr /* no certificate selected */);
      return;
    }
    ShowPlatformKeysCertificateSelector(
        web_contents, extension->short_name(), certs,
        // Don't call |callback| once this delegate is destructed, thus use a
        // WeakPtr.
        base::Bind(&DefaultSelectDelegate::SelectedCertificate,
                   weak_factory_.GetWeakPtr(), callback));
  }

  void SelectedCertificate(
      const CertificateSelectedCallback& callback,
      const scoped_refptr<net::X509Certificate>& selected_cert) {
    callback.Run(selected_cert);
  }

 private:
  base::WeakPtrFactory<DefaultSelectDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DefaultSelectDelegate);
};

}  // namespace

// static
PlatformKeysService* PlatformKeysServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<PlatformKeysService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
PlatformKeysServiceFactory* PlatformKeysServiceFactory::GetInstance() {
  return Singleton<PlatformKeysServiceFactory>::get();
}

PlatformKeysServiceFactory::PlatformKeysServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PlatformKeysService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
}

PlatformKeysServiceFactory::~PlatformKeysServiceFactory() {
}

content::BrowserContext* PlatformKeysServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

KeyedService* PlatformKeysServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  extensions::StateStore* const store =
      extensions::ExtensionSystem::Get(context)->state_store();
  DCHECK(store);
  PlatformKeysService* const service = new PlatformKeysService(context, store);

  policy::ProfilePolicyConnector* const connector =
      policy::ProfilePolicyConnectorFactory::GetForBrowserContext(context);
  // Only allow the user to grant certificate permissions to extensions if the
  // user is not managed by policy. Otherwise the user might leak access to
  // (private keys of) certificates against the intentions of the administrator.
  // TODO(pneubeck): Remove this once the respective policy is implemented.
  //   https://crbug.com/460232
  if (connector->IsManaged())
    service->SetSelectDelegate(make_scoped_ptr(new NoOpSelectDelegate()));
  else
    service->SetSelectDelegate(make_scoped_ptr(new DefaultSelectDelegate()));
  return service;
}

}  // namespace chromeos
