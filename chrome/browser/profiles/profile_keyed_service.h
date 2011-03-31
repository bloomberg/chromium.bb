// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_KEYED_SERVICE_H_
#define CHROME_BROWSER_PROFILES_PROFILE_KEYED_SERVICE_H_

// Base class for all ProfileKeyedServices to allow for correct destruction
// order.
//
// Many services that hang off Profile have a two-pass shutdown. Many
// subsystems need a first pass shutdown phase where they drop references. Not
// all services will need this, so there's a default implementation. Only once
// every system has been given a chance to drop references do we start deleting
// objects.
class ProfileKeyedService {
 public:
  // The first pass is to call Shutdown on a ProfileKeyedService.
  virtual void Shutdown() {}

  // The second pass is the actual deletion of each object.
  virtual ~ProfileKeyedService() {}
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_KEYED_SERVICE_H_

