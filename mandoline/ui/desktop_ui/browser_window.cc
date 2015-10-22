// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/desktop_ui/browser_window.h"

#include "base/command_line.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/mus/public/cpp/event_matcher.h"
#include "components/mus/public/cpp/scoped_window_ptr.h"
#include "components/mus/public/cpp/window_tree_host_factory.h"
#include "mandoline/ui/desktop_ui/browser_commands.h"
#include "mandoline/ui/desktop_ui/browser_manager.h"
#include "mandoline/ui/desktop_ui/find_bar_view.h"
#include "mandoline/ui/desktop_ui/public/interfaces/omnibox.mojom.h"
#include "mandoline/ui/desktop_ui/toolbar_view.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/services/tracing/public/cpp/switches.h"
#include "mojo/services/tracing/public/interfaces/tracing.mojom.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/mus/native_widget_view_manager.h"
#include "ui/views/widget/widget_delegate.h"

namespace mandoline {

class ProgressView : public views::View {
 public:
  ProgressView() : progress_(0.f), loading_(false) {}
  ~ProgressView() override {}

  void SetProgress(double progress) {
    progress_ = progress;
    SchedulePaint();
  }

  void SetIsLoading(bool loading) {
    if (loading == loading_)
      return;

    loading_ = loading;
    progress_ = 0.f;
    SchedulePaint();
  }

 private:
  void OnPaint(gfx::Canvas* canvas) override {
    if (loading_) {
      canvas->FillRect(GetLocalBounds(), SK_ColorGREEN);
      gfx::Rect progress_rect = GetLocalBounds();
      progress_rect.set_width(progress_rect.width() * progress_);
      canvas->FillRect(progress_rect, SK_ColorRED);
    } else {
      canvas->FillRect(GetLocalBounds(), SK_ColorGRAY);
    }
  }

  double progress_;
  bool loading_;

