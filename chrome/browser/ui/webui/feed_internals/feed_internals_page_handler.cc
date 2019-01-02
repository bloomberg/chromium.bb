// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feed_internals/feed_internals_page_handler.h"

#include <utility>

FeedInternalsPageHandler::FeedInternalsPageHandler(
    feed_internals::mojom::PageHandlerRequest request)
    : binding_(this, std::move(request)) {}

FeedInternalsPageHandler::~FeedInternalsPageHandler() = default;
