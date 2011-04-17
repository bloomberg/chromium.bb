// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_sync_data.h"

ExtensionSyncData::ExtensionSyncData()
    : uninstalled(false), enabled(false), incognito_enabled(false) {}

ExtensionSyncData::~ExtensionSyncData() {}
