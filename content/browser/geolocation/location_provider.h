// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A location provider provides position information from a particular source
// (GPS, network etc). The GearsGeolocation object uses a set of location
// providers to obtain a position fix.
//
// This file declares a base class to be used by all location providers.
// Primarily, this class declares interface methods to be implemented by
// derived classes.

#ifndef CONTENT_BROWSER_GEOLOCATION_LOCATION_PROVIDER_H_
#define CONTENT_BROWSER_GEOLOCATION_LOCATION_PROVIDER_H_
#pragma once

#include <map>

#include "base/string16.h"
#include "base/threading/non_thread_safe.h"

class AccessTokenStore;
struct Geoposition;
class GURL;
class URLRequestContextGetter;

// The base class used by all location providers.
class LocationProviderBase : public base::NonThreadSafe {
 public:
  // Clients of the location provider must implement this interface. All call-
  // backs to this interface will happen in the context of the thread on which
  // the location provider was created.
  class ListenerInterface {
   public:
    // Used to inform listener that a new position fix is available or that a
    // fatal error has occurred. Providers should call this for new listeners
    // as soon as a position is available.
    virtual void LocationUpdateAvailable(LocationProviderBase* provider) = 0;

   protected:
    virtual ~ListenerInterface() {}
  };

  virtual ~LocationProviderBase();

  // Registers a listener, which will be called back on
  // ListenerInterface::LocationUpdateAvailable as soon as a position is
  // available and again whenever a new position is available. Ref counts the
  // listener to handle multiple calls to this method.
  void RegisterListener(ListenerInterface* listener);
  // Unregisters a listener. Unrefs the listener to handle multiple calls to
  // this method. Once the ref count reaches zero, the listener is removed and
  // once this method returns, no further calls to
  // ListenerInterface::LocationUpdateAvailable will be made for this listener.
  // It may block if a callback is in progress.
  void UnregisterListener(ListenerInterface* listener);

  // Interface methods
  // StartProvider maybe called multiple times, e.g. to alter the
  // |high_accuracy| setting. Returns false if a fatal error was encountered
  // which prevented the provider from starting.
  virtual bool StartProvider(bool high_accuracy) = 0;
  virtual void StopProvider() = 0;
  // Gets the current best position estimate.
  virtual void GetPosition(Geoposition* position) = 0;
  // Provides a hint to the provider that new location data is needed as soon
  // as possible. Default implementation does nothing.
  virtual void UpdatePosition() {}
  // Delegated to the provider by GeolocationArbitrator. See the corresponding
  // method on that class for more details.
  virtual void OnPermissionGranted(const GURL& requesting_frame) {}

  bool has_listeners() const;

 protected:
  LocationProviderBase();

  // Inform listeners that a new position or error is available, using
  // LocationUpdateAvailable.
  void UpdateListeners();

 private:
  // The listeners registered to this provider. For each listener, we store a
  // ref count.
  typedef std::map<ListenerInterface*, int> ListenerMap;
  ListenerMap listeners_;
};

// Factory functions for the various types of location provider to abstract
// over the platform-dependent implementations.
LocationProviderBase* NewNetworkLocationProvider(
    AccessTokenStore* access_token_store,
    URLRequestContextGetter* context,
    const GURL& url,
    const string16& access_token);
LocationProviderBase* NewSystemLocationProvider();

#endif  // CONTENT_BROWSER_GEOLOCATION_LOCATION_PROVIDER_H_
