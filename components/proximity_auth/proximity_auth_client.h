// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXIMITY_AUTH_CLIENT_H_
#define COMPONENTS_PROXIMITY_AUTH_CLIENT_H_

#include <string>

#include "base/macros.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace proximity_auth {

// An interface that needs to be supplied to the Proximity Auth component by its
// embedder.
class ProximityAuthClient {
 public:
  // Returns the authenticated username for |browser_context|.
  virtual std::string GetAuthenticatedUsername(
      content::BrowserContext* browser_context) const = 0;

  // Locks the screen for |browser_context|.
  virtual void Lock(content::BrowserContext* browser_context) = 0;

 protected:
  ProximityAuthClient() {}
  virtual ~ProximityAuthClient() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ProximityAuthClient);
};

}  // namespace proximity_auth

#endif  // COMPONENTS_PROXIMITY_AUTH_CLIENT_H_
