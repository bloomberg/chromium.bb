// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_CONTENT_APP_ACTIVITY_H_
#define ATHENA_CONTENT_APP_ACTIVITY_H_

#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_view_model.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/image/image_skia.h"

namespace views {
class WebView;
}

namespace athena {

class AppActivityRegistry;

// The activity object for a hosted V2 application.
class AppActivity : public Activity,
                    public ActivityViewModel,
                    public content::WebContentsObserver {
 public:
  explicit AppActivity(const std::string& app_id);

  // Activity:
  virtual athena::ActivityViewModel* GetActivityViewModel() OVERRIDE;
  virtual void SetCurrentState(Activity::ActivityState state) OVERRIDE;
  virtual ActivityState GetCurrentState() OVERRIDE;
  virtual bool IsVisible() OVERRIDE;
  virtual ActivityMediaState GetMediaState() OVERRIDE;
  virtual aura::Window* GetWindow() OVERRIDE;

  // ActivityViewModel:
  virtual void Init() OVERRIDE;
  virtual SkColor GetRepresentativeColor() const OVERRIDE;
  virtual base::string16 GetTitle() const OVERRIDE;
  virtual bool UsesFrame() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual void CreateOverviewModeImage() OVERRIDE;
  virtual gfx::ImageSkia GetOverviewModeImage() OVERRIDE;
  virtual void PrepareContentsForOverview() OVERRIDE;
  virtual void ResetContentsView() OVERRIDE;

 protected:
  virtual ~AppActivity();

 // content::WebContentsObserver:
  virtual void TitleWasSet(content::NavigationEntry* entry,
                           bool explicit_set) OVERRIDE;
  virtual void DidUpdateFaviconURL(
      const std::vector<content::FaviconURL>& candidates) OVERRIDE;

  virtual views::WebView* GetWebView() = 0;

 private:
  // Register this activity with its application.
  void RegisterActivity();

  // Make the content visible. This call should only be paired with
  // MakeInvisible. Note: Upon start the content is visible.
  // TODO(skuhne): If this can be combined with web_activity, move this into a
  // separate class.
  void MakeVisible();

  // Make the content invisible. This call should only be paired with
  // MakeVisible.
  // TODO(skuhne): If this can be combined with web_activity, move this into a
  // separate class.
  void MakeInvisible();

  const std::string app_id_;

  views::WebView* web_view_;

  // The current state for this activity.
  ActivityState current_state_;

  // The image which will be used in overview mode.
  gfx::ImageSkia overview_mode_image_;

  // If known the registry which holds all activities for the associated app.
  // This object is owned by |AppRegistry| and will be a valid pointer as long
  // as this object lives.
  AppActivityRegistry* app_activity_registry_;

  DISALLOW_COPY_AND_ASSIGN(AppActivity);
};

}  // namespace athena

#endif  // ATHENA_CONTENT_APP_ACTIVITY_H_
