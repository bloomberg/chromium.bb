// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/guest_view/guest_view_constants.h"

namespace guestview {

// Sizing Attributes.
const char kAttributeAutoSize[] = "autosize";
const char kAttributeMaxHeight[] = "maxheight";
const char kAttributeMaxWidth[] = "maxwidth";
const char kAttributeMinHeight[] = "minheight";
const char kAttributeMinWidth[] = "minwidth";

// Parameters/properties on events.
const char kIsTopLevel[] = "isTopLevel";
const char kReason[] = "reason";
const char kUrl[] = "url";
const char kUserGesture[] = "userGesture";

// Initialization parameters.
const char kParameterApi[] = "api";
const char kParameterInstanceId[] = "instanceId";

// Other.
const char kGuestViewManagerKeyName[] = "guest_view_manager";
const int kInstanceIDNone = 0;

}  // namespace guestview
