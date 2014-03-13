// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DIAL_DIAL_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DIAL_DIAL_API_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/dial/dial_device_data.h"
#include "chrome/browser/extensions/api/dial/dial_registry.h"
#include "components/keyed_service/content/refcounted_browser_context_keyed_service.h"
#include "extensions/browser/api/async_api_function.h"
#include "extensions/browser/event_router.h"

namespace extensions {

class DialRegistry;

// Dial API which is a ref-counted KeyedService that manages
// the DIAL registry. It takes care of creating the registry on the IO thread
// and is an observer of the registry. It makes sure devices events are sent out
// to extension listeners on the right thread.
class DialAPI : public RefcountedBrowserContextKeyedService,
                public EventRouter::Observer,
                public DialRegistry::Observer {
 public:
  explicit DialAPI(Profile* profile);

  // The DialRegistry for the API. This must always be used only from the IO
  // thread.
  DialRegistry* dial_registry();

  // Called by the DialRegistry on the IO thread so that the DialAPI dispatches
  // the event to listeners on the UI thread.
  void SendEventOnUIThread(const DialRegistry::DeviceList& devices);
  void SendErrorOnUIThread(const DialRegistry::DialErrorCode type);

 private:
  virtual ~DialAPI();

  // RefcountedBrowserContextKeyedService:
  virtual void ShutdownOnUIThread() OVERRIDE;

  // EventRouter::Observer:
  virtual void OnListenerAdded(const EventListenerInfo& details) OVERRIDE;
  virtual void OnListenerRemoved(const EventListenerInfo& details) OVERRIDE;

  // DialRegistry::Observer:
  virtual void OnDialDeviceEvent(
      const DialRegistry::DeviceList& devices) OVERRIDE;
  virtual void OnDialError(DialRegistry::DialErrorCode type) OVERRIDE;

  // Methods to notify the DialRegistry on the correct thread of new/removed
  // listeners.
  void NotifyListenerAddedOnIOThread();
  void NotifyListenerRemovedOnIOThread();

  Profile* profile_;

  // Created lazily on first access on the IO thread.
  scoped_ptr<DialRegistry> dial_registry_;

  DISALLOW_COPY_AND_ASSIGN(DialAPI);
};

namespace api {

// DiscoverNow function. This function needs a round-trip from the IO thread
// because it needs to grab a pointer to the DIAL API in order to get a
// reference to the DialRegistry while on the IO thread. Then, the result
// must be returned on the UI thread.
class DialDiscoverNowFunction : public AsyncApiFunction {
 public:
  DialDiscoverNowFunction();

 protected:
  virtual ~DialDiscoverNowFunction() {}

  // AsyncApiFunction:
  virtual bool Prepare() OVERRIDE;
  virtual void Work() OVERRIDE;
  virtual bool Respond() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION("dial.discoverNow", DIAL_DISCOVERNOW)

  // Pointer to the DIAL API for this profile. We get this on the UI thread.
  DialAPI* dial_;

  // Result of the discoverNow call to the DIAL registry. This result is
  // retrieved on the IO thread but the function result is returned on the UI
  // thread.
  bool result_;

  DISALLOW_COPY_AND_ASSIGN(DialDiscoverNowFunction);
};

}  // namespace api

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DIAL_DIAL_API_H_
