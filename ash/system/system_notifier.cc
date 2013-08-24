// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/system_notifier.h"

#include "base/logging.h"

namespace ash {

std::string SystemComponentTypeToString(AshSystemComponentNotifierType type) {
  if (type == NOTIFIER_SCREENSHOT)
    return "screenshot";

  // TODO(mukai): fill the names of other components.
  NOTIMPLEMENTED();
  return std::string();
}

}  // namespace
