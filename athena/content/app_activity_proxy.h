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
  virtual ActivityViewModel* GetActivityViewModel() OVERRIDE;
  virtual void SetCurrentState(ActivityState state) OVERRIDE;
  virtual ActivityState GetCurrentState() OVERRIDE;
  virtual bool IsVisible() OVERRIDE;
  virtual ActivityMediaState GetMediaState() OVERRIDE;
  virtual aura::Window* GetWindow() OVERRIDE;
  virtual content::WebContents* GetWebContents() OVERRIDE;

  // ActivityViewModel overrides:
  virtual void Init() OVERRIDE;
  virtual SkColor GetRepresentativeColor() const OVERRIDE;
  virtual base::string16 GetTitle() const OVERRIDE;
  virtual gfx::ImageSkia GetIcon() const OVERRIDE;
  virtual bool UsesFrame() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual views::Widget* CreateWidget() OVERRIDE;
  virtual gfx::ImageSkia GetOverviewModeImage() OVERRIDE;
  virtual void PrepareContentsForOverview() OVERRIDE;
  virtual void ResetContentsView() OVERRIDE;

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
