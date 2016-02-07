// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/bitmap_uploader/bitmap_uploader.h"
#include "components/mus/common/types.h"
#include "components/mus/public/cpp/input_event_handler.h"
#include "components/mus/public/cpp/scoped_window_ptr.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_observer.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/public/cpp/window_tree_delegate.h"
#include "components/mus/public/interfaces/input_events.mojom.h"
#include "components/mus/public/interfaces/input_key_codes.mojom.h"
#include "components/web_view/public/interfaces/frame.mojom.h"
#include "mojo/common/data_pipe_utils.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "mojo/shell/public/cpp/application_runner.h"
#include "mojo/shell/public/cpp/interface_factory_impl.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/public/interfaces/content_handler.mojom.h"
#include "mojo/shell/public/interfaces/shell.mojom.h"
#include "third_party/pdfium/public/fpdf_ext.h"
#include "third_party/pdfium/public/fpdfview.h"
#include "ui/gfx/geometry/rect.h"
#include "v8/include/v8.h"

const uint32_t g_background_color = 0xFF888888;

namespace pdf_viewer {
namespace {

// Responsible for managing a particlar view displaying a PDF document.
class PDFView : public mus::WindowTreeDelegate,
                public mus::WindowObserver,
                public mus::InputEventHandler,
                public web_view::mojom::FrameClient,
                public mojo::InterfaceFactory<web_view::mojom::FrameClient> {
 public:
  using DeleteCallback = base::Callback<void(PDFView*)>;

  PDFView(mojo::Shell* shell,
          mojo::Connection* connection,
          FPDF_DOCUMENT doc,
          const DeleteCallback& delete_callback)
      : app_ref_(shell->CreateAppRefCount()),
        doc_(doc),
        current_page_(0),
        page_count_(FPDF_GetPageCount(doc_)),
        shell_(shell),
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

    bitmap_uploader_->SetBitmap(width, height, std::move(bitmap),
                                bitmap_uploader::BitmapUploader::BGRA);
  }

