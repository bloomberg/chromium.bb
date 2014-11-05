// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_CONTENT_PUBLIC_WEB_ACTIVITY_H_
#define ATHENA_CONTENT_PUBLIC_WEB_ACTIVITY_H_

#include <vector>

#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_view_model.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/image/image_skia.h"

class SkBitmap;

namespace content {
class BrowserContext;
class WebContents;
}

namespace gfx {
class Size;
}

namespace views {
class WebView;
class WidgetDelegate;
}

namespace athena {

class AthenaWebView;
class ContentProxy;

class WebActivity : public Activity,
                    public ActivityViewModel,
                    public content::WebContentsObserver {
 public:
  WebActivity(content::BrowserContext* context,
              const base::string16& title,
              const GURL& gurl);
  explicit WebActivity(content::WebContents* contents);

 protected:
  ~WebActivity() override;

 // Activity:
  athena::ActivityViewModel* GetActivityViewModel() override;
  void SetCurrentState(ActivityState state) override;
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

  // content::WebContentsObserver:
  void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override;
  void TitleWasSet(content::NavigationEntry* entry, bool explicit_set) override;
  void DidUpdateFaviconURL(
      const std::vector<content::FaviconURL>& candidates) override;
  void DidChangeThemeColor(SkColor theme_color) override;

 private:
  // Called when a favicon download initiated in DidUpdateFaviconURL()
  // has completed.
  void OnDidDownloadFavicon(
      int id,
      int http_status_code,
      const GURL& url,
      const std::vector<SkBitmap>& bitmaps,
      const std::vector<gfx::Size>& original_bitmap_sizes);

  // Hiding the contet proxy and showing the real content instead.
  void HideContentProxy();

  // Showing a content proxy instead of the real content to save resoruces.
  void ShowContentProxy();

  content::BrowserContext* browser_context_;
  AthenaWebView* web_view_;
  const base::string16 title_;
  SkColor title_color_;
  gfx::ImageSkia icon_;

  // The current state for this activity.
  ActivityState current_state_;

  // The content proxy.
  scoped_ptr<ContentProxy> content_proxy_;

  // WebActivity does not take ownership of |activity_view_|. If the view is
  // destroyed before the activity, then it must be reset using
  // SetActivityView(nullptr).
  ActivityView* activity_view_;

  base::WeakPtrFactory<WebActivity> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebActivity);
};

}  // namespace athena

#endif  // ATHENA_CONTENT_WEB_ACTIVITY_H_
