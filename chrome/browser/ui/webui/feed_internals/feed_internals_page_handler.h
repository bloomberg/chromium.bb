// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEED_INTERNALS_FEED_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_FEED_INTERNALS_FEED_INTERNALS_PAGE_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/feed_internals/feed_internals.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace feed {
class FeedHostService;
class FeedSchedulerHost;
}  // namespace feed

// Concrete implementation of feed_internals::mojom::PageHandler.
class FeedInternalsPageHandler : public feed_internals::mojom::PageHandler {
 public:
  FeedInternalsPageHandler(feed_internals::mojom::PageHandlerRequest request,
                           feed::FeedHostService* feed_host_service);
  ~FeedInternalsPageHandler() override;

  // feed_internals::mojom::PageHandler
  void GetGeneralProperties(GetGeneralPropertiesCallback) override;
  void GetUserClassifierProperties(
      GetUserClassifierPropertiesCallback) override;
  void ClearUserClassifierProperties() override;

 private:
  // Binding from the mojo interface to concrete implementation.
  mojo::Binding<feed_internals::mojom::PageHandler> binding_;

  // Services that provide the data and functionality.
  feed::FeedSchedulerHost* feed_scheduler_host_;

  DISALLOW_COPY_AND_ASSIGN(FeedInternalsPageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_FEED_INTERNALS_FEED_INTERNALS_PAGE_HANDLER_H_
