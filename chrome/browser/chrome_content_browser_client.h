// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_H_
#define CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_H_
#pragma once

#include "content/browser/content_browser_client.h"

namespace chrome {

class ChromeContentBrowserClient : public content::ContentBrowserClient {
 public:
  virtual void OnRenderViewCreation(RenderViewHost* render_view_host,
                                    Profile* profile,
                                    const GURL& url);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_CHROME_CONTENT_BROWSER_CLIENT_H_
