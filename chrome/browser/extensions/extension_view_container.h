// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_CONTAINER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_CONTAINER_H_

class ExtensionView;

namespace gfx {
class Size;
}

// A class that represents the container that this extension view is in.
// (bottom shelf, side bar, etc.)
class ExtensionViewContainer {
 public:
  virtual ~ExtensionViewContainer() {}

  virtual void OnExtensionSizeChanged(ExtensionView* view,
                                      const gfx::Size& new_size) = 0;

  virtual void OnExtensionViewDidShow(ExtensionView* view) = 0;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_VIEW_CONTAINER_H_
