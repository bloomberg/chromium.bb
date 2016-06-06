// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_CONTAINER_IDS_H_
#define ASH_MUS_CONTAINER_IDS_H_

#include <stddef.h>

namespace ash {
namespace mojom {
enum class Container;
}

namespace mus {

// Converts a Container to a shell window ids (ash/common/shell_window_ids.h).
int MashContainerToAshShellWindowId(mojom::Container container);

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_CONTAINER_IDS_H_