  DISALLOW_COPY_AND_ASSIGN(ProgressView);
};

////////////////////////////////////////////////////////////////////////////////
// BrowserWindow, public:

BrowserWindow::BrowserWindow(mojo::ApplicationImpl* app,
                             mus::mojom::WindowTreeHostFactory* host_factory,
                             BrowserManager* manager)
    : app_(app),
      host_client_binding_(this),
      manager_(manager),
      toolbar_view_(nullptr),
      progress_bar_(nullptr),
      find_bar_view_(nullptr),
      root_(nullptr),
      content_(nullptr),
      omnibox_view_(nullptr),
      find_active_(0),
      find_count_(0),
      web_view_(this) {
  mus::mojom::WindowTreeHostClientPtr host_client;
  host_client_binding_.Bind(GetProxy(&host_client));
  mus::CreateWindowTreeHost(host_factory, host_client.Pass(), this, &host_);
}

void BrowserWindow::LoadURL(const GURL& url) {
  // Haven't been embedded yet, can't embed.
  // TODO(beng): remove this.
  if (!root_) {
    default_url_ = url;
    return;
  }

  if (!url.is_valid()) {
    ShowOmnibox();
    return;
  }

  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = url.spec();
  Embed(request.Pass());
}

void BrowserWindow::Close() {
  if (root_)
    mus::ScopedWindowPtr::DeleteWindowOrWindowManager(root_);
  else
    delete this;
}

void BrowserWindow::ShowOmnibox() {
  if (!omnibox_.get()) {
    mojo::URLRequestPtr request(mojo::URLRequest::New());
    request->url = mojo::String::From("mojo:omnibox");
    omnibox_connection_ = app_->ConnectToApplication(request.Pass());
    omnibox_connection_->AddService<ViewEmbedder>(this);
    omnibox_connection_->ConnectToService(&omnibox_);
    omnibox_connection_->SetRemoteServiceProviderConnectionErrorHandler(
      [this]() {
        // This will cause the connection to be re-established the next time
        // we come through this codepath.
        omnibox_.reset();
    });
  }
  omnibox_->ShowForURL(mojo::String::From(current_url_.spec()));
}

void BrowserWindow::ShowFind() {
  toolbar_view_->SetVisible(false);
  find_bar_view_->Show();
}

void BrowserWindow::GoBack() {
  web_view_.web_view()->GoBack();
}

void BrowserWindow::GoForward() {
  web_view_.web_view()->GoForward();
}

BrowserWindow::~BrowserWindow() {
  DCHECK(!root_);
  manager_->BrowserWindowClosed(this);
}

float BrowserWindow::DIPSToPixels(float value) const {
  return value * root_->viewport_metrics().device_pixel_ratio;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserWindow, mus::ViewTreeDelegate implementation:

void BrowserWindow::OnEmbed(mus::Window* root) {
  // BrowserWindow does not support being embedded more than once.
  CHECK(!root_);

  // Record when the browser window was displayed, used for performance testing.
  const base::Time display_time = base::Time::Now();

  root_ = root;

  host_->SetTitle("Mandoline");

  content_ = root_->connection()->NewWindow();
  Init(root_);

  host_->SetSize(mojo::Size::From(gfx::Size(1280, 800)));

  root_->AddChild(content_);
  content_->SetVisible(true);

  web_view_.Init(app_, content_);

  host_->AddAccelerator(static_cast<uint32_t>(BrowserCommand::CLOSE),
                        mus::CreateKeyMatcher(mojo::KEYBOARD_CODE_W,
                                              mojo::EVENT_FLAGS_CONTROL_DOWN));
  host_->AddAccelerator(static_cast<uint32_t>(BrowserCommand::FOCUS_OMNIBOX),
                        mus::CreateKeyMatcher(mojo::KEYBOARD_CODE_L,
                                              mojo::EVENT_FLAGS_CONTROL_DOWN));
  host_->AddAccelerator(static_cast<uint32_t>(BrowserCommand::NEW_WINDOW),
                        mus::CreateKeyMatcher(mojo::KEYBOARD_CODE_N,
                                              mojo::EVENT_FLAGS_CONTROL_DOWN));
  host_->AddAccelerator(static_cast<uint32_t>(BrowserCommand::SHOW_FIND),
                        mus::CreateKeyMatcher(mojo::KEYBOARD_CODE_F,
                                              mojo::EVENT_FLAGS_CONTROL_DOWN));
  host_->AddAccelerator(static_cast<uint32_t>(BrowserCommand::GO_BACK),
                        mus::CreateKeyMatcher(mojo::KEYBOARD_CODE_LEFT,
                                              mojo::EVENT_FLAGS_ALT_DOWN));
  host_->AddAccelerator(static_cast<uint32_t>(BrowserCommand::GO_FORWARD),
                        mus::CreateKeyMatcher(mojo::KEYBOARD_CODE_RIGHT,
                                              mojo::EVENT_FLAGS_ALT_DOWN));
  // Now that we're ready, load the default url.
  LoadURL(default_url_);

  // Record the time spent opening initial tabs, used for performance testing.
  const base::TimeDelta open_tabs_delta = base::Time::Now() - display_time;

  // Record the browser startup time metrics, used for performance testing.
  static bool recorded_browser_startup_metrics = false;
  if (!recorded_browser_startup_metrics &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          tracing::kEnableStatsCollectionBindings)) {
    mojo::URLRequestPtr request(mojo::URLRequest::New());
    request->url = mojo::String::From("mojo:tracing");
    tracing::StartupPerformanceDataCollectorPtr collector;
    app_->ConnectToService(request.Pass(), &collector);
    collector->SetBrowserWindowDisplayTime(display_time.ToInternalValue());
    collector->SetBrowserOpenTabsTimeDelta(open_tabs_delta.ToInternalValue());
    collector->SetBrowserMessageLoopStartTime(
        manager_->startup_time().ToInternalValue());
    recorded_browser_startup_metrics = true;
  }
}

void BrowserWindow::OnConnectionLost(mus::WindowTreeConnection* connection) {
  root_ = nullptr;
  delete this;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserWindow, mus::ViewTreeHostClient implementation:

void BrowserWindow::OnAccelerator(uint32_t id, mojo::EventPtr event) {
  switch (static_cast<BrowserCommand>(id)) {
    case BrowserCommand::CLOSE:
      Close();
      break;
    case BrowserCommand::NEW_WINDOW:
      manager_->CreateBrowser(GURL());
      break;
    case BrowserCommand::FOCUS_OMNIBOX:
      ShowOmnibox();
      break;
    case BrowserCommand::SHOW_FIND:
      ShowFind();
      break;
    case BrowserCommand::GO_BACK:
      GoBack();
      break;
    case BrowserCommand::GO_FORWARD:
      GoForward();
      break;
    default:
      NOTREACHED();
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// BrowserWindow, web_view::mojom::WebViewClient implementation:

void BrowserWindow::TopLevelNavigateRequest(mojo::URLRequestPtr request) {
  OnHideFindBar();
  Embed(request.Pass());
}

void BrowserWindow::TopLevelNavigationStarted(const mojo::String& url) {
  GURL gurl(url);
  bool changed = current_url_ != gurl;
  current_url_ = gurl;
  if (changed)
    toolbar_view_->SetOmniboxText(base::UTF8ToUTF16(current_url_.spec()));
}

void BrowserWindow::LoadingStateChanged(bool is_loading, double progress) {
  progress_bar_->SetIsLoading(is_loading);
  progress_bar_->SetProgress(progress);
}

void BrowserWindow::BackForwardChanged(
    web_view::mojom::ButtonState back_button,
    web_view::mojom::ButtonState forward_button) {
  toolbar_view_->SetBackForwardEnabled(
      back_button == web_view::mojom::ButtonState::BUTTON_STATE_ENABLED,
      forward_button == web_view::mojom::ButtonState::BUTTON_STATE_ENABLED);
}

void BrowserWindow::TitleChanged(const mojo::String& title) {
  base::string16 formatted =
      title.is_null() ? base::ASCIIToUTF16("Untitled")
                      : title.To<base::string16>() +
          base::ASCIIToUTF16(" - Mandoline");
  host_->SetTitle(mojo::String::From(formatted));
}

void BrowserWindow::FindInPageMatchCountUpdated(int32_t request_id,
                                                int32_t count,
                                                bool final_update) {
  find_count_ = count;
  find_bar_view_->SetMatchLabel(find_active_, find_count_);
}

void BrowserWindow::FindInPageSelectionUpdated(int32_t request_id,
                                               int32_t active_match_ordinal) {
  find_active_ = active_match_ordinal;
  find_bar_view_->SetMatchLabel(find_active_, find_count_);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserWindow, ViewEmbedder implementation:

void BrowserWindow::Embed(mojo::URLRequestPtr request) {
  const std::string string_url = request->url.To<std::string>();
  if (string_url == "mojo:omnibox") {
    EmbedOmnibox();
    return;
  }
  web_view_.web_view()->LoadRequest(request.Pass());
}

////////////////////////////////////////////////////////////////////////////////
// BrowserWindow, mojo::InterfaceFactory<ViewEmbedder> implementation:

void BrowserWindow::Create(mojo::ApplicationConnection* connection,
                           mojo::InterfaceRequest<ViewEmbedder> request) {
  view_embedder_bindings_.AddBinding(this, request.Pass());
}

////////////////////////////////////////////////////////////////////////////////
// BrowserWindow, views::LayoutManager implementation:

gfx::Size BrowserWindow::GetPreferredSize(const views::View* view) const {
  return gfx::Size();
}

void BrowserWindow::Layout(views::View* host) {
  // TODO(fsamuel): All bounds should be in physical pixels.
  gfx::Rect bounds_in_physical_pixels(host->bounds());
  float inverse_device_pixel_ratio =
      1.0f / root_->viewport_metrics().device_pixel_ratio;

  gfx::Rect toolbar_bounds = gfx::ScaleToEnclosingRect(
      bounds_in_physical_pixels, inverse_device_pixel_ratio);
  toolbar_bounds.Inset(10, 10, 10, toolbar_bounds.height() - 40);
  toolbar_view_->SetBoundsRect(toolbar_bounds);

  find_bar_view_->SetBoundsRect(toolbar_bounds);

  gfx::Rect progress_bar_bounds(toolbar_bounds.x(), toolbar_bounds.bottom() + 2,
                                toolbar_bounds.width(), 5);

  // The content view bounds are in physical pixels.
  mojo::Rect content_bounds_mojo;
  content_bounds_mojo.x = DIPSToPixels(progress_bar_bounds.x());
  content_bounds_mojo.y = DIPSToPixels(progress_bar_bounds.bottom()+ 10);
  content_bounds_mojo.width = DIPSToPixels(progress_bar_bounds.width());
  content_bounds_mojo.height =
      host->bounds().height() - content_bounds_mojo.y - DIPSToPixels(10);
  content_->SetBounds(content_bounds_mojo);

  // The omnibox view bounds are in physical pixels.
  omnibox_view_->SetBounds(
      mojo::TypeConverter<mojo::Rect, gfx::Rect>::Convert(
          bounds_in_physical_pixels));
}

////////////////////////////////////////////////////////////////////////////////
// BrowserWindow, FindBarDelegate implementation:

void BrowserWindow::OnDoFind(const std::string& find, bool forward) {
  web_view_.web_view()->Find(mojo::String::From(find), forward);
}

void BrowserWindow::OnHideFindBar() {
  toolbar_view_->SetVisible(true);
  find_bar_view_->Hide();
  web_view_.web_view()->StopFinding();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserWindow, private:

void BrowserWindow::Init(mus::Window* root) {
  DCHECK_GT(root->viewport_metrics().device_pixel_ratio, 0);
  if (!aura_init_)
    aura_init_.reset(new views::AuraInit(app_, "mandoline_ui.pak", root));

  root_ = root;
  omnibox_view_ = root_->connection()->NewWindow();
  root_->AddChild(omnibox_view_);

  views::WidgetDelegateView* widget_delegate = new views::WidgetDelegateView;
  widget_delegate->GetContentsView()->set_background(
    views::Background::CreateSolidBackground(0xFFDDDDDD));
  toolbar_view_ = new ToolbarView(this);
  progress_bar_ = new ProgressView;
  widget_delegate->GetContentsView()->AddChildView(toolbar_view_);
  widget_delegate->GetContentsView()->AddChildView(progress_bar_);
  widget_delegate->GetContentsView()->SetLayoutManager(this);

  find_bar_view_ = new FindBarView(this);
  widget_delegate->GetContentsView()->AddChildView(find_bar_view_);

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.native_widget =
      new views::NativeWidgetViewManager(widget, app_->shell(), root_);
  params.delegate = widget_delegate;
  params.bounds = root_->bounds().To<gfx::Rect>();
  widget->Init(params);
  widget->Show();
  root_->SetFocus();
}

void BrowserWindow::EmbedOmnibox() {
  mus::mojom::WindowTreeClientPtr view_tree_client;
  omnibox_->GetWindowTreeClient(GetProxy(&view_tree_client));
  omnibox_view_->Embed(view_tree_client.Pass());

  // TODO(beng): This should be handled sufficiently by
  //             OmniboxImpl::ShowWindow() but unfortunately view manager policy
  //             currently prevents the embedded app from changing window z for
  //             its own window.
  omnibox_view_->MoveToFront();
}

}  // namespace mandoline
