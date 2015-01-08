// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SDCH_OBSERVER_H_
#define NET_BASE_SDCH_OBSERVER_H_

#include <iosfwd>
#include <string>

#include "net/base/net_export.h"

class GURL;

namespace net {

class SdchManager;

// Observer interface for SDCH.  Observers can register with
// the SdchManager to receive notifications of various SDCH events.
class NET_EXPORT SdchObserver {
 public:
  virtual ~SdchObserver();

  // TODO(rdsmith): Add Added/Removed signals.  These are only needed if
  // we end up with an implementation in which more than one observer
  // generates Add/Removed events; otherwise, tracking can be done internally.

  // TODO(rdsmith): Add signal that an Avail-Dictionary header was generated.
  // Should be added if/when an observer wants to use it to fine-tune
  // dictionary deprecation (e.g. if Avail-Dictionary is generated and
  // the remote *doesn't* use it, that should deprecate the dictionary faster)

  // A SDCH encoded response was received and the specified dictionary
  // was used to decode it.  This notification only occurs for successful
  // decodes.
  // TODO(rdsmith): Should this notification indicate how much
  // compression the dictionary provided?
  virtual void OnDictionaryUsed(SdchManager* manager,
                                const std::string& server_hash) = 0;

  // A "Get-Dictionary" header has been seen.
  virtual void OnGetDictionary(SdchManager* manager,
                               const GURL& request_url,
                               const GURL& dictionary_url) = 0;

  // Notification that SDCH has received a request to clear all
  // its dictionaries.
  virtual void OnClearDictionaries(SdchManager* manager) = 0;
};

}  // namespace net

#endif  // NET_BASE_SDCH_MANAGER_H_
