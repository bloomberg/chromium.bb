// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FILESYSTEM_PUBLIC_CPP_PREFS_PREF_SERVICE_FACTORY_H_
#define COMPONENTS_FILESYSTEM_PUBLIC_CPP_PREFS_PREF_SERVICE_FACTORY_H_

#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"

namespace mojo {
class ApplicationImpl;
}

class PrefRegistry;

namespace filesystem {

// This factory method creates a PrefService for the local process based on the
// preference registry passed in. This PrefService will synchronize with a JSON
// file in the mojo:filesystem.
scoped_ptr<PrefService> CreatePrefService(mojo::ApplicationImpl* application,
                                          PrefRegistry* registry);

}  // namespace filesystem

#endif  // COMPONENTS_FILESYSTEM_PUBLIC_CPP_PREFS_PREF_SERVICE_FACTORY_H_
