/// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/activity/activity_manager_impl.h"

#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_factory.h"
#include "athena/activity/public/activity_view_model.h"
#include "athena/test/base/athena_test_base.h"
#include "athena/test/base/test_windows.h"
#include "ui/aura/window.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace athena {

typedef test::AthenaTestBase ActivityManagerTest;

class WidgetActivity : public Activity,
                       public ActivityViewModel {
 public:
  WidgetActivity() : initialized_(false) {
    widget_.reset(new views::Widget());
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget_->Init(params);
  }

  bool initialized() const { return initialized_; }

 private:
  ~WidgetActivity() override {}

  // athena::Activity:
  athena::ActivityViewModel* GetActivityViewModel() override { return this; }
  void SetCurrentState(Activity::ActivityState state) override {}
  ActivityState GetCurrentState() override { return ACTIVITY_VISIBLE; }
  bool IsVisible() override { return true; }
  ActivityMediaState GetMediaState() override {
    return ACTIVITY_MEDIA_STATE_NONE;
  }
  aura::Window* GetWindow() override { return widget_->GetNativeView(); }
  content::WebContents* GetWebContents() override { return nullptr; }

  // athena::ActivityViewModel:
  void Init() override {
    initialized_ = true;
  }

  SkColor GetRepresentativeColor() const override { return SK_ColorBLACK; }
  base::string16 GetTitle() const override { return base::string16(); }
  gfx::ImageSkia GetIcon() const override { return gfx::ImageSkia(); }
  void SetActivityView(ActivityView* activity_view) override {}
  bool UsesFrame() const override { return false; }
  views::View* GetContentsView() override { return nullptr; }
  gfx::ImageSkia GetOverviewModeImage() override { return gfx::ImageSkia(); }
  void PrepareContentsForOverview() override {}
  void ResetContentsView() override {}

  scoped_ptr<views::Widget> widget_;
  bool initialized_;

  DISALLOW_COPY_AND_ASSIGN(WidgetActivity);
};

TEST_F(ActivityManagerTest, Basic) {
  ActivityManagerImpl* activity_manager =
      static_cast<ActivityManagerImpl*>(ActivityManager::Get());
  ActivityFactory* factory = ActivityFactory::Get();

  Activity* activity1 =
      factory->CreateWebActivity(nullptr, base::string16(), GURL());
  EXPECT_EQ(1, activity_manager->num_activities());

  // Activity is not visible when created.
  EXPECT_FALSE(activity1->GetWindow()->TargetVisibility());
  Activity::Show(activity1);
  EXPECT_TRUE(activity1->GetWindow()->TargetVisibility());

  Activity* activity2 =
      factory->CreateWebActivity(nullptr, base::string16(), GURL());
  EXPECT_EQ(2, activity_manager->num_activities());

  Activity::Delete(activity1);
  EXPECT_EQ(1, activity_manager->num_activities());

  // Deleting the activity's window should delete the activity itself.
  delete activity2->GetWindow();
  EXPECT_EQ(0, activity_manager->num_activities());
}

TEST_F(ActivityManagerTest, GetActivityForWindow) {
  ActivityManager* manager = ActivityManager::Get();
  ActivityFactory* factory = ActivityFactory::Get();

  Activity* activity1 =
      factory->CreateWebActivity(nullptr, base::string16(), GURL());
  Activity* activity2 =
      factory->CreateWebActivity(nullptr, base::string16(), GURL());

  EXPECT_EQ(activity1, manager->GetActivityForWindow(activity1->GetWindow()));
  EXPECT_EQ(activity2, manager->GetActivityForWindow(activity2->GetWindow()));

  EXPECT_EQ(nullptr, manager->GetActivityForWindow(nullptr));

  scoped_ptr<aura::Window> window = test::CreateNormalWindow(nullptr, nullptr);
  EXPECT_EQ(nullptr, manager->GetActivityForWindow(window.get()));
}

TEST_F(ActivityManagerTest, ActivationBringsActivityToTop) {
  ActivityManager* manager = ActivityManager::Get();
  ActivityFactory* factory = ActivityFactory::Get();

  Activity* activity1 =
      factory->CreateWebActivity(nullptr, base::string16(), GURL());
  Activity* activity2 =
      factory->CreateWebActivity(nullptr, base::string16(), GURL());
  activity1->GetWindow()->Show();
  activity2->GetWindow()->Show();

  ASSERT_EQ(2u, manager->GetActivityList().size());
  EXPECT_EQ(activity2, manager->GetActivityList()[0]);
  EXPECT_EQ(activity1, manager->GetActivityList()[1]);

  wm::ActivateWindow(activity1->GetWindow());
  ASSERT_EQ(2u, manager->GetActivityList().size());
  EXPECT_EQ(activity1, manager->GetActivityList()[0]);
  EXPECT_EQ(activity2, manager->GetActivityList()[1]);
}

TEST_F(ActivityManagerTest, WidgetActivityModelIsInitialized) {
  ActivityManager* manager = ActivityManager::Get();
  WidgetActivity* activity = new WidgetActivity();
  manager->AddActivity(activity);
  EXPECT_TRUE(activity->initialized());
  Activity::Delete(activity);
}

}  // namespace athena
