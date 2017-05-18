// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_RESOURCE_COORDINATOR_WEB_CONTENTS_OBSERVER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_RESOURCE_COORDINATOR_WEB_CONTENTS_OBSERVER_H_

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_interface.h"

class ResourceCoordinatorWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<
          ResourceCoordinatorWebContentsObserver> {
 public:
  ~ResourceCoordinatorWebContentsObserver() override;

  static bool IsEnabled();

  resource_coordinator::ResourceCoordinatorInterface*
  tab_resource_coordinator() {
    return tab_resource_coordinator_.get();
  }

  // WebContentsObserver implementation.
  void WasShown() override;
  void WasHidden() override;

 private:
  explicit ResourceCoordinatorWebContentsObserver(
      content::WebContents* web_contents);

  friend class content::WebContentsUserData<
      ResourceCoordinatorWebContentsObserver>;

  std::unique_ptr<resource_coordinator::ResourceCoordinatorInterface>
      tab_resource_coordinator_;

  DISALLOW_COPY_AND_ASSIGN(ResourceCoordinatorWebContentsObserver);
};

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_RESOURCE_COORDINATOR_WEB_CONTENTS_OBSERVER_H_
