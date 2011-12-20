// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_H_
#pragma once

#include "base/string16.h"
#include "content/common/content_export.h"

// TODO(jam): of course we will have to rename TabContentsDelgate,
// TabContentsView etc to use WebContents.
class TabContentsDelegate;
class RenderViewHost;

namespace base {
class PropertyBag;
}

namespace content {

// Describes what goes in the main content area of a tab.
class WebContents {
 public:
  // Intrinsic tab state -------------------------------------------------------

  // Returns the property bag for this tab contents, where callers can add
  // extra data they may wish to associate with the tab. Returns a pointer
  // rather than a reference since the PropertyAccessors expect this.
  virtual const base::PropertyBag* GetPropertyBag() const = 0;
  virtual base::PropertyBag* GetPropertyBag() = 0;

  // Get/Set the delegate.
  virtual TabContentsDelegate* GetDelegate() = 0;
  virtual void SetDelegate(TabContentsDelegate* delegate) = 0;

  // Gets the current RenderViewHost for this tab.
  virtual RenderViewHost* GetRenderViewHost() const = 0;

};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_H_
