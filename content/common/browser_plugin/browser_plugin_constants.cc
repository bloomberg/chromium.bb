// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/browser_plugin/browser_plugin_constants.h"

namespace content {

namespace browser_plugin {

// Internal method bindings.
const char kMethodInternalAttach[] = "-internal-attach";

// Internal events.
const char kEventInternalInstanceIDAllocated[] = "instanceid-allocated";

// Attributes.
const char kAttributeAllowTransparency[] = "allowtransparency";
const char kAttributeAutoSize[] = "autosize";
const char kAttributeContentWindow[] = "contentWindow";
const char kAttributeMaxHeight[] = "maxheight";
const char kAttributeMaxWidth[] = "maxwidth";
const char kAttributeMinHeight[] = "minheight";
const char kAttributeMinWidth[] = "minwidth";
const char kAttributeName[] = "name";
const char kAttributePartition[] = "partition";
const char kAttributeSrc[] = "src";

// Parameters/properties on events.
const char kPersistPrefix[] = "persist:";
const char kWindowID[] = "windowId";

// Error messages.
const char kErrorAlreadyNavigated[] =
    "The object has already navigated, so its partition cannot be changed.";
const char kErrorInvalidPartition[] =
    "Invalid partition attribute.";
const char kErrorCannotRemovePartition[] =
    "Cannot remove partition attribute after navigating.";

// Other.
const int kInstanceIDNone = 0;
const int kInvalidPermissionRequestID = 0;

}  // namespace browser_plugin

}  // namespace content
