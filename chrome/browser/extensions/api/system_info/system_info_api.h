// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_SYSTEM_INFO_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_SYSTEM_INFO_API_H_

#include "chrome/browser/extensions/api/profile_keyed_api_factory.h"
#include "chrome/browser/extensions/event_router.h"

namespace extensions {

// A Profile-scoped object which is registered as an observer of EventRouter
// to observe the systemInfo event listener arrival/removal.
class SystemInfoAPI : public ProfileKeyedAPI,
                      public EventRouter::Observer {
 public:
  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<SystemInfoAPI>* GetFactoryInstance();

  explicit SystemInfoAPI(Profile* profile);
  virtual ~SystemInfoAPI();

  // ProfileKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // EventRouter::Observer implementation.
  virtual void OnListenerAdded(const EventListenerInfo& details) OVERRIDE;
  virtual void OnListenerRemoved(const EventListenerInfo& details) OVERRIDE;

 private:
  friend class ProfileKeyedAPIFactory<SystemInfoAPI>;

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "SystemInfoAPI";
  }
  static const bool kServiceIsNULLWhileTesting = true;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(SystemInfoAPI);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_SYSTEM_INFO_API_H_
