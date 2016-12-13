// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_router_struct_traits.h"

namespace mojo {

// static
bool StructTraits<media_router::mojom::IssueDataView, media_router::IssueInfo>::
    Read(media_router::mojom::IssueDataView data,
         media_router::IssueInfo* out) {
  if (!data.ReadTitle(&out->title))
    return false;

  if (!data.ReadDefaultAction(&out->default_action))
    return false;

  if (!data.ReadSeverity(&out->severity))
    return false;

  base::Optional<std::string> message;
  if (!data.ReadMessage(&message))
    return false;

  out->message = message.value_or(std::string());

  if (!data.ReadSecondaryActions(&out->secondary_actions))
    return false;

  base::Optional<std::string> route_id;
  if (!data.ReadRouteId(&route_id))
    return false;

  out->route_id = route_id.value_or(std::string());

  out->is_blocking = data.is_blocking();
  out->help_page_id = data.help_page_id();

  return true;
}

}  // namespace mojo
