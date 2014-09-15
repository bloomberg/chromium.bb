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
#include "ui/gfx/image/image_skia.h"

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
  explicit AppActivity(const std::string& app_id);

  // Gets the content proxy so that the AppProxy can take it over.
  scoped_ptr<ContentProxy> GetContentProxy(aura::Window* window);

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
  virtual gfx::ImageSkia GetIcon() const OVERRIDE;
  virtual bool UsesFrame() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
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
