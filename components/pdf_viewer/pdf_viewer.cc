// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "components/mus/public/cpp/scoped_window_ptr.h"
#include "components/mus/public/cpp/types.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_observer.h"
#include "components/mus/public/cpp/window_surface.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/public/cpp/window_tree_delegate.h"
#include "components/mus/public/interfaces/compositor_frame.mojom.h"
#include "components/mus/public/interfaces/gpu.mojom.h"
#include "components/mus/public/interfaces/surface_id.mojom.h"
#include "components/web_view/public/interfaces/frame.mojom.h"
#include "gpu/GLES2/gl2chromium.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/application_runner.h"
#include "mojo/application/public/cpp/connect.h"
#include "mojo/application/public/cpp/interface_factory_impl.h"
#include "mojo/application/public/cpp/service_provider_impl.h"
#include "mojo/application/public/interfaces/content_handler.mojom.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "mojo/common/data_pipe_utils.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/surfaces/surfaces_type_converters.h"
#include "mojo/converters/surfaces/surfaces_utils.h"
#include "mojo/public/c/gles2/gles2.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/pdfium/public/fpdf_ext.h"
#include "third_party/pdfium/public/fpdfview.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/mojo/events/input_events.mojom.h"
#include "ui/mojo/events/input_key_codes.mojom.h"
#include "ui/mojo/geometry/geometry.mojom.h"
#include "ui/mojo/geometry/geometry_util.h"
#include "v8/include/v8.h"

const uint32_t g_background_color = 0xFF888888;
const uint32_t g_transparent_color = 0x00000000;

namespace pdf_viewer {
namespace {

void LostContext(void*) {
  DCHECK(false);
}

void OnGotContentHandlerID(uint32_t content_handler_id) {}

// BitmapUploader is useful if you want to draw a bitmap or color in a View.
class BitmapUploader : public mus::mojom::SurfaceClient {
 public:
  explicit BitmapUploader(mus::Window* view)
      : view_(view),
        color_(g_transparent_color),
        width_(0),
        height_(0),
        format_(BGRA),
        next_resource_id_(1u),
        id_namespace_(0u),
        local_id_(0u),
        returner_binding_(this) {}
  ~BitmapUploader() override {
    MojoGLES2DestroyContext(gles2_context_);
  }

  void Init(mojo::Shell* shell) {
    surface_ = view_->RequestSurface();
    surface_->BindToThread();

    mojo::ServiceProviderPtr gpu_service_provider;
    mojo::URLRequestPtr request2(mojo::URLRequest::New());
    request2->url = mojo::String::From("mojo:mus");
    shell->ConnectToApplication(request2.Pass(),
                                mojo::GetProxy(&gpu_service_provider), nullptr,
                                nullptr, base::Bind(&OnGotContentHandlerID));
    ConnectToService(gpu_service_provider.get(), &gpu_service_);

    mus::mojom::CommandBufferPtr gles2_client;
    gpu_service_->CreateOffscreenGLES2Context(GetProxy(&gles2_client));
    gles2_context_ = MojoGLES2CreateContext(
        gles2_client.PassInterface().PassHandle().release().value(),
        nullptr,
        &LostContext, NULL, mojo::Environment::GetDefaultAsyncWaiter());
    MojoGLES2MakeCurrent(gles2_context_);
  }

  // Sets the color which is RGBA.
  void SetColor(uint32_t color) {
    if (color_ == color)
      return;
    color_ = color;
    if (surface_)
      Upload();
  }

  enum Format {
    RGBA,  // Pixel layout on Android.
    BGRA,  // Pixel layout everywhere else.
  };

  // Sets a bitmap.
  void SetBitmap(int width,
                 int height,
                 scoped_ptr<std::vector<unsigned char>> data,
                 Format format) {
    width_ = width;
    height_ = height;
    bitmap_ = data.Pass();
    format_ = format;
    if (surface_)
      Upload();
  }

