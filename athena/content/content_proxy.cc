// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/content_proxy.h"

#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_view_model.h"
#include "base/bind.h"
#include "base/threading/worker_pool.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_png_rep.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

namespace athena {

// Encodes an A8 SkBitmap to grayscale PNG in a worker thread.
class ProxyImageData : public base::RefCountedThreadSafe<ProxyImageData> {
 public:
  ProxyImageData() {
  }

  void EncodeImage(const SkBitmap& bitmap, base::Closure callback) {
    if (!base::WorkerPool::PostTaskAndReply(FROM_HERE,
            base::Bind(&ProxyImageData::EncodeOnWorker,
                       this,
                       bitmap),
            callback,
            true)) {
      // When coming here, the resulting image will be empty.
      DCHECK(false) << "Cannot start bitmap encode task.";
      callback.Run();
    }
  }

  scoped_refptr<base::RefCountedBytes> data() const { return data_; }

 private:
  friend class base::RefCountedThreadSafe<ProxyImageData>;
  virtual ~ProxyImageData() {
  }

  void EncodeOnWorker(const SkBitmap& bitmap) {
    DCHECK_EQ(bitmap.colorType(), kAlpha_8_SkColorType);
    // Encode the A8 bitmap to grayscale PNG treating alpha as color intensity.
    std::vector<unsigned char> data;
    if (gfx::PNGCodec::EncodeA8SkBitmap(bitmap, &data))
      data_ = new base::RefCountedBytes(data);
  }

  scoped_refptr<base::RefCountedBytes> data_;

  DISALLOW_COPY_AND_ASSIGN(ProxyImageData);
};

ContentProxy::ContentProxy(views::WebView* web_view, Activity* activity)
    : web_view_(web_view),
      content_visible_(true),
      content_loaded_(true),
      content_creation_called_(false),
      proxy_content_to_image_factory_(this) {
  // Note: The content will be hidden once the image got created.
  CreateProxyContent();
}

ContentProxy::~ContentProxy() {
  // If we still have a connection to the original Activity, we make it visible
  // again.
  ShowOriginalContent();
}

void ContentProxy::ContentWillUnload() {
  content_loaded_ = false;
}

gfx::ImageSkia ContentProxy::GetContentImage() {
  // While we compress to PNG, we use the original read back.
  if (!raw_image_.isNull() || !png_data_.get())
    return raw_image_;

  // Otherwise we convert the PNG.
  std::vector<gfx::ImagePNGRep> image_reps;
  image_reps.push_back(gfx::ImagePNGRep(png_data_, 0.0f));
  return *(gfx::Image(image_reps).ToImageSkia());
}

void ContentProxy::EvictContent() {
  raw_image_ = gfx::ImageSkia();
  png_data_->Release();
}

void ContentProxy::OnPreContentDestroyed() {
  // Since we are breaking now the connection to the old content, we make the
  // content visible again before we continue.
  // Note: Since the owning window is invisible, it does not matter that we
  // make the web content visible if the window gets destroyed shortly after.
  ShowOriginalContent();

  web_view_ = NULL;
}

void ContentProxy::ShowOriginalContent() {
  if (web_view_ && !content_visible_) {
    // Show the original |web_view_| again.
    web_view_->SetFastResize(false);
    // If the content is loaded, we ask it to relayout itself since the
    // dimensions might have changed. If not, we will reload new content and no
    // layout is required for the old content.
    if (content_loaded_)
      web_view_->Layout();
    web_view_->GetWebContents()->GetNativeView()->Show();
    web_view_->SetVisible(true);
    content_visible_ = true;
  }
}

void ContentProxy::HideOriginalContent() {
  if (web_view_ && content_visible_) {
    // Hide the |web_view_|.
    // TODO(skuhne): We might consider removing the view from the window while
    // it's hidden - it should work the same way as show/hide and does not have
    // any window re-ordering effect. Furthermore we want possibly to suppress
    // any resizing of content (not only fast resize) here to avoid jank on
    // rotation.
    web_view_->GetWebContents()->GetNativeView()->Hide();
    web_view_->SetVisible(false);
    // Don't allow the content to get resized with window size changes.
    web_view_->SetFastResize(true);
    content_visible_ = false;
  }
}

void ContentProxy::CreateProxyContent() {
  DCHECK(!content_creation_called_);
  content_creation_called_ = true;
  // Unit tests might not have a |web_view_|.
  if (!web_view_)
    return;

  content::RenderViewHost* host =
      web_view_->GetWebContents()->GetRenderViewHost();
  DCHECK(host && host->GetView());
  gfx::Size source = host->GetView()->GetViewBounds().size();
  gfx::Size target = gfx::Size(source.width() / 2, source.height() / 2);
  host->CopyFromBackingStore(
      gfx::Rect(),
      target,
      base::Bind(&ContentProxy::OnContentImageRead,
                 proxy_content_to_image_factory_.GetWeakPtr()),
      kAlpha_8_SkColorType);
}

void ContentProxy::OnContentImageRead(bool success, const SkBitmap& bitmap) {
  // Now we can hide the content. Note that after hiding we are freeing memory
  // and if something goes wrong we will end up with an empty page.
  HideOriginalContent();

  if (!success || bitmap.empty() || bitmap.isNull())
    return;

  // While we are encoding the image, we keep the current image as reference
  // to have something for the overview mode to grab. Once we have the encoded
  // PNG, we will get rid of this.
  raw_image_ = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);

  scoped_refptr<ProxyImageData> png_image = new ProxyImageData();
  png_image->EncodeImage(
      bitmap,
      base::Bind(&ContentProxy::OnContentImageEncodeComplete,
                 proxy_content_to_image_factory_.GetWeakPtr(),
                 png_image));
}

void ContentProxy::OnContentImageEncodeComplete(
    scoped_refptr<ProxyImageData> image) {
  png_data_ = image->data();

  // From now on we decode the image as needed to save memory.
  raw_image_ = gfx::ImageSkia();
}

}  // namespace athena
