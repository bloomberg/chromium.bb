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
// Id used when there is no corresponding ash container.
const int kUnknownAshId = -1;

// The set of containers that allow their children to be active.
extern const mojom::Container kActivationContainers[];

// Number of kActivationContainers.
extern const size_t kNumActivationContainers;

// Converts window ids between ash (ash/common/shell_window_ids.h) and
// mash.
mojom::Container AshContainerToMashContainer(int shell_window_id);
int MashContainerToAshContainer(mojom::Container container);

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_CONTAINER_IDS_H_
