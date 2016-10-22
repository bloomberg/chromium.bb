// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/native_widget_factory_mus.h"

#include "ash/common/wm_shell.h"
#include "ash/mus/bridge/wm_window_mus.h"
#include "ash/mus/window_manager.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace mus {

namespace {

views::NativeWidget* CreateNativeWidgetMus(
    WindowManager* window_manager,
    const views::Widget::InitParams& init_params,
    views::internal::NativeWidgetDelegate* delegate) {
  // TYPE_CONTROL widgets require a NativeWidgetAura. So we let this fall
  // through, so that the default NativeWidgetPrivate::CreateNativeWidget() is
  // used instead.
  if (init_params.type == views::Widget::InitParams::TYPE_CONTROL)
    return nullptr;
  std::map<std::string, std::vector<uint8_t>> props;
  views::NativeWidgetMus::ConfigurePropertiesForNewWindow(init_params, &props);
  return new views::NativeWidgetMus(
      delegate, window_manager->NewTopLevelWindow(&props),
      ui::mojom::CompositorFrameSinkType::DEFAULT);
}

}  // namespace

NativeWidgetFactoryMus::NativeWidgetFactoryMus(WindowManager* window_manager) {
  views::ViewsDelegate::GetInstance()->set_native_widget_factory(
      base::Bind(&CreateNativeWidgetMus, window_manager));
}

NativeWidgetFactoryMus::~NativeWidgetFactoryMus() {
  if (views::ViewsDelegate::GetInstance()) {
    views::ViewsDelegate::GetInstance()->set_native_widget_factory(
        views::ViewsDelegate::NativeWidgetFactory());
  }
}

}  // namespace mus
}  // namespace ash
