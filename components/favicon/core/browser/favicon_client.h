// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_BROWSER_FAVICON_CLIENT_H_
#define COMPONENTS_FAVICON_CORE_BROWSER_FAVICON_CLIENT_H_

class FaviconService;

// This class abstracts operations that depend on the embedder's environment,
// e.g.
// Chrome.
class FaviconClient {
 public:
  virtual FaviconService* GetFaviconService() = 0;
};

#endif  // COMPONENTS_FAVICON_CORE_BROWSER_FAVICON_CLIENT_H_
