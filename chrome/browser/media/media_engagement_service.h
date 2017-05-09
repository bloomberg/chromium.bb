// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_SERVICE_H_
#define CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_SERVICE_H_

#include <set>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

class MediaEngagementService : public KeyedService {
 public:
  // Returns the instance attached to the given |profile|.
  static MediaEngagementService* Get(Profile* profile);

  // Returns whether the feature is enabled.
  static bool IsEnabled();

  // Observe the given |web_contents| by creating an internal
  // WebContentsObserver.
  static void CreateWebContentsObserver(content::WebContents* web_contents);

  explicit MediaEngagementService(Profile* profile);
  ~MediaEngagementService() override;

 private:
  class ContentsObserver;

  std::set<ContentsObserver*> contents_observers_;

  DISALLOW_COPY_AND_ASSIGN(MediaEngagementService);
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_ENGAGEMENT_SERVICE_H_
