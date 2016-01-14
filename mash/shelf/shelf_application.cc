// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/shelf/shelf_application.h"

#include <stdint.h>

#include "components/mus/public/cpp/property_type_converters.h"
#include "mash/shelf/shelf_view.h"
#include "mash/wm/public/interfaces/container.mojom.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/mus/window_manager_connection.h"

namespace mash {
namespace shelf {

ShelfApplication::ShelfApplication() {}

ShelfApplication::~ShelfApplication() {}

void ShelfApplication::Initialize(mojo::ApplicationImpl* app) {
  tracing_.Initialize(app);

  aura_init_.reset(new views::AuraInit(app, "views_mus_resources.pak"));
  views::WindowManagerConnection::Create(app);

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = new ShelfView(app);

  std::map<std::string, std::vector<uint8_t>> properties;
  properties[mash::wm::mojom::kWindowContainer_Property] =
      mojo::TypeConverter<const std::vector<uint8_t>, int32_t>::Convert(
          mash::wm::mojom::CONTAINER_USER_SHELF);
  mus::Window* window =
      views::WindowManagerConnection::Get()->NewWindow(properties);
  params.native_widget = new views::NativeWidgetMus(
      widget, app->shell(), window, mus::mojom::SURFACE_TYPE_DEFAULT);
  widget->Init(params);
  widget->Show();
}

bool ShelfApplication::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  return true;
}

}  // namespace shelf
}  // namespace mash
