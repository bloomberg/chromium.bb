// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/message_loop/message_loop.h"
#include "base/strings/string_tokenizer.h"
#include "mojo/examples/media_viewer/media_viewer.mojom.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/interface_factory_impl.h"
#include "mojo/services/public/cpp/view_manager/types.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_client_factory.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "mojo/services/public/interfaces/navigation/navigation.mojom.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/gfx/codec/png_codec.h"

namespace mojo {
namespace examples {

class PNGViewer;

class ZoomableMediaImpl : public InterfaceImpl<ZoomableMedia> {
 public:
  explicit ZoomableMediaImpl(PNGViewer* viewer) : viewer_(viewer) {}
  virtual ~ZoomableMediaImpl() {}

 private:
  // Overridden from ZoomableMedia:
  virtual void ZoomIn() OVERRIDE;
  virtual void ZoomOut() OVERRIDE;
  virtual void ZoomToActualSize() OVERRIDE;

  PNGViewer* viewer_;

  DISALLOW_COPY_AND_ASSIGN(ZoomableMediaImpl);
};

class NavigatorImpl : public InterfaceImpl<Navigator> {
 public:
  explicit NavigatorImpl(PNGViewer* viewer) : viewer_(viewer) {}
  virtual ~NavigatorImpl() {}

 private:
  // Overridden from Navigator:
  virtual void Navigate(
      uint32_t view_id,
      NavigationDetailsPtr navigation_details,
      ResponseDetailsPtr response_details) OVERRIDE {
    int content_length = GetContentLength(response_details->response->headers);
    unsigned char* data = new unsigned char[content_length];
    unsigned char* buf = data;
    uint32_t bytes_remaining = content_length;
    uint32_t num_bytes = bytes_remaining;
    while (bytes_remaining > 0) {
      MojoResult result = ReadDataRaw(
          response_details->response->body.get(),
          buf,
          &num_bytes,
          MOJO_READ_DATA_FLAG_NONE);
      if (result == MOJO_RESULT_SHOULD_WAIT) {
        Wait(response_details->response->body.get(),
             MOJO_HANDLE_SIGNAL_READABLE,
             MOJO_DEADLINE_INDEFINITE);
      } else if (result == MOJO_RESULT_OK) {
        buf += num_bytes;
        num_bytes = bytes_remaining -= num_bytes;
      } else {
        break;
      }
    }

    SkBitmap bitmap;
    gfx::PNGCodec::Decode(static_cast<const unsigned char*>(data),
                          content_length, &bitmap);
    UpdateView(view_id, bitmap);

    delete[] data;
  }

  void UpdateView(Id view_id, const SkBitmap& bitmap);

  int GetContentLength(const Array<String>& headers) {
    for (size_t i = 0; i < headers.size(); ++i) {
      base::StringTokenizer t(headers[i], ": ;=");
      while (t.GetNext()) {
        if (!t.token_is_delim() && t.token() == "Content-Length") {
          while (t.GetNext()) {
            if (!t.token_is_delim())
              return atoi(t.token().c_str());
          }
        }
      }
    }
    return 0;
  }

  PNGViewer* viewer_;

