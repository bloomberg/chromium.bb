// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONTENT_BROWSER_CLIENT_H_
#define CONTENT_BROWSER_CONTENT_BROWSER_CLIENT_H_
#pragma once

class GURL;
class Profile;
class RenderViewHost;

namespace content {

// Embedder API for participating in browser logic.
class ContentBrowserClient {
 public:
  // Initialize a RenderViewHost before its CreateRenderView method is called.
  virtual void OnRenderViewCreation(RenderViewHost* render_view_host,
                                    Profile* profile,
                                    const GURL& url) {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONTENT_BROWSER_CLIENT_H_