 private:
  void Upload() {
    gfx::Rect bounds(view_->bounds().To<gfx::Rect>());
    mus::mojom::PassPtr pass = mojo::CreateDefaultPass(1, bounds);
    mus::mojom::CompositorFramePtr frame = mus::mojom::CompositorFrame::New();

    // TODO(rjkroege): Support device scale factor in PDF viewer
    mus::mojom::CompositorFrameMetadataPtr meta =
        mus::mojom::CompositorFrameMetadata::New();
    meta->device_scale_factor = 1.0f;
    frame->metadata = meta.Pass();

    frame->resources.resize(0u);

    pass->quads.resize(0u);
    pass->shared_quad_states.push_back(
        mojo::CreateDefaultSQS(bounds.size()));

    MojoGLES2MakeCurrent(gles2_context_);
    if (bitmap_.get()) {
      mojo::Size bitmap_size;
      bitmap_size.width = width_;
      bitmap_size.height = height_;
      GLuint texture_id = BindTextureForSize(bitmap_size);
      glTexSubImage2D(GL_TEXTURE_2D,
                      0,
                      0,
                      0,
                      bitmap_size.width,
                      bitmap_size.height,
                      TextureFormat(),
                      GL_UNSIGNED_BYTE,
                      &((*bitmap_)[0]));

      GLbyte mailbox[GL_MAILBOX_SIZE_CHROMIUM];
      glGenMailboxCHROMIUM(mailbox);
      glProduceTextureCHROMIUM(GL_TEXTURE_2D, mailbox);
      GLuint sync_point = glInsertSyncPointCHROMIUM();

      mus::mojom::TransferableResourcePtr resource =
          mus::mojom::TransferableResource::New();
      resource->id = next_resource_id_++;
      resource_to_texture_id_map_[resource->id] = texture_id;
      resource->format = mus::mojom::RESOURCE_FORMAT_RGBA_8888;
      resource->filter = GL_LINEAR;
      resource->size = bitmap_size.Clone();
      mus::mojom::MailboxHolderPtr mailbox_holder =
          mus::mojom::MailboxHolder::New();
      mailbox_holder->mailbox = mus::mojom::Mailbox::New();
      for (int i = 0; i < GL_MAILBOX_SIZE_CHROMIUM; ++i)
        mailbox_holder->mailbox->name.push_back(mailbox[i]);
      mailbox_holder->texture_target = GL_TEXTURE_2D;
      mailbox_holder->sync_point = sync_point;
      resource->mailbox_holder = mailbox_holder.Pass();
      resource->is_repeated = false;
      resource->is_software = false;

      mus::mojom::QuadPtr quad = mus::mojom::Quad::New();
      quad->material = mus::mojom::MATERIAL_TEXTURE_CONTENT;

      mojo::RectPtr rect = mojo::Rect::New();
      if (width_ <= bounds.width() && height_ <= bounds.height()) {
        rect->width = width_;
        rect->height = height_;
      } else {
        // The source bitmap is larger than the viewport. Resize it while
        // maintaining the aspect ratio.
        float width_ratio = static_cast<float>(width_) / bounds.width();
        float height_ratio = static_cast<float>(height_) / bounds.height();
        if (width_ratio > height_ratio) {
          rect->width = bounds.width();
          rect->height = height_ / width_ratio;
        } else {
          rect->height = bounds.height();
          rect->width = width_ / height_ratio;
        }
      }
      quad->rect = rect.Clone();
      quad->opaque_rect = rect.Clone();
      quad->visible_rect = rect.Clone();
      quad->needs_blending = true;
      quad->shared_quad_state_index = 0u;

      mus::mojom::TextureQuadStatePtr texture_state =
          mus::mojom::TextureQuadState::New();
      texture_state->resource_id = resource->id;
      texture_state->premultiplied_alpha = true;
      texture_state->uv_top_left = mojo::PointF::New();
      texture_state->uv_bottom_right = mojo::PointF::New();
      texture_state->uv_bottom_right->x = 1.f;
      texture_state->uv_bottom_right->y = 1.f;
      texture_state->background_color = mus::mojom::Color::New();
      texture_state->background_color->rgba = g_transparent_color;
      for (int i = 0; i < 4; ++i)
        texture_state->vertex_opacity.push_back(1.f);
      texture_state->y_flipped = false;

      frame->resources.push_back(resource.Pass());
      quad->texture_quad_state = texture_state.Pass();
      pass->quads.push_back(quad.Pass());
    }

    if (color_ != g_transparent_color) {
      mus::mojom::QuadPtr quad = mus::mojom::Quad::New();
      quad->material = mus::mojom::MATERIAL_SOLID_COLOR;
      quad->rect = mojo::Rect::From(bounds);
      quad->opaque_rect = mojo::Rect::New();
      quad->visible_rect = mojo::Rect::From(bounds);
      quad->needs_blending = true;
      quad->shared_quad_state_index = 0u;

      mus::mojom::SolidColorQuadStatePtr color_state =
          mus::mojom::SolidColorQuadState::New();
      color_state->color = mus::mojom::Color::New();
      color_state->color->rgba = color_;
      color_state->force_anti_aliasing_off = false;

      quad->solid_color_quad_state = color_state.Pass();
      pass->quads.push_back(quad.Pass());
    }

    frame->passes.push_back(pass.Pass());

    // TODO(rjkroege, fsamuel): We should throttle frames.
    surface_->SubmitCompositorFrame(frame.Pass(), mojo::Closure());
  }

