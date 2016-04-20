// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/website_settings/permission_bubble_request.h"

#include "ui/gfx/vector_icons_public.h"

gfx::VectorIconId PermissionBubbleRequest::GetVectorIconId() const {
  return gfx::VectorIconId::VECTOR_ICON_NONE;
}

PermissionBubbleType PermissionBubbleRequest::GetPermissionBubbleType() const {
  return PermissionBubbleType::UNKNOWN;
}
