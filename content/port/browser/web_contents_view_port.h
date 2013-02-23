// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PORT_BROWSER_WEB_CONTENTS_VIEW_PORT_H_
#define CONTENT_PORT_BROWSER_WEB_CONTENTS_VIEW_PORT_H_

#include "content/public/browser/web_contents_view.h"

namespace content {

// This is the larger WebContentsView interface exposed only within content/ and
// to embedders looking to port to new platforms.
class CONTENT_EXPORT WebContentsViewPort : public WebContentsView {
 public:
  virtual ~WebContentsViewPort() {}

  // Invoked when the WebContents is notified that the RenderView has been
  // swapped in.
  virtual void RenderViewSwappedIn(RenderViewHost* host) = 0;
};

}  // namespace content

#endif  // CONTENT_PORT_BROWSER_WEB_CONTENTS_VIEW_PORT_H_
