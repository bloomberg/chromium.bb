// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_TEST_BASE_SAMPLE_ACTIVITY_H_
#define ATHENA_TEST_BASE_SAMPLE_ACTIVITY_H_

#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_view_model.h"
#include "base/memory/scoped_ptr.h"

namespace gfx {
class ImageSkia;
}

namespace athena {
class AcceleratorManager;

namespace test {

class SampleActivity : public Activity,
                       public ActivityViewModel {
 public:
  SampleActivity(SkColor color,
                 SkColor contents_color,
                 const base::string16& title);
  ~SampleActivity() override;

  // athena::Activity:
  athena::ActivityViewModel* GetActivityViewModel() override;
  void SetCurrentState(Activity::ActivityState state) override;
  ActivityState GetCurrentState() override;
  bool IsVisible() override;
  ActivityMediaState GetMediaState() override;
  aura::Window* GetWindow() override;
  content::WebContents* GetWebContents() override;

  // athena::ActivityViewModel:
  void Init() override;
  SkColor GetRepresentativeColor() const override;
  base::string16 GetTitle() const override;
  gfx::ImageSkia GetIcon() const override;
  void SetActivityView(ActivityView* activity_view) override;
  bool UsesFrame() const override;
  views::View* GetContentsView() override;
  gfx::ImageSkia GetOverviewModeImage() override;
  void PrepareContentsForOverview() override;
  void ResetContentsView() override;

 private:
  SkColor color_;
  SkColor contents_color_;
  base::string16 title_;
  views::View* contents_view_;

  // The current state for this activity.
  ActivityState current_state_;

  // This is to emulate real implementation.
  scoped_ptr<AcceleratorManager> accelerator_manager_;

  DISALLOW_COPY_AND_ASSIGN(SampleActivity);
};

}  // namespace test
}  // namespace athena

#endif  // ATHENA_TEST_BASE_SAMPLE_ACTIVITY_H_
