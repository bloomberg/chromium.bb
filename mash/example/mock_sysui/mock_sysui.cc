// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/example/mock_sysui/mock_sysui.h"

#include "components/mus/public/cpp/property_type_converters.h"
#include "mash/wm/public/interfaces/container.mojom.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "ui/gfx/canvas.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/mus/window_manager_connection.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

class DesktopBackground : public views::WidgetDelegateView {
 public:
  static void Create(mojo::Shell* shell) {
    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.delegate = new DesktopBackground;

    std::map<std::string, std::vector<uint8_t>> properties;
    properties[mash::wm::mojom::kWindowContainer_Property] =
        mojo::TypeConverter<const std::vector<uint8_t>, int32_t>::Convert(
            mash::wm::mojom::CONTAINER_USER_BACKGROUND);
    mus::Window* window =
        views::WindowManagerConnection::Get()->NewWindow(properties);
    params.native_widget = new views::NativeWidgetMus(
        widget, shell, window, mus::mojom::SURFACE_TYPE_DEFAULT);
    widget->Init(params);
    widget->Show();
  }

 private:
  DesktopBackground() {}
  ~DesktopBackground() override {}

  // Overridden from views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    canvas->FillRect(GetLocalBounds(), SK_ColorRED);
  }

  // Overridden from views::WidgetDelegate:
  views::View* GetContentsView() override { return this; }

  DISALLOW_COPY_AND_ASSIGN(DesktopBackground);
};

class Shelf : public views::WidgetDelegateView {
 public:
  static void Create(mojo::Shell* shell) {
    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.delegate = new Shelf;

    std::map<std::string, std::vector<uint8_t>> properties;
    properties[mash::wm::mojom::kWindowContainer_Property] =
        mojo::TypeConverter<const std::vector<uint8_t>, int32_t>::Convert(
            mash::wm::mojom::CONTAINER_USER_SHELF);
    mus::Window* window =
        views::WindowManagerConnection::Get()->NewWindow(properties);
    params.native_widget = new views::NativeWidgetMus(
        widget, shell, window, mus::mojom::SURFACE_TYPE_DEFAULT);
    widget->Init(params);
    widget->Show();
  }

 private:
  Shelf() {}
  ~Shelf() override {}

  // Overridden from views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    canvas->FillRect(GetLocalBounds(), SK_ColorYELLOW);
  }
  gfx::Size GetPreferredSize() const override { return gfx::Size(1, 48); }

  // Overridden from views::WidgetDelegate:
  views::View* GetContentsView() override { return this; }

  DISALLOW_COPY_AND_ASSIGN(Shelf);
};

}  // namespace

MockSysUI::MockSysUI() {}

MockSysUI::~MockSysUI() {
}

void MockSysUI::Initialize(mojo::ApplicationImpl* app) {
  tracing_.Initialize(app);

  aura_init_.reset(new views::AuraInit(app, "views_mus_resources.pak"));
  views::WindowManagerConnection::Create(app);

  DesktopBackground::Create(app->shell());
  Shelf::Create(app->shell());
}

bool MockSysUI::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  return false;
}
