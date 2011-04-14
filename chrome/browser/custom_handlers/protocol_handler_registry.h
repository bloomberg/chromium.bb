// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CUSTOM_HANDLERS_PROTOCOL_HANDLER_REGISTRY_H_
#define CHROME_BROWSER_CUSTOM_HANDLERS_PROTOCOL_HANDLER_REGISTRY_H_
#pragma once

#include <string>
#include <map>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/custom_handlers/protocol_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"



// This is where handlers for protocols registered with
// navigator.registerProtocolHandler() are registered. Each Profile owns an
// instance of this class, which is initialized on browser start through
// Profile::InitRegisteredProtocolHandlers(), and they should be the only
// instances of this class.

class ProtocolHandlerRegistry
    : public base::RefCountedThreadSafe<ProtocolHandlerRegistry> {
 public:
  explicit ProtocolHandlerRegistry(Profile* profile);

  // Called when the user accepts the registration of a given protocol handler.
  void OnAcceptRegisterProtocolHandler(ProtocolHandler* handler);

  // Called when the user denies the registration of a given protocol handler.
  void OnDenyRegisterProtocolHandler(ProtocolHandler* handler);

  // Loads a user's registered protocol handlers.
  void Load();

  // Saves a user's registered protocol handlers.
  void Save();

  // Returns the handler for this protocol.
  ProtocolHandler* GetHandlerFor(const std::string& scheme) const;

  // Returns true if we allow websites to register handlers for the given
  // scheme.
  bool CanSchemeBeOverridden(const std::string& scheme) const;

  // Returns true if an identical protocol handler has already been registered.
  bool IsAlreadyRegistered(const ProtocolHandler* handler) const;

  // URLRequestFactory for use with URLRequest::RegisterProtocolFactory().
  // Redirects any URLRequests for which there is a matching protocol handler.
  static net::URLRequestJob* Factory(net::URLRequest* request,
                                     const std::string& scheme);

  // Registers the preferences that we store registered protocol handlers in.
  static void RegisterPrefs(PrefService* prefService);

  // Creates a URL request job for the given request if there is a matching
  // protocol handler, returns NULL otherwise.
  net::URLRequestJob* MaybeCreateJob(net::URLRequest* request) const;

 private:
  typedef std::map<std::string, ProtocolHandler*> ProtocolHandlerMap;

  friend class base::RefCountedThreadSafe<ProtocolHandlerRegistry>;
  ~ProtocolHandlerRegistry();

  // Returns a JSON dictionary of protocols to protocol handlers. The caller is
  // responsible for deleting this Value.
  Value* Encode();

  // Registers a new protocol handler.
  void RegisterProtocolHandler(ProtocolHandler* handler);

  // Registers a new protocol handler from a JSON dictionary.
  void RegisterHandlerFromValue(const DictionaryValue* value);

  // Map from protocols (strings) to protocol handlers.
  ProtocolHandlerMap protocolHandlers_;

  // The Profile that owns this ProtocolHandlerRegistry.
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ProtocolHandlerRegistry);
};
#endif  // CHROME_BROWSER_CUSTOM_HANDLERS_PROTOCOL_HANDLER_REGISTRY_H_

