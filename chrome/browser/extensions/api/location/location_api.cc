// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/location/location_api.h"

#include "chrome/browser/extensions/api/location/location_manager.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/common/extensions/api/location.h"

// TODO(vadimt): add tests.

namespace location = extensions::api::location;
namespace WatchLocation = location::WatchLocation;
namespace ClearWatch = location::ClearWatch;

namespace extensions {

bool LocationWatchLocationFunction::RunImpl() {
  scoped_ptr<WatchLocation::Params> params(
      WatchLocation::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  // TODO(vadimt): validate and use params->request_info.
  ExtensionSystem::Get(profile())->location_manager()->AddLocationRequest(
      extension_id(), params->name);

  return true;
}

bool LocationClearWatchFunction::RunImpl() {
  scoped_ptr<ClearWatch::Params> params(
      ClearWatch::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  ExtensionSystem::Get(profile())->location_manager()->RemoveLocationRequest(
      extension_id(), params->name);

  return true;
}

}  // namespace extensions
