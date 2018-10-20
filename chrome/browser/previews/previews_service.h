// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_SERVICE_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace optimization_guide {
class OptimizationGuideService;
}

namespace previews {
class PreviewsDeciderImpl;
class PreviewsUIService;
}

class PreviewsLitePageDecider;

// Keyed service that owns a previews::PreviewsUIService. PreviewsService lives
// on the UI thread.
class PreviewsService : public KeyedService {
 public:
  explicit PreviewsService(content::BrowserContext* browser_context);
  ~PreviewsService() override;

  // Initializes the UI Service. |previews_decider_impl| is the main previews IO
  // object, and cannot be null. |optimization_guide_service| is the
  // Optimization Guide Service that is being listened to and is guaranteed to
  // outlive |this|. |io_task_runner| is the IO thread task runner.
  // |profile_path| is the path to user data on disc.
  void Initialize(
      previews::PreviewsDeciderImpl* previews_decider_impl,
      optimization_guide::OptimizationGuideService* optimization_guide_service,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      const base::FilePath& profile_path);

  // Allows the |previews_lite_page_decider_| to remove itself from observed
  // classes.
  void Shutdown() override;

  // The previews UI thread service.
  previews::PreviewsUIService* previews_ui_service() {
    return previews_ui_service_.get();
  }

  // The server lite page preview decider.
  PreviewsLitePageDecider* previews_lite_page_decider() {
    return previews_lite_page_decider_.get();
  }

 private:
  // The previews UI thread service.
  std::unique_ptr<previews::PreviewsUIService> previews_ui_service_;

  // The server lite page preview decider.
  std::unique_ptr<PreviewsLitePageDecider> previews_lite_page_decider_;

  DISALLOW_COPY_AND_ASSIGN(PreviewsService);
};

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_SERVICE_H_