  DISALLOW_COPY_AND_ASSIGN(NavigatorImpl);
};

class PNGViewer
    : public ApplicationDelegate,
      public ViewManagerDelegate,
      public ViewObserver {
 public:
  PNGViewer()
      : navigator_factory_(this),
        zoomable_media_factory_(this),
        view_manager_client_factory_(this),
        root_(NULL),
        zoom_percentage_(kDefaultZoomPercentage) {}
  virtual ~PNGViewer() {
    if (root_)
      root_->RemoveObserver(this);
  }

  void UpdateView(Id view_id, const SkBitmap& bitmap) {
    bitmap_ = bitmap;
    zoom_percentage_ = kDefaultZoomPercentage;
    DrawBitmap();
  }

  void ZoomIn() {
    if (zoom_percentage_ >= kMaxZoomPercentage)
      return;
    zoom_percentage_ += kZoomStep;
    DrawBitmap();
  }

  void ZoomOut() {
    if (zoom_percentage_ <= kMinZoomPercentage)
      return;
    zoom_percentage_ -= kZoomStep;
    DrawBitmap();
  }

  void ZoomToActualSize() {
    if (zoom_percentage_ == kDefaultZoomPercentage)
      return;
    zoom_percentage_ = kDefaultZoomPercentage;
    DrawBitmap();
  }

 private:
  static const uint16_t kMaxZoomPercentage = 400;
  static const uint16_t kMinZoomPercentage = 20;
  static const uint16_t kDefaultZoomPercentage = 100;
  static const uint16_t kZoomStep = 20;

  // Overridden from ApplicationDelegate:
  virtual bool ConfigureIncomingConnection(ApplicationConnection* connection)
      MOJO_OVERRIDE {
    connection->AddService(&navigator_factory_);
    connection->AddService(&zoomable_media_factory_);
    connection->AddService(&view_manager_client_factory_);
    return true;
  }

  // Overridden from ViewManagerDelegate:
  virtual void OnEmbed(ViewManager* view_manager,
                       View* root,
                       ServiceProviderImpl* exported_services,
                       scoped_ptr<ServiceProvider> imported_services) OVERRIDE {
    root_ = root;
    root_->AddObserver(this);
    root_->SetColor(SK_ColorGRAY);
    if (!bitmap_.isNull())
      DrawBitmap();
  }
  virtual void OnViewManagerDisconnected(
      ViewManager* view_manager) OVERRIDE {
    base::MessageLoop::current()->Quit();
  }

  void DrawBitmap() {
    if (!root_)
      return;

    skia::RefPtr<SkCanvas> canvas(skia::AdoptRef(skia::CreatePlatformCanvas(
        root_->bounds().width(),
        root_->bounds().height(),
        true)));
    canvas->drawColor(SK_ColorGRAY);
    SkPaint paint;
    SkScalar scale =
        SkFloatToScalar(zoom_percentage_ * 1.0f / kDefaultZoomPercentage);
    canvas->scale(scale, scale);
    canvas->drawBitmap(bitmap_, 0, 0, &paint);
    root_->SetContents(skia::GetTopDevice(*canvas)->accessBitmap(true));
  }

  // ViewObserver:
  virtual void OnViewBoundsChanged(View* view,
                                   const gfx::Rect& old_bounds,
                                   const gfx::Rect& new_bounds) OVERRIDE {
    DCHECK_EQ(view, root_);
    DrawBitmap();
  }
  virtual void OnViewDestroyed(View* view) OVERRIDE {
    DCHECK_EQ(view, root_);
    view->RemoveObserver(this);
    root_ = NULL;
  }

  InterfaceFactoryImplWithContext<NavigatorImpl, PNGViewer> navigator_factory_;
  InterfaceFactoryImplWithContext<ZoomableMediaImpl, PNGViewer>
      zoomable_media_factory_;
  ViewManagerClientFactory view_manager_client_factory_;

  View* root_;
  SkBitmap bitmap_;
  uint16_t zoom_percentage_;

  DISALLOW_COPY_AND_ASSIGN(PNGViewer);
};

void ZoomableMediaImpl::ZoomIn() {
  viewer_->ZoomIn();
}

void ZoomableMediaImpl::ZoomOut() {
  viewer_->ZoomOut();
}

void ZoomableMediaImpl::ZoomToActualSize() {
  viewer_->ZoomToActualSize();
}

void NavigatorImpl::UpdateView(Id view_id,
                               const SkBitmap& bitmap) {
  viewer_->UpdateView(view_id, bitmap);
}

}  // namespace examples

// static
ApplicationDelegate* ApplicationDelegate::Create() {
  return new examples::PNGViewer;
}

}  // namespace mojo
