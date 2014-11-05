// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_CONTENT_APP_ACTIVITY_PROXY_H_
#define ATHENA_CONTENT_APP_ACTIVITY_PROXY_H_

#include <vector>

#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_view_model.h"
#include "athena/content/content_proxy.h"
#include "base/memory/scoped_ptr.h"

namespace athena {

class AppActivity;
class AppActivityRegistry;

// This activity object is a proxy placeholder for the application while it is
// unloaded. When selected it will launch the application again and destroy
// itself indirectly.
class AppActivityProxy : public Activity,
                         public ActivityViewModel {
 public:
  // The |replaced_activity| is the activity which this proxy replaces. Note
  // that after the Init() call got called, this object will become invalid.
  // The |creator| should be informed when the object goes away.
  AppActivityProxy(AppActivity* replaced_activity,
                   AppActivityRegistry* creator);

  // Activity overrides:
  ActivityViewModel* GetActivityViewModel() override;
  void SetCurrentState(ActivityState state) override;
  ActivityState GetCurrentState() override;
  bool IsVisible() override;
  ActivityMediaState GetMediaState() override;
  aura::Window* GetWindow() override;
  content::WebContents* GetWebContents() override;

  // ActivityViewModel overrides:
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

 protected:
  ~AppActivityProxy() override;

 private:
  // The creator of this object which needs to be informed if the object gets
  // destroyed or the application should get restarted.
  AppActivityRegistry* app_activity_registry_;

  // The presentation values.
  const base::string16 title_;
  const SkColor color_;

  // The activity which gets replaced. It is used to sort the activity against
  // upon initialization. Once moved, this value gets reset since the object
  // can go away at any time.
  AppActivity* replaced_activity_;

  // The associated view.
  views::View* view_;

  // The content proxy.
  scoped_ptr<ContentProxy> content_proxy_;

  // True if restart got already called.
  bool restart_called_;

  DISALLOW_COPY_AND_ASSIGN(AppActivityProxy);
};

}  // namespace athena

#endif  // ATHENA_CONTENT_APP_ACTIVITY_PROXY_H_
