// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_tokenizer.h"
#include "mojo/application/application_runner_chromium.h"
#include "mojo/examples/media_viewer/media_viewer.mojom.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/interface_factory_impl.h"
#include "mojo/public/cpp/application/service_provider_impl.h"
#include "mojo/services/public/cpp/view_manager/types.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_client_factory.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "mojo/services/public/interfaces/content_handler/content_handler.mojom.h"
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

// TODO(aa): Hook up ZoomableMedia interface again.
class PNGView : public ViewManagerDelegate, public ViewObserver {
 public:
  static void Spawn(URLResponsePtr response,
                    ServiceProviderImpl* exported_services,
                    scoped_ptr<ServiceProvider> imported_services,
                    Shell* shell) {
    // PNGView deletes itself when its View is destroyed.
    new PNGView(
        response.Pass(), exported_services, imported_services.Pass(), shell);
  }

 private:
  static const uint16_t kMaxZoomPercentage = 400;
  static const uint16_t kMinZoomPercentage = 20;
  static const uint16_t kDefaultZoomPercentage = 100;
  static const uint16_t kZoomStep = 20;

  PNGView(URLResponsePtr response,
          ServiceProviderImpl* exported_services,
          scoped_ptr<ServiceProvider> imported_services,
          Shell* shell)
      : imported_services_(imported_services.Pass()),
        root_(NULL),
        view_manager_client_factory_(shell, this),
        zoom_percentage_(kDefaultZoomPercentage) {
    exported_services->AddService(&view_manager_client_factory_);
    DecodePNG(response.Pass());
  }

  virtual ~PNGView() {
    if (root_)
      root_->RemoveObserver(this);
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

  virtual void OnViewManagerDisconnected(ViewManager* view_manager) OVERRIDE {
    // TODO(aa): Need to figure out how shutdown works.
  }

  // Overridden from ViewObserver:
  virtual void OnViewBoundsChanged(View* view,
                                   const gfx::Rect& old_bounds,
                                   const gfx::Rect& new_bounds) OVERRIDE {
    DCHECK_EQ(view, root_);
    DrawBitmap();
  }

  virtual void OnViewDestroyed(View* view) OVERRIDE {
    DCHECK_EQ(view, root_);
    delete this;
  }

  void DecodePNG(URLResponsePtr response) {
    int content_length = GetContentLength(response->headers);
    scoped_ptr<unsigned char[]> data(new unsigned char[content_length]);
    unsigned char* buf = data.get();
    uint32_t bytes_remaining = content_length;
    uint32_t num_bytes = bytes_remaining;
    while (bytes_remaining > 0) {
      MojoResult result = ReadDataRaw(
          response->body.get(), buf, &num_bytes, MOJO_READ_DATA_FLAG_NONE);
      if (result == MOJO_RESULT_SHOULD_WAIT) {
        Wait(response->body.get(),
             MOJO_HANDLE_SIGNAL_READABLE,
             MOJO_DEADLINE_INDEFINITE);
      } else if (result == MOJO_RESULT_OK) {
        buf += num_bytes;
        num_bytes = bytes_remaining -= num_bytes;
      } else {
        break;
      }
    }

    gfx::PNGCodec::Decode(static_cast<const unsigned char*>(data.get()),
                          content_length,
                          &bitmap_);
  }

  void DrawBitmap() {
    if (!root_)
      return;

    skia::RefPtr<SkCanvas> canvas(skia::AdoptRef(skia::CreatePlatformCanvas(
        root_->bounds().width(), root_->bounds().height(), true)));
    canvas->drawColor(SK_ColorGRAY);
    SkPaint paint;
    SkScalar scale =
        SkFloatToScalar(zoom_percentage_ * 1.0f / kDefaultZoomPercentage);
    canvas->scale(scale, scale);
    canvas->drawBitmap(bitmap_, 0, 0, &paint);
    root_->SetContents(skia::GetTopDevice(*canvas)->accessBitmap(true));
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

  SkBitmap bitmap_;
  scoped_ptr<ServiceProvider> imported_services_;
  View* root_;
  ViewManagerClientFactory view_manager_client_factory_;
  uint16_t zoom_percentage_;

  DISALLOW_COPY_AND_ASSIGN(PNGView);
};

class ContentHandlerImpl : public InterfaceImpl<ContentHandler> {
 public:
  explicit ContentHandlerImpl(Shell* shell) : shell_(shell) {}
  virtual ~ContentHandlerImpl() {}

 private:
  // Overridden from ContentHandler:
  virtual void OnConnect(
      const mojo::String& url,
      URLResponsePtr response,
      InterfaceRequest<ServiceProvider> service_provider) OVERRIDE {
    ServiceProviderImpl* exported_services = new ServiceProviderImpl();
    BindToRequest(exported_services, &service_provider);
    scoped_ptr<ServiceProvider> remote(
        exported_services->CreateRemoteServiceProvider());
    PNGView::Spawn(response.Pass(), exported_services, remote.Pass(), shell_);
  }

  Shell* shell_;

  DISALLOW_COPY_AND_ASSIGN(ContentHandlerImpl);
};

class PNGViewer : public ApplicationDelegate {
 public:
  PNGViewer() {}
 private:
  // Overridden from ApplicationDelegate:
  virtual void Initialize(ApplicationImpl* app) MOJO_OVERRIDE {
    content_handler_factory_.reset(
        new InterfaceFactoryImplWithContext<ContentHandlerImpl, Shell>(
            app->shell()));
  }

  // Overridden from ApplicationDelegate:
  virtual bool ConfigureIncomingConnection(ApplicationConnection* connection)
      MOJO_OVERRIDE {
    connection->AddService(content_handler_factory_.get());
    return true;
  }

  scoped_ptr<InterfaceFactoryImplWithContext<ContentHandlerImpl, Shell> >
      content_handler_factory_;

  DISALLOW_COPY_AND_ASSIGN(PNGViewer);
};

}  // namespace examples
}  // namespace mojo

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new mojo::examples::PNGViewer);
  return runner.Run(shell_handle);
}
