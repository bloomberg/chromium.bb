// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_ACTIVITY_PUBLIC_ACTIVITY_VIEW_MODEL_H_
#define ATHENA_ACTIVITY_PUBLIC_ACTIVITY_VIEW_MODEL_H_

#include "athena/athena_export.h"
#include "base/strings/string16.h"

typedef unsigned int SkColor;

namespace gfx {
class ImageSkia;
}

namespace views {
class View;
}

namespace athena {

// The view model for the representation of the activity.
class ATHENA_EXPORT ActivityViewModel {
 public:
  virtual ~ActivityViewModel() {}

  // Called after the view model is attached to the widget/window tree.
  virtual void Init() = 0;

  // Returns a color most representative of this activity.
  virtual SkColor GetRepresentativeColor() const = 0;

  // Returns a title for the activity.
  virtual base::string16 GetTitle() const = 0;

  // True if the activity wants to use Widget's frame, or false if the activity
  // draws its own frame.
  virtual bool UsesFrame() const = 0;

  // Returns the contents view which might be NULL if the activity is not
  // loaded. Note that the caller should not hold on to the view since it can
  // be deleted by the resource manager.
  virtual views::View* GetContentsView() = 0;

  // This gets called before the Activity gets (partially) thrown out of memory
  // to create a preview image of the activity. Note that even if this function
  // gets called, |GetOverviewModeImage()| could still return an empty image.
  virtual void CreateOverviewModeImage() = 0;

  // Returns an image which can be used to represent the activity in e.g. the
  // overview mode. The returned image can have no size if either a view exists
  // or the activity has not yet been loaded. In that case
  // GetRepresentativeColor() should be used to clear the preview area.
  // Note: We intentionally do not use a layer / view for this.
  virtual gfx::ImageSkia GetOverviewModeImage() = 0;
};

}  // namespace athena

#endif  // ATHENA_ACTIVITY_PUBLIC_ACTIVITY_VIEW_MODEL_H_