  uint32_t BindTextureForSize(const mojo::Size size) {
    // TODO(jamesr): Recycle textures.
    GLuint texture = 0u;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 TextureFormat(),
                 size.width,
                 size.height,
                 0,
                 TextureFormat(),
                 GL_UNSIGNED_BYTE,
                 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    return texture;
  }

  uint32_t TextureFormat() {
    return format_ == BGRA ? GL_BGRA_EXT : GL_RGBA;
  }

  void SetIdNamespace(uint32_t id_namespace) {
    id_namespace_ = id_namespace;
    if (color_ != g_transparent_color || bitmap_.get())
      Upload();
  }

  // SurfaceClient implementation.
  void ReturnResources(
      mojo::Array<mus::mojom::ReturnedResourcePtr> resources) override {
    MojoGLES2MakeCurrent(gles2_context_);
    // TODO(jamesr): Recycle.
    for (size_t i = 0; i < resources.size(); ++i) {
      mus::mojom::ReturnedResourcePtr resource = resources[i].Pass();
      DCHECK_EQ(1, resource->count);
      glWaitSyncPointCHROMIUM(resource->sync_point);
      uint32_t texture_id = resource_to_texture_id_map_[resource->id];
      DCHECK_NE(0u, texture_id);
      resource_to_texture_id_map_.erase(resource->id);
      glDeleteTextures(1, &texture_id);
    }
  }

  mus::Window* view_;
  mus::mojom::GpuPtr gpu_service_;
  scoped_ptr<mus::WindowSurface> surface_;
  MojoGLES2Context gles2_context_;

  mojo::Size size_;
  uint32_t color_;
  int width_;
  int height_;
  Format format_;
  scoped_ptr<std::vector<unsigned char>> bitmap_;
  uint32_t next_resource_id_;
  uint32_t id_namespace_;
  uint32_t local_id_;
  base::hash_map<uint32_t, uint32_t> resource_to_texture_id_map_;
  mojo::Binding<mus::mojom::SurfaceClient> returner_binding_;

