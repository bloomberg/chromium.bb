// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SDCH_OBSERVER_H_
#define NET_BASE_SDCH_OBSERVER_H_

#include "net/base/net_export.h"

class GURL;

namespace net {

class SdchManager;

// Observer interface for SDCH.  Observers can register with
// the SdchManager to receive notifications of various SDCH events.
class NET_EXPORT SdchObserver {
 public:
  virtual ~SdchObserver();

  // Notification that SDCH has seen a "Get-Dictionary" header.
  virtual void OnGetDictionary(SdchManager* manager,
                               const GURL& request_url,
                               const GURL& dictionary_url) = 0;

  // Notification that SDCH has received a request to clear all
  // its dictionaries.
  virtual void OnClearDictionaries(SdchManager* manager) = 0;
};

}  // namespace net

#endif  // NET_BASE_SDCH_MANAGER_H_
