// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SCREENLOCK_PRIVATE_SCREENLOCK_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SCREENLOCK_PRIVATE_SCREENLOCK_PRIVATE_API_H_

#include <string>

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/signin/screenlock_bridge.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

namespace gfx {
class Image;
}

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

class ScreenlockPrivateShowMessageFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("screenlockPrivate.showMessage",
                             SCREENLOCKPRIVATE_SHOWMESSAGE)
  ScreenlockPrivateShowMessageFunction();
  virtual bool RunAsync() OVERRIDE;

 private:
  virtual ~ScreenlockPrivateShowMessageFunction();
  DISALLOW_COPY_AND_ASSIGN(ScreenlockPrivateShowMessageFunction);
};

class ScreenlockPrivateShowCustomIconFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("screenlockPrivate.showCustomIcon",
                             SCREENLOCKPRIVATE_SHOWCUSTOMICON)
  ScreenlockPrivateShowCustomIconFunction();
  virtual bool RunAsync() OVERRIDE;

 private:
  virtual ~ScreenlockPrivateShowCustomIconFunction();
  void OnImageLoaded(const gfx::Image& image);
  DISALLOW_COPY_AND_ASSIGN(ScreenlockPrivateShowCustomIconFunction);
};

class ScreenlockPrivateHideCustomIconFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("screenlockPrivate.hideCustomIcon",
                             SCREENLOCKPRIVATE_HIDECUSTOMICON)
  ScreenlockPrivateHideCustomIconFunction();
  virtual bool RunAsync() OVERRIDE;

 private:
  virtual ~ScreenlockPrivateHideCustomIconFunction();
  void OnImageLoaded(const gfx::Image& image);
  DISALLOW_COPY_AND_ASSIGN(ScreenlockPrivateHideCustomIconFunction);
};

class ScreenlockPrivateSetAuthTypeFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("screenlockPrivate.setAuthType",
                             SCREENLOCKPRIVATE_SETAUTHTYPE)
  ScreenlockPrivateSetAuthTypeFunction();
  virtual bool RunAsync() OVERRIDE;

 private:
  virtual ~ScreenlockPrivateSetAuthTypeFunction();
  DISALLOW_COPY_AND_ASSIGN(ScreenlockPrivateSetAuthTypeFunction);
};

class ScreenlockPrivateGetAuthTypeFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("screenlockPrivate.getAuthType",
                             SCREENLOCKPRIVATE_GETAUTHTYPE)
  ScreenlockPrivateGetAuthTypeFunction();
  virtual bool RunAsync() OVERRIDE;

 private:
  virtual ~ScreenlockPrivateGetAuthTypeFunction();
  DISALLOW_COPY_AND_ASSIGN(ScreenlockPrivateGetAuthTypeFunction);
};

class ScreenlockPrivateAcceptAuthAttemptFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("screenlockPrivate.acceptAuthAttempt",
                             SCREENLOCKPRIVATE_ACCEPTAUTHATTEMPT)
  ScreenlockPrivateAcceptAuthAttemptFunction();
  virtual bool RunAsync() OVERRIDE;

 private:
  virtual ~ScreenlockPrivateAcceptAuthAttemptFunction();
  DISALLOW_COPY_AND_ASSIGN(ScreenlockPrivateAcceptAuthAttemptFunction);
};

class ScreenlockPrivateEventRouter : public extensions::BrowserContextKeyedAPI,
                                     public ScreenlockBridge::Observer {
 public:
  explicit ScreenlockPrivateEventRouter(content::BrowserContext* context);
  virtual ~ScreenlockPrivateEventRouter();

  void OnAuthAttempted(ScreenlockBridge::LockHandler::AuthType auth_type,
                       const std::string& value);

  // BrowserContextKeyedAPI
  static extensions::BrowserContextKeyedAPIFactory<
      ScreenlockPrivateEventRouter>*
      GetFactoryInstance();
  virtual void Shutdown() OVERRIDE;

  // ScreenlockBridge::Observer
  virtual void OnScreenDidLock() OVERRIDE;
  virtual void OnScreenDidUnlock() OVERRIDE;

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
