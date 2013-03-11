// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autocheckout_bubble.h"

#include "base/logging.h"

namespace autofill {

AutocheckoutBubble::~AutocheckoutBubble() {}

#if !defined(TOOLKIT_VIEWS)
// TODO(ahutter): Implement the bubble on other platforms. See
// http://crbug.com/173416.
base::WeakPtr<AutocheckoutBubble> AutocheckoutBubble::Create(
    scoped_ptr<AutocheckoutBubbleController> controller) {
  NOTIMPLEMENTED();
  return base::WeakPtr<AutocheckoutBubble>();
}
#endif

}  // namespace autofill
