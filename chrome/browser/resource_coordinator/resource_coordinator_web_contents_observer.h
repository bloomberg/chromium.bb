// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_RESOURCE_COORDINATOR_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_RESOURCE_COORDINATOR_WEB_CONTENTS_OBSERVER_H_

#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_interface.h"
#include "services/resource_coordinator/public/interfaces/service_callbacks.mojom.h"
#include "url/gurl.h"

class ResourceCoordinatorWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<
          ResourceCoordinatorWebContentsObserver> {
 public:
  ~ResourceCoordinatorWebContentsObserver() override;

  static bool ukm_recorder_initialized;

  static bool IsEnabled();

  resource_coordinator::ResourceCoordinatorInterface*
  tab_resource_coordinator() {
    return tab_resource_coordinator_.get();
  }

  // WebContentsObserver implementation.
  void WasShown() override;
  void WasHidden() override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void TitleWasSet(content::NavigationEntry* entry, bool explicit_set) override;
  void DidUpdateFaviconURL(
      const std::vector<content::FaviconURL>& candidates) override;

  void EnsureUkmRecorderInterface();
  void MaybeSetUkmRecorderInterface(bool ukm_recorder_already_initialized);
  void UpdateUkmRecorder(int64_t navigation_id);

 private:
  explicit ResourceCoordinatorWebContentsObserver(
      content::WebContents* web_contents);
  // Favicon, title are set the first time a page is loaded, thus we want to
  // ignore the very first update, and reset the flags when a non same-document
  // navigation finished in main frame.
  void ResetFlag();

  friend class content::WebContentsUserData<
      ResourceCoordinatorWebContentsObserver>;

  std::unique_ptr<resource_coordinator::ResourceCoordinatorInterface>
      tab_resource_coordinator_;
  ukm::SourceId ukm_source_id_;

  // We only want to send signals to GRC 5 minutes after the main frame is
  // committed. Thus we record the time when navigation of main frame is
  // finished, when title/favicon is changed, we check whether it's already more
  // than 5 minutes from the time navigation of main frame is finished.
  base::TimeTicks navigation_finished_time_;

  // Favicon and title are set when a page is loaded, we only want to send
  // signals to GRC about title and favicon update from the previous title and
  // favicon, thus we want to ignore the very first update since it is always
  // supposed to happen.
  bool first_time_favicon_set_ = false;
  bool first_time_title_set_ = false;

  resource_coordinator::mojom::ServiceCallbacksPtr service_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(ResourceCoordinatorWebContentsObserver);
};

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_RESOURCE_COORDINATOR_WEB_CONTENTS_OBSERVER_H_
