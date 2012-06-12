// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/help/version_updater_unimplemented.h"

VersionUpdater* VersionUpdater::Create() {
  return new VersionUpdaterUnimplemented;
}
