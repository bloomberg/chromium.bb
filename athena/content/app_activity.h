// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_CONTENT_PUBLIC_APP_ACTIVITY_H_
#define ATHENA_CONTENT_PUBLIC_APP_ACTIVITY_H_

#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_view_model.h"
#include "content/public/browser/web_contents_observer.h"

namespace apps {
class ShellAppWindow;
}

namespace views {
class WebView;
}

namespace athena {

class AppActivity : public Activity,
                    public ActivityViewModel,
                    public content::WebContentsObserver {
 public:
  explicit AppActivity(apps::ShellAppWindow* app_window);
  virtual ~AppActivity();

 protected:
  // Activity:
  virtual athena::ActivityViewModel* GetActivityViewModel() OVERRIDE;

  // ActivityViewModel:
  virtual void Init() OVERRIDE;
  virtual SkColor GetRepresentativeColor() const OVERRIDE;
  virtual base::string16 GetTitle() const OVERRIDE;
  virtual bool UsesFrame() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;

  // content::WebContentsObserver:
  virtual void TitleWasSet(content::NavigationEntry* entry,
                           bool explicit_set) OVERRIDE;
  virtual void DidUpdateFaviconURL(
      const std::vector<content::FaviconURL>& candidates) OVERRIDE;

 private:
  scoped_ptr<apps::ShellAppWindow> app_window_;
  views::WebView* web_view_;

  DISALLOW_COPY_AND_ASSIGN(AppActivity);
};

}  // namespace athena

#endif  // ATHENA_CONTENT_APP_ACTIVITY_H_
