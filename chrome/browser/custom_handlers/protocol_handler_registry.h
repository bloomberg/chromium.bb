// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CUSTOM_HANDLERS_PROTOCOL_HANDLER_REGISTRY_H_
#define CHROME_BROWSER_CUSTOM_HANDLERS_PROTOCOL_HANDLER_REGISTRY_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

class ProtocolHandler;

typedef std::map<std::string, ProtocolHandler*> ProtocolHandlerMap;
typedef std::vector<ProtocolHandler*> ProtocolHandlerList;

// This is where handlers for protocols registered with
// navigator.registerProtocolHandler() are registered. Each Profile owns an
// instance of this class, which is initialized on browser start through
// Profile::InitRegisteredProtocolHandlers(), and they should be the only
// instances of this class.

class ProtocolHandlerRegistry
    : public base::RefCountedThreadSafe<ProtocolHandlerRegistry> {
 public:
  class Delegate;

  ProtocolHandlerRegistry(Profile* profile, Delegate* delegate);
  ~ProtocolHandlerRegistry();

  // Called when the user accepts the registration of a given protocol handler.
  void OnAcceptRegisterProtocolHandler(ProtocolHandler* handler);

  // Called when the user denies the registration of a given protocol handler.
  void OnDenyRegisterProtocolHandler(ProtocolHandler* handler);

  // Called when the user indicates that they don't want to be asked about the
  // given protocol handler again.
  void OnIgnoreRegisterProtocolHandler(ProtocolHandler* handler);

  // Loads a user's registered protocol handlers.
  void Load();

  // Saves a user's registered protocol handlers.
  void Save();

  // Returns the handler for this protocol.
  ProtocolHandler* GetHandlerFor(const std::string& scheme) const;

  // Yields a list of the protocols handled by this registry.
  void GetHandledProtocols(std::vector<std::string>* output);

  // Returns true if we allow websites to register handlers for the given
  // scheme.
  bool CanSchemeBeOverridden(const std::string& scheme) const;

  // Returns true if an identical protocol handler has already been registered.
  bool IsRegistered(const ProtocolHandler* handler) const;

  // Returns true if the protocol handler is being ignored.
  bool IsIgnored(const ProtocolHandler* handler) const;

  // Returns true if the protocol has a registered protocol handler.
  bool IsHandledProtocol(const std::string& scheme) const;

  // Removes any existing protocol handler for the given protocol.
  void RemoveHandlerFor(const std::string& scheme);

  // Causes the given protocol handler to not be ignored anymore.
  void RemoveIgnoredHandler(ProtocolHandler* handler);

  // URLRequestFactory for use with URLRequest::RegisterProtocolFactory().
  // Redirects any URLRequests for which there is a matching protocol handler.
  static net::URLRequestJob* Factory(net::URLRequest* request,
                                     const std::string& scheme);

  // Registers the preferences that we store registered protocol handlers in.
  static void RegisterPrefs(PrefService* prefService);

  // Creates a URL request job for the given request if there is a matching
  // protocol handler, returns NULL otherwise.
  net::URLRequestJob* MaybeCreateJob(net::URLRequest* request) const;

  // Puts this registry in the enabled state - registered protocol handlers
  // will handle requests.
  void Enable();

  // Puts this registry in the disabled state - registered protocol handlers
  // will not handle requests.
  void Disable();

  bool enabled() { return enabled_; }

  class Delegate {
   public:
    virtual ~Delegate();
    virtual void RegisterExternalHandler(const std::string& protocol);
    virtual void DeregisterExternalHandler(const std::string& protocol);
    virtual bool IsExternalHandlerRegistered(const std::string& protocol);
  };

 private:

  friend class base::RefCountedThreadSafe<ProtocolHandlerRegistry>;

  // Returns a JSON list of protocol handlers. The caller is responsible for
  // deleting this Value.
  Value* EncodeRegisteredHandlers();

  // Returns a JSON list of ignored protocol handlers. The caller is
  // responsible for deleting this Value.
  Value* EncodeIgnoredHandlers();

  // Registers a new protocol handler.
  void RegisterProtocolHandler(ProtocolHandler* handler);

  // Get the ProtocolHandlers stored under the given pref name. The caller owns
  // the returned ProtocolHandlers and is responsible for deleting them.
  ProtocolHandlerList GetHandlersFromPref(const char* pref_name);

  // Ignores future requests to register the given protocol handler.
  void IgnoreProtocolHandler(ProtocolHandler* handler);

  // Registers a new protocol handler from a JSON dictionary.
  void RegisterHandlerFromValue(const DictionaryValue* value);

  // Register
  void IgnoreHandlerFromValue(const DictionaryValue* value);

  // Map from protocols (strings) to protocol handlers.
  ProtocolHandlerMap protocol_handlers_;

  // Protocol handlers that the user has told us to ignore.
  ProtocolHandlerList ignored_protocol_handlers_;

  // The Profile that owns this ProtocolHandlerRegistry.
  Profile* profile_;

  // The Delegate that registers / deregisters external handlers on our behalf.
  scoped_ptr<Delegate> delegate_;

  // If false then registered protocol handlers will not be used to handle
  // requests.
  bool enabled_;

  DISALLOW_COPY_AND_ASSIGN(ProtocolHandlerRegistry);
};
#endif  // CHROME_BROWSER_CUSTOM_HANDLERS_PROTOCOL_HANDLER_REGISTRY_H_

