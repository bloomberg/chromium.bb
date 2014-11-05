// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_ACTIVITY_PUBLIC_ACTIVITY_VIEW_H_
#define ATHENA_ACTIVITY_PUBLIC_ACTIVITY_VIEW_H_

#include "athena/athena_export.h"

namespace athena {

class ATHENA_EXPORT ActivityView {
 public:
  virtual ~ActivityView() {}

  virtual void UpdateTitle() = 0;
  virtual void UpdateIcon() = 0;
  virtual void UpdateRepresentativeColor() = 0;
};

}  // namespace athena

#endif  // ATHENA_ACTIVITY_PUBLIC_ACTIVITY_VIEW_H_
