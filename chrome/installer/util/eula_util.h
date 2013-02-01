// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains helper functions to read the Chrome EULA.

#ifndef CHROME_INSTALLER_UTIL_EULA_UTIL_H_
#define CHROME_INSTALLER_UTIL_EULA_UTIL_H_

namespace installer {

enum EULAAcceptanceResponse{
  QUERY_EULA_FAIL = -1,
  QUERY_EULA_NOT_ACCEPTED = 0,
  QUERY_EULA_ACCEPTED = 1,
};

// Performs a comprehensive test on EULA acceptance in Chrome.
// This is mostly intended for system-level Chrome, although it also supports
// user-level Chrome (should return QUERY_EULA_ACCEPTED).
EULAAcceptanceResponse IsEULAAccepted(bool system_level);

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_EULA_UTIL_H_
