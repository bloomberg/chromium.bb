// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_NATIVE_TAB_CONTENTS_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_NATIVE_TAB_CONTENTS_CONTAINER_H_
#pragma once

class RenderViewHost;
class TabContentsContainer;

namespace content {
class WebContents;
}

namespace views {
class View;
}

// An interface that the TabContentsContainer uses to talk to a platform-
// specific view that hosts the native handle of the WebContents' view.
class NativeTabContentsContainer {
 public:
  // Creates an appropriate native container for the current platform.
  static NativeTabContentsContainer* CreateNativeContainer(
      TabContentsContainer* container);

  // Attaches the new WebContents to the native container.
  virtual void AttachContents(content::WebContents* contents) = 0;

  // Detaches the old WebContents from the native container.
  virtual void DetachContents(content::WebContents* contents) = 0;

  // Tells the container to update less frequently during resizing operations
  // so performance is better.
  virtual void SetFastResize(bool fast_resize) = 0;
  virtual bool GetFastResize() const = 0;

  // Returns the value of GetFastResize() the last time layout occurred.
  virtual bool FastResizeAtLastLayout() const = 0;

  // Tells the container that the RenderViewHost for the attached WebContents
  // has changed and it should update focus.
  virtual void RenderViewHostChanged(RenderViewHost* old_host,
                                     RenderViewHost* new_host) = 0;

  // Tells the container that |contents| got the focus.
  virtual void WebContentsFocused(content::WebContents* contents) = 0;

  // Retrieves the views::View that hosts the TabContents.
  virtual views::View* GetView() = 0;

 protected:
  virtual ~NativeTabContentsContainer() {}
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_NATIVE_TAB_CONTENTS_CONTAINER_H_
