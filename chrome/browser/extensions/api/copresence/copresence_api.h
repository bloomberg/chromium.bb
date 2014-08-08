// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_COPRESENCE_COPRESENCE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_COPRESENCE_COPRESENCE_API_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/copresence/copresence_translations.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/common/extensions/api/copresence.h"
#include "components/copresence/public/copresence_client_delegate.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

namespace copresence {
class CopresenceClient;
class WhispernetClient;
}

namespace extensions {

class CopresenceService : public BrowserContextKeyedAPI,
                          public copresence::CopresenceClientDelegate {
 public:
  explicit CopresenceService(content::BrowserContext* context);
  virtual ~CopresenceService();

  // BrowserContextKeyedAPI implementation.
  virtual void Shutdown() OVERRIDE;

  // These accessors will always return an object. If the object doesn't exist,
  // they will create one first.
  copresence::CopresenceClient* client();
  copresence::WhispernetClient* whispernet_client();

  // A registry containing the app id's associated with every subscription.
  SubscriptionToAppMap& apps_by_subscription_id() {
    return apps_by_subscription_id_;
  }

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<CopresenceService>* GetFactoryInstance();

 private:
  friend class BrowserContextKeyedAPIFactory<CopresenceService>;

  // CopresenceClientDelegate overrides:
  virtual void HandleMessages(
      const std::string& app_id,
      const std::string& subscription_id,
      const std::vector<copresence::Message>& message) OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContext() const OVERRIDE;
  virtual const std::string GetPlatformVersionString() const OVERRIDE;
  virtual copresence::WhispernetClient* GetWhispernetClient() OVERRIDE;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "CopresenceService"; }

  bool is_shutting_down_;
  std::map<std::string, std::string> apps_by_subscription_id_;

  content::BrowserContext* const browser_context_;
  scoped_ptr<copresence::CopresenceClient> client_;
  scoped_ptr<copresence::WhispernetClient> whispernet_client_;

  DISALLOW_COPY_AND_ASSIGN(CopresenceService);
};

template <>
void BrowserContextKeyedAPIFactory<
    CopresenceService>::DeclareFactoryDependencies();

class CopresenceExecuteFunction : public ChromeUIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("copresence.execute", COPRESENCE_EXECUTE);

 protected:
  virtual ~CopresenceExecuteFunction() {}
  virtual ExtensionFunction::ResponseAction Run() OVERRIDE;

 private:
  void SendResult(copresence::CopresenceStatus status);
};

class CopresenceSetApiKeyFunction : public ChromeUIThreadExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("copresence.setApiKey", COPRESENCE_SETAPIKEY);

 protected:
  virtual ~CopresenceSetApiKeyFunction() {}
  virtual ExtensionFunction::ResponseAction Run() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_COPRESENCE_COPRESENCE_API_H_
