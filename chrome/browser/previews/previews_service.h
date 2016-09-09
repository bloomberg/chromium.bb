// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_SERVICE_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

namespace previews {
class PreviewsUIService;
}

// Keyed service that owns a previews::PreviewsUIService. PreviewsService lives
// on the UI thread.
class PreviewsService : public KeyedService {
 public:
  PreviewsService();
  ~PreviewsService() override;

  // The previews UI thread service.
  void set_previews_ui_service(
      std::unique_ptr<previews::PreviewsUIService> previews_ui_service);
  previews::PreviewsUIService* previews_ui_service() {
    return previews_ui_service_.get();
  }

 private:
  // The previews UI thread service.
  std::unique_ptr<previews::PreviewsUIService> previews_ui_service_;

  DISALLOW_COPY_AND_ASSIGN(PreviewsService);
};

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_SERVICE_H_
