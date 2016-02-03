// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "components/filesystem/public/cpp/prefs/pref_service_factory.h"
#include "components/mus/public/cpp/property_type_converters.h"
#include "components/prefs/pref_registry_simple.h"
#include "mash/wm/public/interfaces/container.mojom.h"
#include "mojo/public/c/system/main.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "mojo/shell/public/cpp/application_connection.h"
#include "mojo/shell/public/cpp/application_delegate.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "mojo/shell/public/cpp/application_runner.h"
#include "ui/gfx/canvas.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/mus/window_manager_connection.h"
#include "ui/views/widget/widget_delegate.h"

namespace mash {
namespace wallpaper {
namespace {

class Wallpaper : public views::WidgetDelegateView {
 public:
  Wallpaper() {}
  ~Wallpaper() override {}

  // Overridden from views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    canvas->FillRect(GetLocalBounds(), SK_ColorRED);
  }

  // Overridden from views::WidgetDelegate:
  views::View* GetContentsView() override { return this; }

  DISALLOW_COPY_AND_ASSIGN(Wallpaper);
};

class WallpaperApplicationDelegate : public mojo::ApplicationDelegate {
 public:
  WallpaperApplicationDelegate() {}
  ~WallpaperApplicationDelegate() override {}

  void OnLoaded(bool whatever) {
    // TODO(erg): Now do something with this result.
  }

 private:
  // mojo::ApplicationDelegate:
  void Initialize(mojo::ApplicationImpl* app) override {
    tracing_.Initialize(app);

    aura_init_.reset(new views::AuraInit(app, "views_mus_resources.pak"));
    views::WindowManagerConnection::Create(app);

    scoped_refptr<PrefRegistrySimple> registry = new PrefRegistrySimple;
    registry->RegisterStringPref("filename", "", 0);
    pref_service_ = filesystem::CreatePrefService(app, registry.get());
    pref_service_->AddPrefInitObserver(base::Bind(
        &WallpaperApplicationDelegate::OnLoaded, base::Unretained(this)));

    views::Widget* widget = new views::Widget;
    views::Widget::InitParams params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.delegate = new Wallpaper;

    std::map<std::string, std::vector<uint8_t>> properties;
    properties[mash::wm::mojom::kWindowContainer_Property] =
        mojo::TypeConverter<const std::vector<uint8_t>, int32_t>::Convert(
            static_cast<int32_t>(mash::wm::mojom::Container::USER_BACKGROUND));
    mus::Window* window =
        views::WindowManagerConnection::Get()->NewWindow(properties);
    params.native_widget = new views::NativeWidgetMus(
        widget, app->shell(), window, mus::mojom::SurfaceType::DEFAULT);
    widget->Init(params);
    widget->Show();
  }

  mojo::TracingImpl tracing_;
  scoped_ptr<views::AuraInit> aura_init_;
  scoped_ptr<PrefService> pref_service_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperApplicationDelegate);
};

}  // namespace
}  // namespace wallpaper
}  // namespace mash

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunner runner(
      new mash::wallpaper::WallpaperApplicationDelegate);
  return runner.Run(shell_handle);
}
