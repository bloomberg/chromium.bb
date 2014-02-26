// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_SCREENLOCK_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_SCREENLOCK_PRIVATE_API_H_

#include "chrome/browser/chromeos/login/login_display.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chromeos/dbus/session_manager_client.h"

namespace gfx {
class Image;
}

namespace extensions {

class ScreenlockPrivateGetLockedFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("screenlockPrivate.getLocked",
                             SCREENLOCKPRIVATE_GETLOCKED)
  ScreenlockPrivateGetLockedFunction();
  virtual bool RunImpl() OVERRIDE;
 private:
  virtual ~ScreenlockPrivateGetLockedFunction();
  DISALLOW_COPY_AND_ASSIGN(ScreenlockPrivateGetLockedFunction);
};

class ScreenlockPrivateSetLockedFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("screenlockPrivate.setLocked",
                             SCREENLOCKPRIVATE_SETLOCKED)
  ScreenlockPrivateSetLockedFunction();
  virtual bool RunImpl() OVERRIDE;
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
  virtual bool RunImpl() OVERRIDE;
 private:
  virtual ~ScreenlockPrivateShowMessageFunction();
  DISALLOW_COPY_AND_ASSIGN(ScreenlockPrivateShowMessageFunction );
};

class ScreenlockPrivateShowButtonFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("screenlockPrivate.showButton",
                             SCREENLOCKPRIVATE_SHOWBUTTON)
  ScreenlockPrivateShowButtonFunction();
  virtual bool RunImpl() OVERRIDE;
 private:
  virtual ~ScreenlockPrivateShowButtonFunction();
  void OnImageLoaded(const gfx::Image& image);
  DISALLOW_COPY_AND_ASSIGN(ScreenlockPrivateShowButtonFunction);
};

class ScreenlockPrivateHideButtonFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("screenlockPrivate.hideButton",
                             SCREENLOCKPRIVATE_HIDEBUTTON)
  ScreenlockPrivateHideButtonFunction();
  virtual bool RunImpl() OVERRIDE;

 private:
  virtual ~ScreenlockPrivateHideButtonFunction();
  void OnImageLoaded(const gfx::Image& image);
  DISALLOW_COPY_AND_ASSIGN(ScreenlockPrivateHideButtonFunction);
};

class ScreenlockPrivateSetAuthTypeFunction
    : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("screenlockPrivate.setAuthType",
                             SCREENLOCKPRIVATE_SETAUTHTYPE)
  ScreenlockPrivateSetAuthTypeFunction();
  virtual bool RunImpl() OVERRIDE;

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
  virtual bool RunImpl() OVERRIDE;

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
  virtual bool RunImpl() OVERRIDE;

 private:
  virtual ~ScreenlockPrivateAcceptAuthAttemptFunction();
  DISALLOW_COPY_AND_ASSIGN(ScreenlockPrivateAcceptAuthAttemptFunction);
};

class ScreenlockPrivateEventRouter
    : public extensions::ProfileKeyedAPI,
      public chromeos::SessionManagerClient::Observer {
 public:
  explicit ScreenlockPrivateEventRouter(content::BrowserContext* context);
  virtual ~ScreenlockPrivateEventRouter();

  void OnButtonClicked();

  void OnAuthAttempted(chromeos::LoginDisplay::AuthType auth_type,
                       const std::string& value);

  // ProfileKeyedAPI
  static extensions::ProfileKeyedAPIFactory<ScreenlockPrivateEventRouter>*
    GetFactoryInstance();
  virtual void Shutdown() OVERRIDE;

  // chromeos::SessionManagerClient::Observer
  virtual void ScreenIsLocked() OVERRIDE;
  virtual void ScreenIsUnlocked() OVERRIDE;

 private:
  friend class extensions::ProfileKeyedAPIFactory<ScreenlockPrivateEventRouter>;

  // ProfileKeyedAPI
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

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_SCREENLOCK_PRIVATE_API_H_
