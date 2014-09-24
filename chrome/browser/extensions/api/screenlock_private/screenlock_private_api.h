// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SCREENLOCK_PRIVATE_SCREENLOCK_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SCREENLOCK_PRIVATE_SCREENLOCK_PRIVATE_API_H_

#include <string>

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/signin/screenlock_bridge.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

namespace extensions {

class ScreenlockPrivateGetLockedFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("screenlockPrivate.getLocked",
                             SCREENLOCKPRIVATE_GETLOCKED)
  ScreenlockPrivateGetLockedFunction();
  virtual bool RunAsync() OVERRIDE;

 private:
  virtual ~ScreenlockPrivateGetLockedFunction();
  DISALLOW_COPY_AND_ASSIGN(ScreenlockPrivateGetLockedFunction);
};

class ScreenlockPrivateSetLockedFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("screenlockPrivate.setLocked",
                             SCREENLOCKPRIVATE_SETLOCKED)
  ScreenlockPrivateSetLockedFunction();
  virtual bool RunAsync() OVERRIDE;

 private:
  virtual ~ScreenlockPrivateSetLockedFunction();
  DISALLOW_COPY_AND_ASSIGN(ScreenlockPrivateSetLockedFunction);
};

class ScreenlockPrivateAcceptAuthAttemptFunction
    : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("screenlockPrivate.acceptAuthAttempt",
                             SCREENLOCKPRIVATE_ACCEPTAUTHATTEMPT)
  ScreenlockPrivateAcceptAuthAttemptFunction();
  virtual bool RunSync() OVERRIDE;

 private:
  virtual ~ScreenlockPrivateAcceptAuthAttemptFunction();
  DISALLOW_COPY_AND_ASSIGN(ScreenlockPrivateAcceptAuthAttemptFunction);
};

class ScreenlockPrivateEventRouter : public extensions::BrowserContextKeyedAPI,
                                     public ScreenlockBridge::Observer {
 public:
  explicit ScreenlockPrivateEventRouter(content::BrowserContext* context);
  virtual ~ScreenlockPrivateEventRouter();

  bool OnAuthAttempted(ScreenlockBridge::LockHandler::AuthType auth_type,
                       const std::string& value);

  // BrowserContextKeyedAPI
  static extensions::BrowserContextKeyedAPIFactory<
      ScreenlockPrivateEventRouter>*
      GetFactoryInstance();
  virtual void Shutdown() OVERRIDE;

  // ScreenlockBridge::Observer
  virtual void OnScreenDidLock() OVERRIDE;
  virtual void OnScreenDidUnlock() OVERRIDE;
  virtual void OnFocusedUserChanged(const std::string& user_id) OVERRIDE;

 private:
  friend class extensions::BrowserContextKeyedAPIFactory<
      ScreenlockPrivateEventRouter>;

  // BrowserContextKeyedAPI
  static const char* service_name() {
    return "ScreenlockPrivateEventRouter";
  }
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceRedirectedInIncognito = true;

  void DispatchEvent(const std::string& event_name, base::Value* arg);

  content::BrowserContext* browser_context_;
  DISALLOW_COPY_AND_ASSIGN(ScreenlockPrivateEventRouter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SCREENLOCK_PRIVATE_SCREENLOCK_PRIVATE_API_H_
