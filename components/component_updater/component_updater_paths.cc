// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/component_updater_paths.h"

#include "base/lazy_instance.h"
#include "base/path_service.h"

namespace component_updater {

namespace {

static base::LazyInstance<base::FilePath> g_components_root =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

bool PathProvider(int key, base::FilePath* result) {
  DCHECK(!g_components_root.Get().empty());
  base::FilePath cur = g_components_root.Get();
  switch (key) {
    case DIR_COMPONENT_CLD2:
      cur = cur.Append(FILE_PATH_LITERAL("CLD"));
      break;
    case DIR_RECOVERY_BASE:
      cur = cur.Append(FILE_PATH_LITERAL("recovery"));
      break;
    case DIR_SWIFT_SHADER:
      cur = cur.Append(FILE_PATH_LITERAL("SwiftShader"));
      break;
    case DIR_SW_REPORTER:
      cur = cur.Append(FILE_PATH_LITERAL("SwReporter"));
      break;
    default:
      return false;
  }

  *result = cur;
  return true;
}

// This cannot be done as a static initializer sadly since Visual Studio will
// eliminate this object file if there is no direct entry point into it.
void RegisterPathProvider(const base::FilePath& components_root) {
  DCHECK(g_components_root.Get().empty());
  DCHECK(!components_root.empty());
  g_components_root.Get() = components_root;

  PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);
}

}  // namespace component_updater
