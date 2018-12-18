// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WAITING_H_
#define MEDIA_BASE_WAITING_H_

#include "base/callback_forward.h"

namespace media {

// Here "waiting" refers to the state that media pipeline stalls waiting because
// of some reason, e.g. no decryption key. It could cause Javascript events like
// "waitingforkey" [1], but not necessarily.
// Note: this generally does not cause the "waiting" event on HTML5 media
// elements [2], which is tightly related to the buffering state change (see
// buffering_state.h).
// [1] https://www.w3.org/TR/encrypted-media/#dom-evt-waitingforkey
// [2]
// https://www.w3.org/TR/html5/semantics-embedded-content.html#eventdef-media-waiting

// TODO(xhwang): Add more waiting reasons.
enum class WaitingReason {
  kNoDecryptionKey,
  kMaxValue = kNoDecryptionKey,
};

// Callback to notify waiting state and the reason.
using WaitingCB = base::RepeatingCallback<void(WaitingReason)>;

}  // namespace media

#endif  // MEDIA_BASE_WAITING_H_
