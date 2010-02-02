// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A location provider provides position information from a particular source
// (GPS, network etc). The GearsGeolocation object uses a set of location
// providers to obtain a position fix.
//
// This file declares a base class to be used by all location providers.
// Primarily, this class declares interface methods to be implemented by derived
// classes.

#ifndef CHROME_BROWSER_GEOLOCATION_LOCATION_PROVIDER_H_
#define CHROME_BROWSER_GEOLOCATION_LOCATION_PROVIDER_H_

#include <map>
#include "base/lock.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/string16.h"
#include "base/task.h"

class GURL;
struct Position;
class URLRequestContextGetter;

// The base class used by all location providers.
class LocationProviderBase
    : public base::RefCountedThreadSafe<LocationProviderBase> {
 public:
  // Provides storage for the access token used in the network request.
  // Normally the client (i.e. geolocation controller) implements this, but
  // also allows mocking for testing.
  class AccessTokenStore {
   public:
    virtual bool SetAccessToken(const GURL& url,
                                const string16& access_token) = 0;
    virtual bool GetAccessToken(const GURL& url, string16* access_token) = 0;

   protected:
    virtual ~AccessTokenStore() {}
  };

  // Clients of the location provider must implement this interface. All call-
  // backs to this interface will happen in the context of the thread on which
  // the location provider was created.
  class ListenerInterface {
   public:
    // Used to inform listener that a new position fix is available or that a
    // fatal error has occurred. Providers should call this for new listeners
    // as soon as a position is available.
    virtual bool LocationUpdateAvailable(LocationProviderBase* provider) = 0;
    // Used to inform listener that movement has been detected. If obtaining the
    // position succeeds, this will be followed by a call to
    // LocationUpdateAvailable. Some providers may not be able to detect
    // movement before a new fix is obtained, so will never call this method.
    // Note that this is not called in response to registration of a new
    // listener.
    virtual bool MovementDetected(LocationProviderBase* provider) = 0;

   protected:
    virtual ~ListenerInterface() {}
  };

  // TODO(joth): Make register / unregister non-virtual.
  // Registers a listener, which will be called back on
  // ListenerInterface::LocationUpdateAvailable as soon as a position is
  // available and again whenever a new position is available. Ref counts the
  // listener to handle multiple calls to this method.
  virtual void RegisterListener(ListenerInterface* listener);
  // Unregisters a listener. Unrefs the listener to handle multiple calls to
  // this method. Once the ref count reaches zero, the listener is removed and
  // once this method returns, no further calls to
  // ListenerInterface::LocationUpdateAvailable will be made for this listener.
  // It may block if a callback is in progress.
  virtual void UnregisterListener(ListenerInterface* listener);

  // Interface methods
  // Returns false if the provider failed to start.
  virtual bool StartProvider() = 0;
  // Gets the current best position estimate.
  virtual void GetPosition(Position* position) = 0;
  // Provides a hint to the provider that new location data is needed as soon
  // as possible. Default implementation does nothing.
  virtual void UpdatePosition() {}

 protected:
  // Instances should only be destroyed via the thread safe ref count; derived
  // classes should not have a public destructor.
  friend class base::RefCountedThreadSafe<LocationProviderBase>;
  LocationProviderBase();
  virtual ~LocationProviderBase();

  // Inform listeners that a new position or error is available, using
  // LocationUpdateAvailable.
  virtual void UpdateListeners();
  // Inform listeners that movement has been detected, using MovementDetected.
  virtual void InformListenersOfMovement();

  MessageLoop* client_loop() { return client_loop_; }
  void CheckRunningInClientLoop();

 private:
  // Utility methods to delegate method calls into the client thread
  template <class Method>
  bool RunInClientThread(const tracked_objects::Location& from_here,
                         Method method) {
    if (MessageLoop::current() == client_loop_) {
      return false;
    }
    client_loop_->PostTask(from_here, NewRunnableMethod(this, method));
    return true;
  }
  template <class Method, class A>
  bool RunInClientThread(const tracked_objects::Location& from_here,
                         Method method, const A& a) {
    if (MessageLoop::current() == client_loop_) {
      return false;
    }
    client_loop_->PostTask(from_here, NewRunnableMethod(this, method, a));
    return true;
  }

  // The listeners registered to this provider. For each listener, we store a
  // ref count.
  typedef std::map<ListenerInterface*, int> ListenerMap;
  ListenerMap listeners_;

  // Reference to the client's message loop, all callbacks and access to
  // the listeners_ member should happen in this context.
  MessageLoop* client_loop_;
};

// Factory functions for the various types of location provider to abstract over
// the platform-dependent implementations.
LocationProviderBase* NewMockLocationProvider();
LocationProviderBase* NewGpsLocationProvider();
LocationProviderBase* NewNetworkLocationProvider(
    LocationProviderBase::AccessTokenStore* access_token_store,
    URLRequestContextGetter* context,
    const GURL& url,
    const string16& host_name);

#endif  // CHROME_BROWSER_GEOLOCATION_LOCATION_PROVIDER_H_
