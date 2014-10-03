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
  virtual athena::ActivityViewModel* GetActivityViewModel() override;
  virtual void SetCurrentState(Activity::ActivityState state) override;
  virtual ActivityState GetCurrentState() override;
  virtual bool IsVisible() override;
  virtual ActivityMediaState GetMediaState() override;
  virtual aura::Window* GetWindow() override;
  virtual content::WebContents* GetWebContents() override;

  // ActivityViewModel:
  virtual void Init() override;
  virtual SkColor GetRepresentativeColor() const override;
  virtual base::string16 GetTitle() const override;
  virtual gfx::ImageSkia GetIcon() const override;
  virtual bool UsesFrame() const override;
  virtual views::Widget* CreateWidget() override;
  virtual views::View* GetContentsView() override;
  virtual gfx::ImageSkia GetOverviewModeImage() override;
  virtual void PrepareContentsForOverview() override;
  virtual void ResetContentsView() override;

 protected:
  // Constructor for test.
  explicit AppActivity(const std::string& app_id);

  virtual ~AppActivity();

 private:
 // content::WebContentsObserver:
  virtual void TitleWasSet(content::NavigationEntry* entry,
                           bool explicit_set) override;
  virtual void DidUpdateFaviconURL(
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

  DISALLOW_COPY_AND_ASSIGN(AppActivity);
};

}  // namespace athena

#endif  // ATHENA_CONTENT_APP_ACTIVITY_H_
