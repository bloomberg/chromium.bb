// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEED_INTERNALS_FEED_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_FEED_INTERNALS_FEED_INTERNALS_PAGE_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/feed_internals/feed_internals.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

// Concrete implementation of feed_internals::mojom::PageHandler.
class FeedInternalsPageHandler : public feed_internals::mojom::PageHandler {
 public:
  explicit FeedInternalsPageHandler(
      feed_internals::mojom::PageHandlerRequest request);
  ~FeedInternalsPageHandler() override;

 private:
  mojo::Binding<feed_internals::mojom::PageHandler> binding_;

  DISALLOW_COPY_AND_ASSIGN(FeedInternalsPageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_FEED_INTERNALS_FEED_INTERNALS_PAGE_HANDLER_H_
