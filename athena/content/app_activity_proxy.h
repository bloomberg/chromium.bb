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

 protected:
  virtual ~AppActivityProxy();

 // Activity overrides:
  virtual ActivityViewModel* GetActivityViewModel() override;
  virtual void SetCurrentState(ActivityState state) override;
  virtual ActivityState GetCurrentState() override;
  virtual bool IsVisible() override;
  virtual ActivityMediaState GetMediaState() override;
  virtual aura::Window* GetWindow() override;
  virtual content::WebContents* GetWebContents() override;

  // ActivityViewModel overrides:
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

  DISALLOW_COPY_AND_ASSIGN(AppActivityProxy);
};

}  // namespace athena

#endif  // ATHENA_CONTENT_APP_ACTIVITY_PROXY_H_
