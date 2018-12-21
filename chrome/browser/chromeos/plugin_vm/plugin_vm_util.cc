// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"

#include "chrome/browser/profiles/profile.h"

namespace plugin_vm {

bool IsPluginVmAllowedForProfile(Profile* profile) {
  // TODO (okalitova): check that the right policies are in place.
  // Right now, this always returns false so that no icon for
  // PluginVm will ever be visible.
  return false;
}

}  // namespace plugin_vm
