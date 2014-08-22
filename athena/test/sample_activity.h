// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_TEST_SAMPLE_ACTIVITY_H_
#define ATHENA_TEST_SAMPLE_ACTIVITY_H_

#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_view_model.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/image/image_skia.h"

namespace athena {
namespace test {

class SampleActivity : public Activity,
                       public ActivityViewModel {
 public:
  SampleActivity(SkColor color,
                 SkColor contents_color,
                 const base::string16& title);
  virtual ~SampleActivity();

 private:
  // athena::Activity:
  virtual athena::ActivityViewModel* GetActivityViewModel() OVERRIDE;
  virtual void SetCurrentState(Activity::ActivityState state) OVERRIDE;
  virtual ActivityState GetCurrentState() OVERRIDE;
  virtual bool IsVisible() OVERRIDE;
  virtual ActivityMediaState GetMediaState() OVERRIDE;
  virtual aura::Window* GetWindow() OVERRIDE;

  // athena::ActivityViewModel:
  virtual void Init() OVERRIDE;
  virtual SkColor GetRepresentativeColor() const OVERRIDE;
  virtual base::string16 GetTitle() const OVERRIDE;
  virtual bool UsesFrame() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual void CreateOverviewModeImage() OVERRIDE;
  virtual gfx::ImageSkia GetOverviewModeImage() OVERRIDE;

  SkColor color_;
  SkColor contents_color_;
  base::string16 title_;
  views::View* contents_view_;

  // The current state for this activity.
  ActivityState current_state_;

  DISALLOW_COPY_AND_ASSIGN(SampleActivity);
};

}  // namespace test
}  // namespace athena

#endif  // ATHENA_TEST_SAMPLE_ACTIVITY_H_
