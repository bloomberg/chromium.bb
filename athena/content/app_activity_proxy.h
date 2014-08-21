// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_CONTENT_APP_ACTIVITY_PROXY_H_
#define ATHENA_CONTENT_APP_ACTIVITY_PROXY_H_

#include <vector>

#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_view_model.h"
#include "athena/content/app_activity_proxy.h"
#include "ui/gfx/image/image_skia.h"

namespace athena {

class AppActivityRegistry;

// This activity object is a proxy placeholder for the application while it is
// unloaded. When selected it will launch the applciation again and destroy
// itself indirectly.
class AppActivityProxy : public Activity,
                         public ActivityViewModel {
 public:
  AppActivityProxy(ActivityViewModel* view_model, AppActivityRegistry* creator);
  virtual ~AppActivityProxy();

  // Activity overrides:
  virtual ActivityViewModel* GetActivityViewModel() OVERRIDE;
  virtual void SetCurrentState(ActivityState state) OVERRIDE;
  virtual ActivityState GetCurrentState() OVERRIDE;
  virtual bool IsVisible() OVERRIDE;
  virtual ActivityMediaState GetMediaState() OVERRIDE;
  virtual aura::Window* GetWindow() OVERRIDE;

  // ActivityViewModel overrides:
  virtual void Init() OVERRIDE;
  virtual SkColor GetRepresentativeColor() const OVERRIDE;
  virtual base::string16 GetTitle() const OVERRIDE;
  virtual bool UsesFrame() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual void CreateOverviewModeImage() OVERRIDE;
  virtual gfx::ImageSkia GetOverviewModeImage() OVERRIDE;

 private:
  // The creator of this object which needs to be informed if the object gets
  // destroyed or the application should get restarted.
  AppActivityRegistry* app_activity_registry_;

  // The presentation values.
  const base::string16 title_;
  const gfx::ImageSkia image_;
  const SkColor color_;

  // The associated view.
  views::View* view_;

  DISALLOW_COPY_AND_ASSIGN(AppActivityProxy);
};

}  // namespace athena

#endif  // ATHENA_CONTENT_APP_ACTIVITY_PROXY_H_
