// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_CONTENT_APP_ACTIVITY_H_
#define ATHENA_CONTENT_APP_ACTIVITY_H_

#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_view_model.h"
#include "athena/content/app_activity_proxy.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_contents_observer.h"

namespace views {
class WebView;
}

namespace athena {

class AppActivityRegistry;
class ContentProxy;

// The activity object for a hosted V2 application.
class AppActivity : public Activity,
                    public ActivityViewModel,
                    public content::WebContentsObserver {
 public:
  AppActivity(const std::string& app_id, views::WebView* web_view);

  // Gets the content proxy so that the AppActivityProxy can take it over.
  scoped_ptr<ContentProxy> GetContentProxy();

  // Activity:
  athena::ActivityViewModel* GetActivityViewModel() override;
  void SetCurrentState(Activity::ActivityState state) override;
  ActivityState GetCurrentState() override;
  bool IsVisible() override;
  ActivityMediaState GetMediaState() override;
  aura::Window* GetWindow() override;
  content::WebContents* GetWebContents() override;

  // ActivityViewModel:
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
  // Constructor for test.
  explicit AppActivity(const std::string& app_id);

  ~AppActivity() override;

 private:
 // content::WebContentsObserver:
  void TitleWasSet(content::NavigationEntry* entry, bool explicit_set) override;
  void DidUpdateFaviconURL(
      const std::vector<content::FaviconURL>& candidates) override;

  // Register this activity with its application.
  void RegisterActivity();

  // Hiding the contet proxy and showing the real content instead.
  void HideContentProxy();

  // Showing a content proxy instead of the real content to save resources.
  void ShowContentProxy();

  const std::string app_id_;

  views::WebView* web_view_;

  // The current state for this activity.
  ActivityState current_state_;

  // If known the registry which holds all activities for the associated app.
  // This object is owned by |AppRegistry| and will be a valid pointer as long
  // as this object lives.
  AppActivityRegistry* app_activity_registry_;

  // The content proxy.
  scoped_ptr<ContentProxy> content_proxy_;

  // WebActivity does not take ownership of |activity_view_|. If the view is
  // destroyed before the activity, then it must be reset using
  // SetActivityView(nullptr).
  ActivityView* activity_view_;

  DISALLOW_COPY_AND_ASSIGN(AppActivity);
};

}  // namespace athena

#endif  // ATHENA_CONTENT_APP_ACTIVITY_H_
