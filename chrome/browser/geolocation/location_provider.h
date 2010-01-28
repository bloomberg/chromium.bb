// Copyright 2008, Google Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. Neither the name of Google Inc. nor the names of its contributors may be
//     used to endorse or promote products derived from this software without
//     specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// A location provider provides position information from a particular source
// (GPS, network etc). The GearsGeolocation object uses a set of location
// providers to obtain a position fix.
//
// This file declares a base class to be used by all location providers.
// Primarily, this class declares interface methods to be implemented by derived
// classes.

#ifndef GEARS_GEOLOCATION_LOCATION_PROVIDER_H__
#define GEARS_GEOLOCATION_LOCATION_PROVIDER_H__

// TODO(joth): port to chromium
#if 0

#include <map>
#include "gears/base/common/base_class.h"
#include "gears/base/common/mutex.h"
#include "gears/base/common/string16.h"

struct Position;
class RefCount;

// The base class used by all location providers.
class LocationProviderBase {
 public:
  class ListenerInterface {
   public:
    // Used to inform listener that a new position fix is available or that a
    // fatal error has occurred. Providers should call this for new listeners
    // as soon as a position is available.
    virtual bool LocationUpdateAvailable(LocationProviderBase *provider) = 0;
    // Used to inform listener that movement has been detected. If obtaining the
    // position succeeds, this will be followed by a call to
    // LocationUpdateAvailable. Some providers may not be able to detect
    // movement before a new fix is obtained, so will never call this method.
    // Note that this is not called in response to registration of a new
    // listener.
    virtual bool MovementDetected(LocationProviderBase *provider) = 0;
    virtual ~ListenerInterface() {}
  };

  virtual ~LocationProviderBase() {}

  // Registers a listener, which will be called back on
  // ListenerInterface::LocationUpdateAvailable as soon as a position is
  // available and again whenever a new position is available. Ref counts the
  // listener to handle multiple calls to this method.
  virtual void RegisterListener(ListenerInterface *listener,
                                bool request_address);
  // Unregisters a listener. Unrefs the listener to handle multiple calls to
  // this method. Once the ref count reaches zero, the listener is removed and
  // once this method returns, no further calls to
  // ListenerInterface::LocationUpdateAvailable will be made for this listener.
  // It may block if a callback is in progress.
  virtual void UnregisterListener(ListenerInterface *listener);

  // Interface methods
  // Gets the current best position estimate.
  virtual void GetPosition(Position *position) = 0;
  // Provides a hint to the provider that new location data is needed as soon
  // as possible. Default implementation does nothing.
  virtual void UpdatePosition() {}

  // Accessor methods.
  typedef std::pair<bool, RefCount*> ListenerPair;
  typedef std::map<ListenerInterface*, ListenerPair> ListenerMap;
  ListenerMap *GetListeners();
  Mutex *GetListenersMutex();

 protected:
  // Inform listeners that a new position or error is available, using
  // LocationUpdateAvailable.
  virtual void UpdateListeners();
  // Inform listeners that movement has been detected, using MovementDetected.
  virtual void InformListenersOfMovement();

 private:
  // The listeners registered to this provider. For each listener, we store a
  // ref count and whether it requires an address.
  ListenerMap listeners_;
  Mutex listeners_mutex_;
};

// Factory functions for the various types of location provider to abstract over
// the platform-dependent implementations.
LocationProviderBase *NewMockLocationProvider();
LocationProviderBase *NewGpsLocationProvider(
    BrowsingContext *browsing_context,
    const std::string16 &reverse_geocode_url,
    const std::string16 &host_name,
    const std::string16 &address_language);
LocationProviderBase *NewNetworkLocationProvider(
    BrowsingContext *browsing_context,
    const std::string16 &url,
    const std::string16 &host_name,
    const std::string16 &language);

#endif  // if 0

#endif  // GEARS_GEOLOCATION_LOCATION_PROVIDER_H__
