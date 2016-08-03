// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_DESKTOP_ENGAGEMENT_DESKTOP_ENGAGEMENT_OBSERVER_H_
#define CHROME_BROWSER_METRICS_DESKTOP_ENGAGEMENT_DESKTOP_ENGAGEMENT_OBSERVER_H_

#include "base/macros.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace metrics {

class DesktopEngagementService;

// Tracks user input events from web contents and notifies
// |DesktopEngagementService|.
class DesktopEngagementObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<DesktopEngagementObserver>,
      public content::RenderWidgetHost::InputEventObserver {
 public:
  DesktopEngagementObserver(content::WebContents* web_contents,
                            DesktopEngagementService* service);
  ~DesktopEngagementObserver() override;

  static DesktopEngagementObserver* CreateForWebContents(
      content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<DesktopEngagementObserver>;

  // Register / Unregister input event callback to given RenderViewHost
  void RegisterInputEventObserver(content::RenderViewHost* host);
  void UnregisterInputEventObserver(content::RenderViewHost* host);

  // content::RenderWidgetHost::InputEventObserver:
  void OnInputEvent(const blink::WebInputEvent& event) override;

  // content::WebContentsObserver:
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;

  DesktopEngagementService* service_;

  DISALLOW_COPY_AND_ASSIGN(DesktopEngagementObserver);
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_DESKTOP_ENGAGEMENT_DESKTOP_ENGAGEMENT_OBSERVER_H_
