// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/omnibox/omnibox_application.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/mus/public/cpp/view.h"
#include "components/mus/public/cpp/view_tree_connection.h"
#include "components/mus/public/cpp/view_tree_delegate.h"
#include "components/url_formatter/url_fixer.h"
#include "mandoline/ui/desktop_ui/public/interfaces/view_embedder.mojom.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "ui/views/background.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/native_widget_view_manager.h"
#include "ui/views/widget/widget_delegate.h"

namespace mandoline {

////////////////////////////////////////////////////////////////////////////////
// OmniboxImpl

class OmniboxImpl : public mus::ViewTreeDelegate,
                    public views::LayoutManager,
                    public views::TextfieldController,
                    public Omnibox {
 public:
  OmniboxImpl(mojo::ApplicationImpl* app,
              mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<Omnibox> request);
  ~OmniboxImpl() override;

 private:
  // Overridden from mus::ViewTreeDelegate:
  void OnEmbed(mus::View* root) override;
  void OnConnectionLost(mus::ViewTreeConnection* connection) override;

  // Overridden from views::LayoutManager:
  gfx::Size GetPreferredSize(const views::View* view) const override;
  void Layout(views::View* host) override;

  // Overridden from views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

  // Overridden from Omnibox:
  void GetViewTreeClient(
      mojo::InterfaceRequest<mojo::ViewTreeClient> request) override;
  void ShowForURL(const mojo::String& url) override;

  void HideWindow();
  void ShowWindow();

  scoped_ptr<views::AuraInit> aura_init_;
  mojo::ApplicationImpl* app_;
  mus::View* root_;
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
}

bool OmniboxApplication::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService<Omnibox>(this);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxApplication, mojo::InterfaceFactory<Omnibox> implementation:

void OmniboxApplication::Create(mojo::ApplicationConnection* connection,
                                mojo::InterfaceRequest<Omnibox> request) {
  new OmniboxImpl(app_, connection, request.Pass());
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxImpl, public:

OmniboxImpl::OmniboxImpl(mojo::ApplicationImpl* app,
                         mojo::ApplicationConnection* connection,
                         mojo::InterfaceRequest<Omnibox> request)
    : app_(app),
      root_(nullptr),
      edit_(nullptr),
      binding_(this, request.Pass()) {
  connection->ConnectToService(&view_embedder_);
}
OmniboxImpl::~OmniboxImpl() {}

////////////////////////////////////////////////////////////////////////////////
// OmniboxImpl, mus::ViewTreeDelegate implementation:

void OmniboxImpl::OnEmbed(mus::View* root) {
  root_ = root;

  if (!aura_init_.get()) {
    aura_init_.reset(new views::AuraInit(app_, "mandoline_ui.pak", root_));
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
  params.native_widget =
      new views::NativeWidgetViewManager(widget, app_->shell(), root);
  params.delegate = widget_delegate;
  params.bounds = root->bounds().To<gfx::Rect>();
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  widget->Init(params);
  widget->Show();
  widget->GetCompositor()->SetBackgroundColor(
      SkColorSetA(SK_ColorBLACK, kOpacity));

  ShowWindow();
}

void OmniboxImpl::OnConnectionLost(mus::ViewTreeConnection* connection) {
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
    view_embedder_->Embed(request.Pass());
    HideWindow();
    return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxImpl, Omnibox implementation:

void OmniboxImpl::GetViewTreeClient(
    mojo::InterfaceRequest<mojo::ViewTreeClient> request) {
  mus::ViewTreeConnection::Create(
      this, request.Pass(),
      mus::ViewTreeConnection::CreateType::DONT_WAIT_FOR_EMBED);
}

void OmniboxImpl::ShowForURL(const mojo::String& url) {
  url_ = url;
  if (root_) {
    ShowWindow();
  } else {
    mojo::URLRequestPtr request(mojo::URLRequest::New());
    request->url = mojo::String::From("mojo:omnibox");
    view_embedder_->Embed(request.Pass());
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
