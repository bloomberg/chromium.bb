// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_HOTWORD_PRIVATE_HOTWORD_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_HOTWORD_PRIVATE_HOTWORD_PRIVATE_API_H_

#include "base/prefs/pref_change_registrar.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/common/extensions/api/hotword_private.h"

class Profile;

namespace extensions {

// Listens for changes in disable/enabled state and forwards as an extension
// event.
class HotwordPrivateEventService : public ProfileKeyedAPI  {
 public:
  explicit HotwordPrivateEventService(content::BrowserContext* context);
  virtual ~HotwordPrivateEventService();

  // ProfileKeyedAPI implementation.
  virtual void Shutdown() OVERRIDE;
  static ProfileKeyedAPIFactory<HotwordPrivateEventService>*
      GetFactoryInstance();
  static const char* service_name();

  void OnEnabledChanged(const std::string& pref_name);

 private:
  friend class ProfileKeyedAPIFactory<HotwordPrivateEventService>;

  void SignalEvent();

  Profile* profile_;
  PrefChangeRegistrar pref_change_registrar_;
};


class HotwordPrivateSetEnabledFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hotwordPrivate.setEnabled",
                             HOTWORDPRIVATE_SETENABLED)

 protected:
  virtual ~HotwordPrivateSetEnabledFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class HotwordPrivateGetStatusFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hotwordPrivate.getStatus",
                             HOTWORDPRIVATE_GETSTATUS)

 protected:
  virtual ~HotwordPrivateGetStatusFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_HOTWORD_PRIVATE_HOTWORD_PRIVATE_API_H_
