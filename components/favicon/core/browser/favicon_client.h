// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_BROWSER_FAVICON_CLIENT_H_
#define COMPONENTS_FAVICON_CORE_BROWSER_FAVICON_CLIENT_H_

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;

// This class abstracts operations that depend on the embedder's environment,
// e.g. Chrome.
class FaviconClient : public KeyedService {
 public:
  // Returns true if the specified URL is bookmarked.
  virtual bool IsBookmarked(const GURL& url) = 0;

 protected:
  FaviconClient() {}
  ~FaviconClient() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FaviconClient);
};

#endif  // COMPONENTS_FAVICON_CORE_BROWSER_FAVICON_CLIENT_H_