  DISALLOW_COPY_AND_ASSIGN(BitmapUploader);
};

// Responsible for managing a particlar view displaying a PDF document.
class PDFView : public mus::WindowTreeDelegate,
                public mus::WindowObserver,
                public web_view::mojom::FrameClient,
                public mojo::InterfaceFactory<web_view::mojom::FrameClient> {
 public:
  using DeleteCallback = base::Callback<void(PDFView*)>;

  PDFView(mojo::ApplicationImpl* app,
          mojo::ApplicationConnection* connection,
          FPDF_DOCUMENT doc,
          const DeleteCallback& delete_callback)
      : app_ref_(app->app_lifetime_helper()->CreateAppRefCount()),
        doc_(doc),
        current_page_(0),
        page_count_(FPDF_GetPageCount(doc_)),
        shell_(app->shell()),
        root_(nullptr),
        frame_client_binding_(this),
        delete_callback_(delete_callback) {
    connection->AddService(this);
  }

  void Close() {
    if (root_)
      mus::ScopedWindowPtr::DeleteWindowOrWindowManager(root_);
    else
      delete this;
  }

 private:
  ~PDFView() override {
    DCHECK(!root_);
    if (!delete_callback_.is_null())
      delete_callback_.Run(this);
  }

  void DrawBitmap() {
    if (!doc_)
      return;

    FPDF_PAGE page = FPDF_LoadPage(doc_, current_page_);
    int width = static_cast<int>(FPDF_GetPageWidth(page));
    int height = static_cast<int>(FPDF_GetPageHeight(page));

    scoped_ptr<std::vector<unsigned char>> bitmap;
    bitmap.reset(new std::vector<unsigned char>);
    bitmap->resize(width * height * 4);

    FPDF_BITMAP f_bitmap = FPDFBitmap_CreateEx(width, height, FPDFBitmap_BGRA,
                                               &(*bitmap)[0], width * 4);
    FPDFBitmap_FillRect(f_bitmap, 0, 0, width, height, 0xFFFFFFFF);
    FPDF_RenderPageBitmap(f_bitmap, page, 0, 0, width, height, 0, 0);
    FPDFBitmap_Destroy(f_bitmap);

    FPDF_ClosePage(page);

    bitmap_uploader_->SetBitmap(width, height, bitmap.Pass(),
                                BitmapUploader::BGRA);
  }

  // WindowTreeDelegate:
  void OnEmbed(mus::Window* root) override {
    DCHECK(!root_);
    root_ = root;
    root_->AddObserver(this);
    bitmap_uploader_.reset(new BitmapUploader(root_));
    bitmap_uploader_->Init(shell_);
    bitmap_uploader_->SetColor(g_background_color);
    DrawBitmap();
  }

  void OnConnectionLost(mus::WindowTreeConnection* connection) override {
    root_ = nullptr;
    delete this;
  }

  // WindowObserver:
  void OnWindowBoundsChanged(mus::Window* view,
                             const mojo::Rect& old_bounds,
                             const mojo::Rect& new_bounds) override {
    DrawBitmap();
  }

  void OnWindowInputEvent(mus::Window* view,
                          const mojo::EventPtr& event) override {
    if (event->key_data && (event->action != mojo::EVENT_TYPE_KEY_PRESSED ||
                            event->key_data->is_char)) {
      return;
    }

    // TODO(rjkroege): Make panning and scrolling more performant and
    // responsive to gesture events.
    if ((event->key_data &&
         event->key_data->windows_key_code == mojo::KEYBOARD_CODE_DOWN) ||
        (event->wheel_data && event->wheel_data->delta_y < 0)) {
      if (current_page_ < (page_count_ - 1)) {
        current_page_++;
        DrawBitmap();
      }
    } else if ((event->key_data &&
                event->key_data->windows_key_code == mojo::KEYBOARD_CODE_UP) ||
               (event->wheel_data && event->wheel_data->delta_y > 0)) {
      if (current_page_ > 0) {
        current_page_--;
        DrawBitmap();
      }
    }
  }

  void OnWindowDestroyed(mus::Window* view) override {
    DCHECK_EQ(root_, view);
    root_ = nullptr;
    bitmap_uploader_.reset();
  }

  // web_view::mojom::FrameClient:
  void OnConnect(web_view::mojom::FramePtr frame,
                 uint32_t change_id,
                 uint32_t view_id,
                 web_view::mojom::WindowConnectType view_connect_type,
                 mojo::Array<web_view::mojom::FrameDataPtr> frame_data,
                 int64_t navigation_start_time_ticks,
                 const OnConnectCallback& callback) override {
    callback.Run();

    frame_ = frame.Pass();
    frame_->DidCommitProvisionalLoad();
  }
  void OnFrameAdded(uint32_t change_id,
                    web_view::mojom::FrameDataPtr frame_data) override {}
  void OnFrameRemoved(uint32_t change_id, uint32_t frame_id) override {}
  void OnFrameClientPropertyChanged(uint32_t frame_id,
                                    const mojo::String& name,
                                    mojo::Array<uint8_t> new_value) override {}
  void OnPostMessageEvent(uint32_t source_frame_id,
                          uint32_t target_frame_id,
                          web_view::mojom::HTMLMessageEventPtr event) override {
  }
  void OnWillNavigate(const mojo::String& origin,
                      const OnWillNavigateCallback& callback) override {}
  void OnFrameLoadingStateChanged(uint32_t frame_id, bool loading) override {}
  void OnDispatchFrameLoadEvent(uint32_t frame_id) override {}
  void Find(int32_t request_id,
            const mojo::String& search_text,
            web_view::mojom::FindOptionsPtr options,
            bool wrap_within_frame,
            const FindCallback& callback) override {
    NOTIMPLEMENTED();
    bool found_results = false;
    callback.Run(found_results);
  }
  void StopFinding(bool clear_selection) override {}
  void HighlightFindResults(int32_t request_id,
                            const mojo::String& search_test,
                            web_view::mojom::FindOptionsPtr options,
                            bool reset) override {
    NOTIMPLEMENTED();
  }
  void StopHighlightingFindResults() override {}

  // mojo::InterfaceFactory<web_view::mojom::FrameClient>:
  void Create(
      mojo::ApplicationConnection* connection,
      mojo::InterfaceRequest<web_view::mojom::FrameClient> request) override {
    frame_client_binding_.Bind(request.Pass());
  }

  scoped_ptr<mojo::AppRefCount> app_ref_;
  FPDF_DOCUMENT doc_;
  int current_page_;
  int page_count_;

  scoped_ptr<BitmapUploader> bitmap_uploader_;

  mojo::Shell* shell_;
  mus::Window* root_;

  web_view::mojom::FramePtr frame_;
  mojo::Binding<web_view::mojom::FrameClient> frame_client_binding_;
  DeleteCallback delete_callback_;

  DISALLOW_COPY_AND_ASSIGN(PDFView);
};

// Responsible for managing all the views for displaying a PDF document.
class PDFViewerApplicationDelegate
    : public mojo::ApplicationDelegate,
      public mojo::InterfaceFactory<mus::mojom::WindowTreeClient> {
 public:
  PDFViewerApplicationDelegate(
      mojo::InterfaceRequest<mojo::Application> request,
      mojo::URLResponsePtr response)
      : app_(this,
             request.Pass(),
             base::Bind(&PDFViewerApplicationDelegate::OnTerminate,
                        base::Unretained(this))),
        doc_(nullptr),
        is_destroying_(false) {
    FetchPDF(response.Pass());
  }

  ~PDFViewerApplicationDelegate() override {
    is_destroying_ = true;
    if (doc_)
      FPDF_CloseDocument(doc_);
    while (!pdf_views_.empty())
      pdf_views_.front()->Close();
  }

 private:
  void FetchPDF(mojo::URLResponsePtr response) {
    data_.clear();
    mojo::common::BlockingCopyToString(response->body.Pass(), &data_);
    if (data_.length() >= static_cast<size_t>(std::numeric_limits<int>::max()))
      return;
    doc_ = FPDF_LoadMemDocument(data_.data(), static_cast<int>(data_.length()),
                                nullptr);
  }

  // Callback from the quit closure. We key off this rather than
  // ApplicationDelegate::Quit() as we don't want to shut down the messageloop
  // when we quit (the messageloop is shared among multiple PDFViews).
  void OnTerminate() { delete this; }

  void OnPDFViewDestroyed(PDFView* pdf_view) {
    DCHECK(std::find(pdf_views_.begin(), pdf_views_.end(), pdf_view) !=
           pdf_views_.end());
    pdf_views_.erase(std::find(pdf_views_.begin(), pdf_views_.end(), pdf_view));
  }

  // ApplicationDelegate:
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override {
    connection->AddService<mus::mojom::WindowTreeClient>(this);
    return true;
  }

  // mojo::InterfaceFactory<mus::mojom::WindowTreeClient>:
  void Create(
      mojo::ApplicationConnection* connection,
      mojo::InterfaceRequest<mus::mojom::WindowTreeClient> request) override {
    PDFView* pdf_view = new PDFView(
        &app_, connection, doc_,
        base::Bind(&PDFViewerApplicationDelegate::OnPDFViewDestroyed,
                   base::Unretained(this)));
    pdf_views_.push_back(pdf_view);
    mus::WindowTreeConnection::Create(
        pdf_view, request.Pass(),
        mus::WindowTreeConnection::CreateType::DONT_WAIT_FOR_EMBED);
  }

  mojo::ApplicationImpl app_;
  std::string data_;
  std::vector<PDFView*> pdf_views_;
  FPDF_DOCUMENT doc_;
  bool is_destroying_;

  DISALLOW_COPY_AND_ASSIGN(PDFViewerApplicationDelegate);
};

class ContentHandlerImpl : public mojo::ContentHandler {
 public:
  ContentHandlerImpl(mojo::InterfaceRequest<ContentHandler> request)
      : binding_(this, request.Pass()) {}
  ~ContentHandlerImpl() override {}

 private:
  // ContentHandler:
  void StartApplication(mojo::InterfaceRequest<mojo::Application> request,
                        mojo::URLResponsePtr response) override {
    new PDFViewerApplicationDelegate(request.Pass(), response.Pass());
  }

  mojo::StrongBinding<mojo::ContentHandler> binding_;

  DISALLOW_COPY_AND_ASSIGN(ContentHandlerImpl);
};

class PDFViewer : public mojo::ApplicationDelegate,
                  public mojo::InterfaceFactory<mojo::ContentHandler> {
 public:
  PDFViewer() {
    v8::V8::InitializeICU();
    FPDF_InitLibrary();
  }

  ~PDFViewer() override { FPDF_DestroyLibrary(); }

 private:
  // ApplicationDelegate:
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override {
    connection->AddService(this);
    return true;
  }

  // InterfaceFactory<ContentHandler>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::ContentHandler> request) override {
    new ContentHandlerImpl(request.Pass());
  }

  DISALLOW_COPY_AND_ASSIGN(PDFViewer);
};
}  // namespace
}  // namespace pdf_viewer

MojoResult MojoMain(MojoHandle application_request) {
  mojo::ApplicationRunner runner(new pdf_viewer::PDFViewer());
  return runner.Run(application_request);
}