  // WindowTreeDelegate:
  void OnEmbed(mus::Window* root) override {
    DCHECK(!root_);
    root_ = root;
    root_->AddObserver(this);
    root_->set_input_event_handler(this);
    bitmap_uploader_.reset(new bitmap_uploader::BitmapUploader(root_));
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
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override {
    DrawBitmap();
  }

  void OnWindowDestroyed(mus::Window* view) override {
    DCHECK_EQ(root_, view);
    root_ = nullptr;
    bitmap_uploader_.reset();
  }

  // mus::InputEventHandler:
  void OnWindowInputEvent(mus::Window* view,
                          mus::mojom::EventPtr event,
                          scoped_ptr<base::Closure>* ack_callback) override {
    if (event->key_data &&
        (event->action != mus::mojom::EventType::KEY_PRESSED ||
         event->key_data->is_char)) {
      return;
    }

    // TODO(rjkroege): Make panning and scrolling more performant and
    // responsive to gesture events.
    if ((event->key_data &&
         event->key_data->windows_key_code == mus::mojom::KeyboardCode::DOWN) ||
        (event->pointer_data && event->pointer_data->wheel_data &&
         event->pointer_data->wheel_data->delta_y < 0)) {
      if (current_page_ < (page_count_ - 1)) {
        current_page_++;
        DrawBitmap();
      }
    } else if ((event->key_data &&
                event->key_data->windows_key_code ==
                    mus::mojom::KeyboardCode::UP) ||
               (event->pointer_data && event->pointer_data->wheel_data &&
                event->pointer_data->wheel_data->delta_y > 0)) {
      if (current_page_ > 0) {
        current_page_--;
        DrawBitmap();
      }
    }
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

    frame_ = std::move(frame);
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
      mojo::Connection* connection,
      mojo::InterfaceRequest<web_view::mojom::FrameClient> request) override {
    frame_client_binding_.Bind(std::move(request));
  }

  scoped_ptr<mojo::AppRefCount> app_ref_;
  FPDF_DOCUMENT doc_;
  int current_page_;
  int page_count_;

  scoped_ptr<bitmap_uploader::BitmapUploader> bitmap_uploader_;

  mojo::Shell* shell_;
  mus::Window* root_;

  web_view::mojom::FramePtr frame_;
  mojo::Binding<web_view::mojom::FrameClient> frame_client_binding_;
  DeleteCallback delete_callback_;

  DISALLOW_COPY_AND_ASSIGN(PDFView);
};

// Responsible for managing all the views for displaying a PDF document.
class PDFViewerApplicationDelegate
    : public mojo::ShellClient,
      public mojo::InterfaceFactory<mus::mojom::WindowTreeClient> {
 public:
  PDFViewerApplicationDelegate(
      mojo::ApplicationRequest request,
      mojo::URLResponsePtr response,
      const mojo::Callback<void()>& destruct_callback)
      : app_(this,
             std::move(request),
             base::Bind(&PDFViewerApplicationDelegate::OnTerminate,
                        base::Unretained(this))),
        doc_(nullptr),
        is_destroying_(false),
        destruct_callback_(destruct_callback) {
    FetchPDF(std::move(response));
  }

  ~PDFViewerApplicationDelegate() override {
    is_destroying_ = true;
    if (doc_)
      FPDF_CloseDocument(doc_);
    while (!pdf_views_.empty())
      pdf_views_.front()->Close();
    destruct_callback_.Run();
  }

 private:
  void FetchPDF(mojo::URLResponsePtr response) {
    data_.clear();
    mojo::common::BlockingCopyToString(std::move(response->body), &data_);
    if (data_.length() >= static_cast<size_t>(std::numeric_limits<int>::max()))
      return;
    doc_ = FPDF_LoadMemDocument(data_.data(), static_cast<int>(data_.length()),
                                nullptr);
  }

  // Callback from the quit closure. We key off this rather than
  // ShellClient::Quit() as we don't want to shut down the messageloop
  // when we quit (the messageloop is shared among multiple PDFViews).
  void OnTerminate() { delete this; }

  void OnPDFViewDestroyed(PDFView* pdf_view) {
    DCHECK(std::find(pdf_views_.begin(), pdf_views_.end(), pdf_view) !=
           pdf_views_.end());
    pdf_views_.erase(std::find(pdf_views_.begin(), pdf_views_.end(), pdf_view));
  }

  // mojo::ShellClient:
  bool AcceptConnection(mojo::Connection* connection) override {
    connection->AddService<mus::mojom::WindowTreeClient>(this);
    return true;
  }

  // mojo::InterfaceFactory<mus::mojom::WindowTreeClient>:
  void Create(
      mojo::Connection* connection,
      mojo::InterfaceRequest<mus::mojom::WindowTreeClient> request) override {
    PDFView* pdf_view = new PDFView(
        &app_, connection, doc_,
        base::Bind(&PDFViewerApplicationDelegate::OnPDFViewDestroyed,
                   base::Unretained(this)));
    pdf_views_.push_back(pdf_view);
    mus::WindowTreeConnection::Create(
        pdf_view, std::move(request),
        mus::WindowTreeConnection::CreateType::DONT_WAIT_FOR_EMBED);
  }

  mojo::ApplicationImpl app_;
  std::string data_;
  std::vector<PDFView*> pdf_views_;
  FPDF_DOCUMENT doc_;
  bool is_destroying_;
  mojo::Callback<void()> destruct_callback_;

  DISALLOW_COPY_AND_ASSIGN(PDFViewerApplicationDelegate);
};

class ContentHandlerImpl : public mojo::shell::mojom::ContentHandler {
 public:
  ContentHandlerImpl(
      mojo::InterfaceRequest<mojo::shell::mojom::ContentHandler> request)
      : binding_(this, std::move(request)) {}
  ~ContentHandlerImpl() override {}

 private:
  // mojo::shell::mojom::ContentHandler:
  void StartApplication(
      mojo::ApplicationRequest request,
      mojo::URLResponsePtr response,
      const mojo::Callback<void()>& destruct_callback) override {
    new PDFViewerApplicationDelegate(std::move(request), std::move(response),
                                     destruct_callback);
  }

  mojo::StrongBinding<mojo::shell::mojom::ContentHandler> binding_;

  DISALLOW_COPY_AND_ASSIGN(ContentHandlerImpl);
};

class PDFViewer
    : public mojo::ShellClient,
      public mojo::InterfaceFactory<mojo::shell::mojom::ContentHandler> {
 public:
  PDFViewer() {
    v8::V8::InitializeICU();
    FPDF_InitLibrary();
  }

  ~PDFViewer() override { FPDF_DestroyLibrary(); }

 private:
  // mojo::ShellClient:
  void Initialize(mojo::Shell* shell, const std::string& url,
                  uint32_t id) override {
    tracing_.Initialize(shell, url);
  }

  bool AcceptConnection(mojo::Connection* connection) override {
    connection->AddService(this);
    return true;
  }

  // InterfaceFactory<ContentHandler>:
  void Create(mojo::Connection* connection,
              mojo::InterfaceRequest<mojo::shell::mojom::ContentHandler>
                  request) override {
    new ContentHandlerImpl(std::move(request));
  }

  mojo::TracingImpl tracing_;

  DISALLOW_COPY_AND_ASSIGN(PDFViewer);
};
}  // namespace
}  // namespace pdf_viewer

MojoResult MojoMain(MojoHandle application_request) {
  mojo::ApplicationRunner runner(new pdf_viewer::PDFViewer());
  return runner.Run(application_request);
}
