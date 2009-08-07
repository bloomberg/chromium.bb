// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_AUTH_ERROR_STATE_H_
#define CHROME_BROWSER_SYNC_AUTH_ERROR_STATE_H_

#include <string>
#include "base/string_util.h"

enum AuthErrorState {
  // The user is authenticated.
  AUTH_ERROR_NONE = 0,

  // The credentials supplied to GAIA were either invalid, or the locally
  // cached credentials have expired. If this happens, the sync system
  // will continue as if offline until authentication is reattempted.
  AUTH_ERROR_INVALID_GAIA_CREDENTIALS,

  // The GAIA user is not authorized to use the sync service.
  AUTH_ERROR_USER_NOT_SIGNED_UP,

  // Could not connect to server to verify credentials. This could be in
  // response to either failure to connect to GAIA or failure to connect to
  // the service needing GAIA tokens during authentication.
  AUTH_ERROR_CONNECTION_FAILED,
};

#endif  // CHROME_BROWSER_SYNC_AUTH_ERROR_STATE_H_
