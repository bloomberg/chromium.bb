// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DIAL_DIAL_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DIAL_DIAL_API_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/extensions/api/dial/dial_device_data.h"
#include "chrome/browser/extensions/api/dial/dial_registry.h"
#include "chrome/common/extensions/api/dial.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"
#include "extensions/browser/api/async_api_function.h"
#include "extensions/browser/event_router.h"

namespace extensions {

namespace api {
namespace dial {
class DeviceDescriptionFetcher;
}  // namespace dial
}  // namespace api

class DialFetchDeviceDescriptionFunction;

// Dial API which is a ref-counted KeyedService that manages
// the DIAL registry. It takes care of creating the registry on the IO thread
// and is an observer of the registry. It makes sure devices events are sent out
// to extension listeners on the right thread.
//
// TODO(mfoltz): This should probably inherit from BrowserContextKeyedAPI
// instead; ShutdownOnUIThread below is a no-op, which is the whole point of
// RefcountedKeyedService.
//
// TODO(mfoltz): The threading model for this API needs to be rethought.  At a
// minimum, DialRegistry should move to the UI thread to avoid extra thread hops
// here.  This would also allow a straightforward GetDeviceList implementation
// (crbug.com/576817), cleanup some long-tail crashes (crbug.com/640011) that
// are likely due to lifetime issues, and simplify unit tests
// (crbug.com/661457).
//
// Also, DialRegistry should be an interface that can be mocked and injected for
// tests; this would allow us to remove code that injects test data into the
// real DialRegsitry.
class DialAPI : public RefcountedKeyedService,
                public EventRouter::Observer,
                public api::dial::DialRegistry::Observer {
 public:
  explicit DialAPI(Profile* profile);

  // The DialRegistry for the API. This must always be used only from the IO
  // thread.
  api::dial::DialRegistry* dial_registry();

  // Called by the DialRegistry on the IO thread so that the DialAPI dispatches
  // the event to listeners on the UI thread.
  void SendEventOnUIThread(const api::dial::DialRegistry::DeviceList& devices);
  void SendErrorOnUIThread(const api::dial::DialRegistry::DialErrorCode type);

  // Sets test device data.
  void SetDeviceForTest(
      const api::dial::DialDeviceData& device_data,
      const api::dial::DialDeviceDescriptionData& device_description);

 private:
  ~DialAPI() override;

  friend class DialFetchDeviceDescriptionFunction;

  // RefcountedKeyedService:
  void ShutdownOnUIThread() override;

  // EventRouter::Observer:
  void OnListenerAdded(const EventListenerInfo& details) override;
  void OnListenerRemoved(const EventListenerInfo& details) override;

  // DialRegistry::Observer:
  void OnDialDeviceEvent(
      const api::dial::DialRegistry::DeviceList& devices) override;
  void OnDialError(api::dial::DialRegistry::DialErrorCode type) override;

  // Methods to notify the DialRegistry on the correct thread of new/removed
  // listeners.
  void NotifyListenerAddedOnIOThread();
  void NotifyListenerRemovedOnIOThread();

  Profile* profile_;

  // Created lazily on first access on the IO thread.
  std::unique_ptr<api::dial::DialRegistry> dial_registry_;

  // Device data for testing.
  std::unique_ptr<api::dial::DialDeviceData> test_device_data_;
  std::unique_ptr<api::dial::DialDeviceDescriptionData>
      test_device_description_;

  DISALLOW_COPY_AND_ASSIGN(DialAPI);
};

// DiscoverNow function. This function needs a round-trip from the IO thread
// because it needs to grab a pointer to the DIAL API in order to get a
// reference to the DialRegistry while on the IO thread. Then, the result
// must be returned on the UI thread.
class DialDiscoverNowFunction : public AsyncApiFunction {
 public:
  DialDiscoverNowFunction();

 protected:
  ~DialDiscoverNowFunction() override {}

  // AsyncApiFunction:
  bool Prepare() override;
  void Work() override;
  bool Respond() override;

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

class DialFetchDeviceDescriptionFunction : public AsyncExtensionFunction {
 public:
  DialFetchDeviceDescriptionFunction();

 protected:
  ~DialFetchDeviceDescriptionFunction() override;

  // AsyncExtensionFunction:
  bool RunAsync() override;

 private:
  DECLARE_EXTENSION_FUNCTION("dial.fetchDeviceDescription",
                             DIAL_FETCHDEVICEDESCRIPTION)

  void GetDeviceDescriptionUrlOnIOThread(const std::string& label);
  void MaybeStartFetch(const GURL& url);
  void OnFetchComplete(const api::dial::DialDeviceDescriptionData& result);
  void OnFetchError(const std::string& result);

  std::unique_ptr<api::dial::FetchDeviceDescription::Params> params_;
  std::unique_ptr<api::dial::DeviceDescriptionFetcher>
      device_description_fetcher_;

  DialAPI* dial_;

  DISALLOW_COPY_AND_ASSIGN(DialFetchDeviceDescriptionFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DIAL_DIAL_API_H_
