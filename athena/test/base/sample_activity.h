// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_TEST_BASE_SAMPLE_ACTIVITY_H_
#define ATHENA_TEST_BASE_SAMPLE_ACTIVITY_H_

#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_view_model.h"

namespace gfx {
class ImageSkia;
}

namespace athena {
namespace test {

class SampleActivity : public Activity,
                       public ActivityViewModel {
 public:
  SampleActivity(SkColor color,
                 SkColor contents_color,
                 const base::string16& title);
  virtual ~SampleActivity();

  // athena::Activity:
  virtual athena::ActivityViewModel* GetActivityViewModel() override;
  virtual void SetCurrentState(Activity::ActivityState state) override;
  virtual ActivityState GetCurrentState() override;
  virtual bool IsVisible() override;
  virtual ActivityMediaState GetMediaState() override;
  virtual aura::Window* GetWindow() override;
  virtual content::WebContents* GetWebContents() override;

  // athena::ActivityViewModel:
  virtual void Init() override;
  virtual SkColor GetRepresentativeColor() const override;
  virtual base::string16 GetTitle() const override;
  virtual gfx::ImageSkia GetIcon() const override;
  virtual bool UsesFrame() const override;
  virtual views::View* GetContentsView() override;
  virtual views::Widget* CreateWidget() override;
  virtual gfx::ImageSkia GetOverviewModeImage() override;
  virtual void PrepareContentsForOverview() override;
  virtual void ResetContentsView() override;

 private:
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

#endif  // ATHENA_TEST_BASE_SAMPLE_ACTIVITY_H_
