// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FRAME_OWNER_PROPERTIES_H_
#define CONTENT_COMMON_FRAME_OWNER_PROPERTIES_H_

#include <vector>

#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/modules/permissions/WebPermissionType.h"
#include "third_party/WebKit/public/web/WebFrameOwnerProperties.h"

namespace content {

// Used for IPC transport of WebFrameOwnerProperties. WebFrameOwnerProperties
// can't be used directly as it contains a WebVector which doesn't have
// ParamTraits defined.
struct CONTENT_EXPORT FrameOwnerProperties {
  FrameOwnerProperties();
  FrameOwnerProperties(const FrameOwnerProperties& other);
  explicit FrameOwnerProperties(
      const blink::WebFrameOwnerProperties& web_frame_owner_properties);
  ~FrameOwnerProperties();

  blink::WebFrameOwnerProperties ToWebFrameOwnerProperties() const;

  bool operator==(const FrameOwnerProperties& other) const;
  bool operator!=(const FrameOwnerProperties& other) const {
    return !(*this == other);
  }

  blink::WebFrameOwnerProperties::ScrollingMode scrolling_mode;
  int margin_width;
  int margin_height;
  bool allow_fullscreen;
  std::vector<blink::WebPermissionType> delegated_permissions;
};

}  // namespace content

#endif  // CONTENT_COMMON_FRAME_OWNER_PROPERTIES_H_
