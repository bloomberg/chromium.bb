// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/omnibox/omnibox_application.h"

#include <utility>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/public/cpp/window_tree_delegate.h"
#include "components/url_formatter/url_fixer.h"
#include "mandoline/ui/desktop_ui/public/interfaces/view_embedder.mojom.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "ui/mojo/init/ui_init.h"
#include "ui/views/background.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/display_converter.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/widget/widget_delegate.h"

namespace mandoline {

////////////////////////////////////////////////////////////////////////////////
// OmniboxImpl

class OmniboxImpl : public mus::WindowTreeDelegate,
                    public views::LayoutManager,
                    public views::TextfieldController,
                    public Omnibox {
 public:
  OmniboxImpl(mojo::ApplicationImpl* app,
              mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<Omnibox> request);
  ~OmniboxImpl() override;

 private:
  // Overridden from mus::WindowTreeDelegate:
  void OnEmbed(mus::Window* root) override;
  void OnConnectionLost(mus::WindowTreeConnection* connection) override;

  // Overridden from views::LayoutManager:
  gfx::Size GetPreferredSize(const views::View* view) const override;
  void Layout(views::View* host) override;

  // Overridden from views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  // Overridden from Omnibox:
  void GetWindowTreeClient(
      mojo::InterfaceRequest<mus::mojom::WindowTreeClient> request) override;
  void ShowForURL(const mojo::String& url) override;

  void HideWindow();
  void ShowWindow();

  scoped_ptr<ui::mojo::UIInit> ui_init_;
  scoped_ptr<views::AuraInit> aura_init_;
  mojo::ApplicationImpl* app_;
  mus::Window* root_;
  mojo::String url_;
  views::Textfield* edit_;
  mojo::Binding<Omnibox> binding_;
  ViewEmbedderPtr view_embedder_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxImpl);
};

////////////////////////////////////////////////////////////////////////////////
// OmniboxApplication, public:

OmniboxApplication::OmniboxApplication() : app_(nullptr) {}
OmniboxApplication::~OmniboxApplication() {}

////////////////////////////////////////////////////////////////////////////////
// OmniboxApplication, mojo::ApplicationDelegate implementation:

void OmniboxApplication::Initialize(mojo::ApplicationImpl* app) {
  app_ = app;
  tracing_.Initialize(app);
}

bool OmniboxApplication::AcceptConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService<Omnibox>(this);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxApplication, mojo::InterfaceFactory<Omnibox> implementation:

void OmniboxApplication::Create(mojo::ApplicationConnection* connection,
                                mojo::InterfaceRequest<Omnibox> request) {
  new OmniboxImpl(app_, connection, std::move(request));
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxImpl, public:

OmniboxImpl::OmniboxImpl(mojo::ApplicationImpl* app,
                         mojo::ApplicationConnection* connection,
                         mojo::InterfaceRequest<Omnibox> request)
    : app_(app),
      root_(nullptr),
      edit_(nullptr),
      binding_(this, std::move(request)) {
  connection->ConnectToService(&view_embedder_);
}
OmniboxImpl::~OmniboxImpl() {}

////////////////////////////////////////////////////////////////////////////////
// OmniboxImpl, mus::WindowTreeDelegate implementation:

void OmniboxImpl::OnEmbed(mus::Window* root) {
  root_ = root;

  if (!aura_init_.get()) {
    ui_init_.reset(new ui::mojo::UIInit(views::GetDisplaysFromWindow(root_)));
    aura_init_.reset(new views::AuraInit(app_, "mandoline_ui.pak"));
    edit_ = new views::Textfield;
    edit_->set_controller(this);
    edit_->SetTextInputType(ui::TEXT_INPUT_TYPE_URL);
  }

  const int kOpacity = 0xC0;
  views::WidgetDelegateView* widget_delegate = new views::WidgetDelegateView;
  widget_delegate->GetContentsView()->set_background(
      views::Background::CreateSolidBackground(
          SkColorSetA(0xDDDDDD, kOpacity)));
  widget_delegate->GetContentsView()->AddChildView(edit_);
  widget_delegate->GetContentsView()->SetLayoutManager(this);

  // TODO(beng): we may be leaking these on subsequent calls to OnEmbed()...
  //             probably should only allow once instance per view.
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.native_widget = new views::NativeWidgetMus(
      widget, app_->shell(), root, mus::mojom::SurfaceType::DEFAULT);
  params.delegate = widget_delegate;
  params.bounds = root->bounds();
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  widget->Init(params);
  widget->Show();
  widget->GetCompositor()->SetBackgroundColor(
      SkColorSetA(SK_ColorBLACK, kOpacity));

  ShowWindow();
}

void OmniboxImpl::OnConnectionLost(mus::WindowTreeConnection* connection) {
  root_ = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxImpl, views::LayoutManager implementation:

gfx::Size OmniboxImpl::GetPreferredSize(const views::View* view) const {
  return gfx::Size();
}

void OmniboxImpl::Layout(views::View* host) {
  gfx::Rect edit_bounds = host->bounds();
  edit_bounds.Inset(10, 10, 10, host->bounds().height() - 40);
  edit_->SetBoundsRect(edit_bounds);

  // TODO(beng): layout dropdown...
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxImpl, views::TextfieldController implementation:

bool OmniboxImpl::HandleKeyEvent(views::Textfield* sender,
                                 const ui::KeyEvent& key_event) {
  if (key_event.key_code() == ui::VKEY_RETURN) {
    // TODO(beng): call back to browser.
    mojo::URLRequestPtr request(mojo::URLRequest::New());
    GURL url = url_formatter::FixupURL(base::UTF16ToUTF8(sender->text()),
                                       std::string());
    request->url = url.spec();
    view_embedder_->Embed(std::move(request));
    HideWindow();
    return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxImpl, Omnibox implementation:

void OmniboxImpl::GetWindowTreeClient(
    mojo::InterfaceRequest<mus::mojom::WindowTreeClient> request) {
  mus::WindowTreeConnection::Create(
      this, std::move(request),
      mus::WindowTreeConnection::CreateType::DONT_WAIT_FOR_EMBED);
}

void OmniboxImpl::ShowForURL(const mojo::String& url) {
  url_ = url;
  if (root_) {
    ShowWindow();
  } else {
    mojo::URLRequestPtr request(mojo::URLRequest::New());
    request->url = mojo::String::From("mojo:omnibox");
    view_embedder_->Embed(std::move(request));
  }
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxImpl, private:

void OmniboxImpl::ShowWindow() {
  DCHECK(root_);
  root_->SetVisible(true);
  root_->SetFocus();
  root_->MoveToFront();
  edit_->SetText(url_.To<base::string16>());
  edit_->SelectAll(false);
  edit_->RequestFocus();
}

void OmniboxImpl::HideWindow() {
  DCHECK(root_);
  root_->SetVisible(false);
}

}  // namespace mandoline
