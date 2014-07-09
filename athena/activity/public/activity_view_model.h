// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_ACTIVITY_PUBLIC_ACTIVITY_VIEW_MODEL_H_
#define ATHENA_ACTIVITY_PUBLIC_ACTIVITY_VIEW_MODEL_H_

#include "athena/common/athena_export.h"
#include "base/strings/string16.h"

typedef unsigned int SkColor;

namespace views {
class View;
}

namespace athena {

class ATHENA_EXPORT ActivityViewModel {
 public:
  virtual ~ActivityViewModel() {}

  // Called after the view model is attaced to the widget/window tree.
  virtual void Init() = 0;

  // Returns a color most representative of this activity.
  virtual SkColor GetRepresentativeColor() = 0;

  // Returns a title for the activity.
  virtual base::string16 GetTitle() = 0;

  // Returns the contents view.
  virtual views::View* GetContentsView() = 0;
};

}  // namespace athena

#endif  // ATHENA_ACTIVITY_PUBLIC_ACTIVITY_VIEW_MODEL_H_
