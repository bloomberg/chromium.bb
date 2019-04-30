// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PUSH_MESSAGING_PUSH_MESSAGING_UTILS_H_
#define CONTENT_RENDERER_PUSH_MESSAGING_PUSH_MESSAGING_UTILS_H_

#include "content/common/push_messaging.mojom.h"
#include "third_party/blink/public/platform/modules/push_messaging/web_push_error.h"

namespace content {

namespace mojom {
enum class PushRegistrationStatus;
}  // namespace mojom

blink::WebPushError PushRegistrationStatusToWebPushError(
    mojom::PushRegistrationStatus status);

}  // namespace content

#endif  // CONTENT_RENDERER_PUSH_MESSAGING_PUSH_MESSAGING_UTILS_H_
