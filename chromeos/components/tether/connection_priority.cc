// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/connection_priority.h"

#include "base/logging.h"

namespace chromeos {

namespace tether {

std::ostream& operator<<(std::ostream& stream,
                         const ConnectionPriority& connection_priority) {
  switch (connection_priority) {
    case ConnectionPriority::CONNECTION_PRIORITY_LOW:
      stream << "[low priority]";
      break;
    case ConnectionPriority::CONNECTION_PRIORITY_MEDIUM:
      stream << "[medium priority]";
      break;
    case ConnectionPriority::CONNECTION_PRIORITY_HIGH:
      stream << "[high priority]";
      break;
  }
  return stream;
}

}  // namespace tether

}  // namespace chromeos
