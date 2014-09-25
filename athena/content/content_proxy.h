// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_CONTENT_CONTENT_PROXY_H_
#define ATHENA_CONTENT_CONTENT_PROXY_H_

#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/image/image_skia.h"

namespace views {
class WebView;
}

namespace athena {

class Activity;
class ProxyImageData;

// This object creates and holds proxy content which gets shown instead of the
// actual web content, generated from the passed |web_view|.
// The created |proxy_content_| will be destroyed with the destruction of this
// object.
// Calling EvictContent() will release the rendered content.
// When ContentGetsDestroyed() gets called, the old view will be made visible
// and then the link to the |web_view_| will get severed.
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

  // The content is about to get destroyed by its creator.
  // Note: This function should only be used by AppActivity.
  void OnPreContentDestroyed();

 private:
  // Make the original (web)content visible. This call should only be paired
  // with HideOriginalContent.
  void ShowOriginalContent();

  // Make the content invisible. This call should only be paired with
  // MakeVisible.
  void HideOriginalContent();

  // Creates proxy content from |web_view_|.
  void CreateProxyContent();

  // Creates an image from the current content.
  bool CreateContentImage();

  // Called once the content was read back.
  void OnContentImageRead(bool success, const SkBitmap& bitmap);

  // Called once the image content has been converted to PNG.
  void OnContentImageEncodeComplete(scoped_refptr<ProxyImageData> image);

  // The web view which was passed on creation and is associated with this
  // object. It will be shown when OnPreContentDestroyed() gets called and then
  // set to NULL. The ownership remains with the creator.
  views::WebView* web_view_;

  // While we are doing our PNG encode, we keep the read back image to have
  // something which we can pass back to the overview mode. (It would make no
  // sense to the user to see that more recent windows get painted later than
  // older ones).
  gfx::ImageSkia raw_image_;

  // True if the content is visible.
  bool content_visible_;

  // True if the content is loaded and needs a re-layout when it gets shown
  // again.
  bool content_loaded_;

  // True if a content creation was kicked off once. This ensures that the
  // function is never called twice.
  bool content_creation_called_;

  // The PNG image data.
  scoped_refptr<base::RefCountedBytes> png_data_;

  // Creating an encoded image from the content will be asynchronous. Use a
  // weakptr for the callback to make sure that the read back / encoding image
  // completion callback does not trigger on a destroyed ContentProxy.
  base::WeakPtrFactory<ContentProxy> proxy_content_to_image_factory_;

  DISALLOW_COPY_AND_ASSIGN(ContentProxy);
};

}  // namespace athena

#endif  // ATHENA_CONTENT_CONTENT_PROXY_H_
