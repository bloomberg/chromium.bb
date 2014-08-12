// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/component_updater_paths.h"

#include "base/lazy_instance.h"
#include "base/path_service.h"

namespace component_updater {

namespace {

// This key gives the root directory of all the component installations.
static int g_components_root_key = -1;

}  // namespace

bool PathProvider(int key, base::FilePath* result) {
  DCHECK_GT(g_components_root_key, 0);

  // Early exit here to prevent a potential infinite loop when we retrieve
  // the path for g_components_root_key.
  if (key < PATH_START || key > PATH_END)
    return false;

  base::FilePath cur;
  if (!PathService::Get(g_components_root_key, &cur))
    return false;

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
    case DIR_COMPONENT_EV_WHITELIST:
      cur = cur.Append(FILE_PATH_LITERAL("EVWhitelist"));
      break;
    default:
      return false;
  }

  *result = cur;
  return true;
}

// This cannot be done as a static initializer sadly since Visual Studio will
// eliminate this object file if there is no direct entry point into it.
void RegisterPathProvider(int components_root_key) {
  DCHECK_EQ(g_components_root_key, -1);
  DCHECK_GT(components_root_key, 0);
  DCHECK(components_root_key < PATH_START || components_root_key > PATH_END);

  g_components_root_key = components_root_key;
  PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);
}

}  // namespace component_updater
