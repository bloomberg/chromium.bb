// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/autofill_change.h"

bool AutofillChange::operator==(const AutofillChange& change) const {
  return type_ == change.type() && key_ == change.key();
}
