// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_dialogs.h"

#include "base/metrics/histogram_macros.h"

namespace chrome {

void RecordDialogCreation(DialogIdentifier identifier) {
  UMA_HISTOGRAM_ENUMERATION("Dialog.Creation", identifier,
                            DialogIdentifier::MAX_VALUE);
}

}  // namespace chrome
