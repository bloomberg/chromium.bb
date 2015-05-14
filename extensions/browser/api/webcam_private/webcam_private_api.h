// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_WEBCAM_PRIVATE_API_H_
#define EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_WEBCAM_PRIVATE_API_H_

#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/scoped_observer.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/process_manager_observer.h"

class Profile;

namespace extensions {

class ProcessManager;
class Webcam;

class WebcamPrivateAPI : public BrowserContextKeyedAPI,
                         public ProcessManagerObserver {
 public:
  static BrowserContextKeyedAPIFactory<WebcamPrivateAPI>* GetFactoryInstance();

  // Convenience method to get the WebcamPrivateAPI for a BrowserContext.
  static WebcamPrivateAPI* Get(content::BrowserContext* context);

  explicit WebcamPrivateAPI(content::BrowserContext* context);
  ~WebcamPrivateAPI() override;

  Webcam* GetWebcam(const std::string& extension_id,
                    const std::string& webcam_id);

 private:
  friend class BrowserContextKeyedAPIFactory<WebcamPrivateAPI>;

  bool GetDeviceId(const std::string& extension_id,
                   const std::string& webcam_id,
                   std::string* device_id);

  // ProcessManagerObserver:
  void OnBackgroundHostClose(const std::string& extension_id) override;

  // BrowserContextKeyedAPI:
  static const char* service_name() {
    return "WebcamPrivateAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  content::BrowserContext* const browser_context_;
  ScopedObserver<ProcessManager, ProcessManagerObserver>
      process_manager_observer_;

  std::map<std::string, linked_ptr<Webcam>> webcams_;

  base::WeakPtrFactory<WebcamPrivateAPI> weak_ptr_factory_;
};

template <>
void BrowserContextKeyedAPIFactory<WebcamPrivateAPI>
    ::DeclareFactoryDependencies();

class WebcamPrivateSetFunction : public SyncExtensionFunction {
 public:
  WebcamPrivateSetFunction();
  DECLARE_EXTENSION_FUNCTION("webcamPrivate.set", WEBCAMPRIVATE_SET);

 protected:
  ~WebcamPrivateSetFunction() override;
  bool RunSync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebcamPrivateSetFunction);
};

class WebcamPrivateGetFunction : public SyncExtensionFunction {
 public:
  WebcamPrivateGetFunction();
  DECLARE_EXTENSION_FUNCTION("webcamPrivate.get", WEBCAMPRIVATE_GET);

 protected:
  ~WebcamPrivateGetFunction() override;
  bool RunSync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebcamPrivateGetFunction);
};

class WebcamPrivateResetFunction : public SyncExtensionFunction {
 public:
  WebcamPrivateResetFunction();
  DECLARE_EXTENSION_FUNCTION("webcamPrivate.reset", WEBCAMPRIVATE_RESET);

 protected:
  ~WebcamPrivateResetFunction() override;
  bool RunSync() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebcamPrivateResetFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEBCAM_PRIVATE_WEBCAM_PRIVATE_API_H_
