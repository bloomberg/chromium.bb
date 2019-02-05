// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_SHORTCUT_VIEWER_PUBLIC_CPP_MANIFEST_H_
#define ASH_COMPONENTS_SHORTCUT_VIEWER_PUBLIC_CPP_MANIFEST_H_

#include "services/service_manager/public/cpp/manifest.h"

namespace shortcut_viewer {

const service_manager::Manifest& GetManifest();

}  // namespace shortcut_viewer

#endif  // ASH_COMPONENTS_SHORTCUT_VIEWER_PUBLIC_CPP_MANIFEST_H_
