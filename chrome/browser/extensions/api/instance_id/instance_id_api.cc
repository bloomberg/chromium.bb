// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/instance_id/instance_id_api.h"

#include "base/logging.h"
#include "extensions/common/extension.h"

namespace extensions {

InstanceIDGetIDFunction::InstanceIDGetIDFunction() {}

InstanceIDGetIDFunction::~InstanceIDGetIDFunction() {}

ExtensionFunction::ResponseAction InstanceIDGetIDFunction::Run() {
  NOTIMPLEMENTED();
  return RespondLater();
}

InstanceIDGetCreationTimeFunction::InstanceIDGetCreationTimeFunction() {}

InstanceIDGetCreationTimeFunction::~InstanceIDGetCreationTimeFunction() {}

ExtensionFunction::ResponseAction InstanceIDGetCreationTimeFunction::Run() {
  NOTIMPLEMENTED();
  return RespondLater();
}

InstanceIDGetTokenFunction::InstanceIDGetTokenFunction() {}

InstanceIDGetTokenFunction::~InstanceIDGetTokenFunction() {}

ExtensionFunction::ResponseAction InstanceIDGetTokenFunction::Run() {
  NOTIMPLEMENTED();
  return RespondLater();
}

InstanceIDDeleteTokenFunction::InstanceIDDeleteTokenFunction() {}

InstanceIDDeleteTokenFunction::~InstanceIDDeleteTokenFunction() {}

ExtensionFunction::ResponseAction InstanceIDDeleteTokenFunction::Run() {
  NOTIMPLEMENTED();
  return RespondLater();
}

InstanceIDDeleteIDFunction::InstanceIDDeleteIDFunction() {}

InstanceIDDeleteIDFunction::~InstanceIDDeleteIDFunction() {}

ExtensionFunction::ResponseAction InstanceIDDeleteIDFunction::Run() {
  NOTIMPLEMENTED();
  return RespondLater();
}

}  // namespace extensions
