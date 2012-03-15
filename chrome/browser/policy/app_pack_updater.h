// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_APP_PACK_UPDATER_H_
#define CHROME_BROWSER_POLICY_APP_PACK_UPDATER_H_
#pragma once

namespace policy {

// Work in progress: http://crosbug.com/25463
class AppPackUpdater {
 public:
  // Keys for the entries in the KioskModeAppPack dictionary policy.
  static const char kExtensionId[];
  static const char kUpdateUrl[];
  static const char kOnlineOnly[];
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_APP_PACK_UPDATER_H_
