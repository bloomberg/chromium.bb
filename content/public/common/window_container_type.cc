// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/window_container_type.h"

#include <stddef.h>

#include "base/strings/string_util.h"

const WindowContainerType WINDOW_CONTAINER_TYPE_NORMAL =
    content::mojom::WindowContainerType::NORMAL;
const WindowContainerType WINDOW_CONTAINER_TYPE_BACKGROUND =
    content::mojom::WindowContainerType::BACKGROUND;
const WindowContainerType WINDOW_CONTAINER_TYPE_PERSISTENT =
    content::mojom::WindowContainerType::PERSISTENT;
