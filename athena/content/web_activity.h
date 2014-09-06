// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_CONTENT_PUBLIC_WEB_ACTIVITY_H_
#define ATHENA_CONTENT_PUBLIC_WEB_ACTIVITY_H_

#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_view_model.h"
#include "base/strings/string16.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/image/image_skia.h"

namespace content {
class BrowserContext;
class WebContents;
}

namespace views {
class WebView;
class WidgetDelegate;
}

namespace athena {

class AthenaWebView;

class WebActivity : public Activity,
                    public ActivityViewModel,
                    public content::WebContentsObserver {
 public:
  WebActivity(content::BrowserContext* context,
              const base::string16& title,
              const GURL& gurl);
  WebActivity(AthenaWebView* web_view);

 protected:
  virtual ~WebActivity();

 // Activity:
  virtual athena::ActivityViewModel* GetActivityViewModel() OVERRIDE;
  virtual void SetCurrentState(ActivityState state) OVERRIDE;
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

  // content::WebContentsObserver:
  virtual void TitleWasSet(content::NavigationEntry* entry,
                           bool explicit_set) OVERRIDE;
  virtual void DidUpdateFaviconURL(
      const std::vector<content::FaviconURL>& candidates) OVERRIDE;
  virtual void DidChangeThemeColor(SkColor theme_color) OVERRIDE;

 private:
  // Make the content visible. This call should only be paired with
  // MakeInvisible. Note: Upon object creation the content is visible.
  void MakeVisible();

  // Make the content invisible. This call should only be paired with
  // MakeVisible.
  void MakeInvisible();

  // Reload the content if required, and start observing it.
  void ReloadAndObserve();

  content::BrowserContext* browser_context_;
  const base::string16 title_;
  const GURL url_;
  AthenaWebView* web_view_;
  SkColor title_color_;

  // The current state for this activity.
  ActivityState current_state_;

  // The image which will be used in overview mode.
  gfx::ImageSkia overview_mode_image_;

  DISALLOW_COPY_AND_ASSIGN(WebActivity);
};

}  // namespace athena

#endif  // ATHENA_CONTENT_WEB_ACTIVITY_H_
