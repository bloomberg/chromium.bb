// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_CONTENT_CONTENT_PROXY_H_
#define ATHENA_CONTENT_CONTENT_PROXY_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/image/image_skia.h"

namespace aura {
class Window;
}

namespace views {
class View;
class WebView;
}

typedef unsigned int SkColor;

namespace athena {

class Activity;
class AppActivity;
class AppActivityProxy;
class WebActivity;

// This object creates and holds proxy content which gets shown instead of the
// actual web content, generated from the passed |web_view|.
// The created |proxy_content_| will be destroyed with the destruction of this
// object.
// Calling EvictContent() will release the rendered content and replaces it with
// a solid color to save memory.
// Calling Reparent() will transfer the |proxy_content_| to an ActivityProxy,
// allowing the destruction of the original activity / content.
class ContentProxy {
 public:
  // Creates the object by creating a sized down |web_view| layer and making it
  // visible inside |activity|'s window.
  ContentProxy(views::WebView* web_view, Activity* activity);
  // TODO(skuhne): Add a function to create this object from a passed PNG, so
  // that we can create it from a session restore.

  // With the destruction of the object, the layer should get destroyed and the
  // content should become visible again.
  virtual ~ContentProxy();

  // Called when the content will get unloaded.
  void ContentWillUnload();

  // Can be called to save resources by creating a layer with a solid color
  // instead of creating a content image layer.
  // Note: Currently the GPU does create a full size texture and fills it with
  // the given color - so there isn't really memory savings yet.
  void EvictContent();

  // Get the image of the content. If there is no image known, an empty image
  // will be returned.
  gfx::ImageSkia GetContentImage();

  // Transfer the owned |proxy_content_| to the |new_parent_window|.
  // Once called, the |web_view_| will be made visible again and the connection
  // to it will be removed since the old activity might go away.
  // Note: This function should only be used by AppActivity.
  void Reparent(aura::Window* new_parent_window);

 private:
  // Make the original (web)content visible. This call should only be paired
  // with HideOriginalContent.
  void ShowOriginalContent();

  // Make the content invisible. This call should only be paired with
  // MakeVisible.
  void HideOriginalContent();

  // Creates proxy content from |web_view_|. If there is no |web_view_|,
  // a solid |proxy_content_| with |background_color_| will be created.
  void CreateProxyContent();

  // Creates a solid |proxy_content_| with |background_color_|.
  void CreateSolidProxyContent();

  // Show the |proxy_content_| in the current |window_|.
  void ShowProxyContent();

  // Removes the |proxy_content_| from the window again.
  void HideProxyContent();

  // The web view which is associated with this object. It will be NULL after
  // the object got called with Reparent(), since the Activity which owns it
  // will be destroyed shortly after.
  views::WebView* web_view_;

  // The window which shows our |proxy_content_|,
  aura::Window* window_;

  // The background color to use when evicted.
  SkColor background_color_;

  // If we have an image (e.g. from session restore) it is stored here.
  gfx::ImageSkia image_;

  // True if the content is loaded.
  bool content_loaded_;

  // The content representation which which will be presented to the user. It
  // will either contain a shrunken down image of the |web_view| content or a
  // solid |background_color_|.
  scoped_ptr<views::View> proxy_content_;

  DISALLOW_COPY_AND_ASSIGN(ContentProxy);
};

}  // namespace athena

#endif  // ATHENA_CONTENT_CONTENT_PROXY_H_
