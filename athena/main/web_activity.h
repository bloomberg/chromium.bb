// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_MAIN_WEB_ACTIVITY_H_
#define ATHENA_MAIN_WEB_ACTIVITY_H_

#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_view_model.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}

class WebActivity : public athena::Activity,
                    public athena::ActivityViewModel,
                    public content::WebContentsObserver {
 public:
  explicit WebActivity(content::WebContents* contents);
  virtual ~WebActivity();

 protected:
  // athena::Activity:
  virtual athena::ActivityViewModel* GetActivityViewModel() OVERRIDE;

  // athena::ActivityViewModel:
  virtual SkColor GetRepresentativeColor() OVERRIDE;
  virtual std::string GetTitle() OVERRIDE;
  virtual aura::Window* GetNativeWindow() OVERRIDE;

  // content::WebContentsObserver:
  virtual void TitleWasSet(content::NavigationEntry* entry,
                           bool explicit_set) OVERRIDE;
  virtual void DidUpdateFaviconURL(
      const std::vector<content::FaviconURL>& candidates) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebActivity);
};

#endif  // ATHENA_MAIN_WEB_ACTIVITY_H_
