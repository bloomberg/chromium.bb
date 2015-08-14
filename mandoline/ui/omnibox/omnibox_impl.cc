// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/ui/omnibox/omnibox_impl.h"

#include "base/strings/string16.h"
#include "components/view_manager/public/cpp/view_manager_client_factory.h"
#include "mandoline/ui/aura/aura_init.h"
#include "mandoline/ui/aura/native_widget_view_manager.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "ui/views/background.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget_delegate.h"

namespace mandoline {

////////////////////////////////////////////////////////////////////////////////
// OmniboxImpl, public:

OmniboxImpl::OmniboxImpl()
    : app_impl_(nullptr), root_(nullptr), edit_(nullptr) {}
OmniboxImpl::~OmniboxImpl() {}

////////////////////////////////////////////////////////////////////////////////
// OmniboxImpl, mojo::ApplicationDelegate implementation:

void OmniboxImpl::Initialize(mojo::ApplicationImpl* app) {
  app_impl_ = app;
  view_manager_client_factory_.reset(
      new mojo::ViewManagerClientFactory(app->shell(), this));
}

bool OmniboxImpl::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService<Omnibox>(this);
  connection->AddService(view_manager_client_factory_.get());
  connection->ConnectToService(&view_embedder_);
  return true;
}

bool OmniboxImpl::ConfigureOutgoingConnection(
    mojo::ApplicationConnection* connection) {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxImpl, mojo::ViewManagerDelegate implementation:

void OmniboxImpl::OnEmbed(mojo::View* root) {
  root_ = root;

  if (!aura_init_.get()) {
    aura_init_.reset(new AuraInit(root, app_impl_->shell()));
    edit_ = new views::Textfield;
    edit_->set_controller(this);
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
      new NativeWidgetViewManager(widget, app_impl_->shell(), root);
  params.delegate = widget_delegate;
  params.bounds = root->bounds().To<gfx::Rect>();
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  widget->Init(params);
  widget->Show();
  widget->GetCompositor()->SetBackgroundColor(
      SkColorSetA(SK_ColorBLACK, kOpacity));
  edit_->SetText(url_.To<base::string16>());
  edit_->SelectAll(false);
  edit_->RequestFocus();

  ShowWindow();
}

void OmniboxImpl::OnViewManagerDestroyed(mojo::ViewManager* view_manager) {
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
    request->url = mojo::String::From<base::string16>(sender->text());
    view_embedder_->Embed(request.Pass());
    HideWindow();
    return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxImpl, mojo::InterfaceFactory<Omnibox> implementation:

void OmniboxImpl::Create(mojo::ApplicationConnection* connection,
                         mojo::InterfaceRequest<Omnibox> request) {
  // TODO(beng): methinks this doesn't work well across multiple browser
  //             windows...
  bindings_.AddBinding(this, request.Pass());
}

////////////////////////////////////////////////////////////////////////////////
// OmniboxImpl, Omnibox implementation:

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
}

void OmniboxImpl::HideWindow() {
  DCHECK(root_);
  root_->SetVisible(false);
}

}  // namespace mandoline
