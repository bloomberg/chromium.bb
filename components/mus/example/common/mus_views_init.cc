// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/example/common/mus_views_init.h"

#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/public/interfaces/view_tree.mojom.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/network/network_type_converters.h"
#include "ui/gfx/display.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/mus/native_widget_view_manager.h"

namespace mojo {

gfx::Display::Rotation GFXRotationFromMojomRotation(
    mus::mojom::Rotation input) {
  switch (input) {
    case mus::mojom::ROTATION_VALUE_0:
      return gfx::Display::ROTATE_0;
    case mus::mojom::ROTATION_VALUE_90:
      return gfx::Display::ROTATE_90;
    case mus::mojom::ROTATION_VALUE_180:
      return gfx::Display::ROTATE_180;
    case mus::mojom::ROTATION_VALUE_270:
      return gfx::Display::ROTATE_270;
  }
  return gfx::Display::ROTATE_0;
}

template <>
struct TypeConverter<gfx::Display, mus::mojom::DisplayPtr> {
  static gfx::Display Convert(const mus::mojom::DisplayPtr& input) {
    gfx::Display result;
    result.set_id(input->id);
    result.SetScaleAndBounds(input->device_pixel_ratio,
                             input->bounds.To<gfx::Rect>());
    gfx::Rect work_area(
        gfx::ToFlooredPoint(gfx::ScalePoint(
            gfx::Point(input->work_area->x, input->work_area->y),
            1.0f / input->device_pixel_ratio)),
        gfx::ScaleToFlooredSize(
            gfx::Size(input->work_area->width, input->work_area->height),
            1.0f / input->device_pixel_ratio));
    result.set_work_area(work_area);
    result.set_rotation(GFXRotationFromMojomRotation(input->rotation));
    return result;
  }
};

}  // namespace mojo

namespace {

std::vector<gfx::Display> GetDisplaysFromWindowManager(
    mojo::ApplicationImpl* app) {
  mus::mojom::WindowManagerPtr window_manager;
  app->ConnectToService(mojo::URLRequest::From(std::string("mojo:example_wm")),
                        &window_manager);
  std::vector<gfx::Display> displays;
  window_manager->GetDisplays(
      [&displays](mojo::Array<mus::mojom::DisplayPtr> mojom_displays) {
        displays = mojom_displays.To<std::vector<gfx::Display>>();
      });
  CHECK(window_manager.WaitForIncomingResponse());
  return displays;
}
}

MUSViewsInit::MUSViewsInit(mojo::ApplicationImpl* app)
    : app_(app),
      aura_init_(app,
                 "example_resources.pak",
                 GetDisplaysFromWindowManager(app)) {}

MUSViewsInit::~MUSViewsInit() {}

mus::Window* MUSViewsInit::CreateWindow() {
  mus::mojom::WindowManagerPtr wm;
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = "mojo:example_wm";
  app_->ConnectToService(request.Pass(), &wm);
  mojo::ViewTreeClientPtr view_tree_client;
  mojo::InterfaceRequest<mojo::ViewTreeClient> view_tree_client_request =
      GetProxy(&view_tree_client);
  wm->OpenWindow(view_tree_client.Pass());
  mus::WindowTreeConnection* view_tree_connection =
      mus::WindowTreeConnection::Create(
          this, view_tree_client_request.Pass(),
          mus::WindowTreeConnection::CreateType::WAIT_FOR_EMBED);
  DCHECK(view_tree_connection->GetRoot());
  return view_tree_connection->GetRoot();
}

views::NativeWidget* MUSViewsInit::CreateNativeWidget(
    views::internal::NativeWidgetDelegate* delegate) {
  return new views::NativeWidgetViewManager(delegate, app_->shell(),
                                            CreateWindow());
}

void MUSViewsInit::OnBeforeWidgetInit(
    views::Widget::InitParams* params,
    views::internal::NativeWidgetDelegate* delegate) {}

void MUSViewsInit::OnEmbed(mus::Window* root) {
}

void MUSViewsInit::OnConnectionLost(mus::WindowTreeConnection* connection) {}

#if defined(OS_WIN)
HICON MUSViewsInit::GetSmallWindowIcon() const {
  return nullptr;
}
#endif
