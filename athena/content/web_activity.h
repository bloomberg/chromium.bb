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
  virtual content::WebContents* GetWebContents() OVERRIDE;

  // ActivityViewModel:
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

  // content::WebContentsObserver:
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;
  virtual void TitleWasSet(content::NavigationEntry* entry,
                           bool explicit_set) OVERRIDE;
  virtual void DidUpdateFaviconURL(
      const std::vector<content::FaviconURL>& candidates) OVERRIDE;
  virtual void DidChangeThemeColor(SkColor theme_color) OVERRIDE;

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

  // Reload the content if required, and start observing it.
  void ReloadAndObserve();

  content::BrowserContext* browser_context_;
  const base::string16 title_;
  gfx::ImageSkia icon_;
  const GURL url_;
  AthenaWebView* web_view_;
  SkColor title_color_;

  // The current state for this activity.
  ActivityState current_state_;

  // The content proxy.
  scoped_ptr<ContentProxy> content_proxy_;

  base::WeakPtrFactory<WebActivity> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebActivity);
};

}  // namespace athena

#endif  // ATHENA_CONTENT_WEB_ACTIVITY_H_
