// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/sysui_application.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/host/ash_window_tree_host_init_params.h"
#include "ash/host/ash_window_tree_host_platform.h"
#include "ash/mus/app_list_presenter_mus.h"
#include "ash/mus/keyboard_ui_mus.h"
#include "ash/mus/shelf_delegate_mus.h"
#include "ash/mus/shell_delegate_mus.h"
#include "ash/mus/stub_context_factory.h"
#include "ash/root_window_settings.h"
#include "ash/shell.h"
#include "ash/shell_init_params.h"
#include "ash/shell_window_ids.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/threading/sequenced_worker_pool.h"
#include "components/mus/public/cpp/property_type_converters.h"
#include "components/resource_provider/public/cpp/resource_loader.h"
#include "mash/wm/public/interfaces/ash_window_type.mojom.h"
#include "mash/wm/public/interfaces/container.mojom.h"
#include "ui/aura/env.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/message_center/message_center.h"
#include "ui/platform_window/stub/stub_window.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/mus/window_manager_connection.h"
#include "ui/views/views_delegate.h"

#if defined(OS_CHROMEOS)
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "ui/events/devices/device_data_manager.h"
#endif

using views::ViewsDelegate;

namespace ash {
namespace sysui {

namespace {

const char kResourceFileStrings[] = "ash_resources_strings.pak";
const char kResourceFile100[] = "ash_resources_100_percent.pak";
const char kResourceFile200[] = "ash_resources_200_percent.pak";

// Tries to determine the corresponding mash container from widget init params.
mash::wm::mojom::Container GetContainerId(
    const views::Widget::InitParams& params) {
  const int id = params.parent->id();
  if (id == kShellWindowId_DesktopBackgroundContainer)
    return mash::wm::mojom::Container::USER_BACKGROUND;
  // mash::wm::ShelfLayout manages both the shelf and the status area.
  if (id == kShellWindowId_ShelfContainer ||
      id == kShellWindowId_StatusContainer) {
    return mash::wm::mojom::Container::USER_SHELF;
  }

  // Determine the container based on Widget type.
  switch (params.type) {
    case views::Widget::InitParams::Type::TYPE_BUBBLE:
      return mash::wm::mojom::Container::BUBBLES;
    case views::Widget::InitParams::Type::TYPE_MENU:
      return mash::wm::mojom::Container::MENUS;
    case views::Widget::InitParams::Type::TYPE_TOOLTIP:
      return mash::wm::mojom::Container::TOOLTIPS;
    default:
      return mash::wm::mojom::Container::COUNT;
  }
}

// Tries to determine the corresponding ash window type from the ash container
// for the widget.
mash::wm::mojom::AshWindowType GetAshWindowType(aura::Window* container) {
  DCHECK(container);
  int id = container->id();
  if (id == kShellWindowId_ShelfContainer)
    return mash::wm::mojom::AshWindowType::SHELF;
  if (id == kShellWindowId_StatusContainer)
    return mash::wm::mojom::AshWindowType::STATUS_AREA;
  return mash::wm::mojom::AshWindowType::COUNT;
}

// Creates a StubWindow, which means this window never receives any input event,
// or displays anything to the user.
class AshWindowTreeHostMus : public AshWindowTreeHostPlatform {
 public:
  explicit AshWindowTreeHostMus(const gfx::Rect& initial_bounds)
      : AshWindowTreeHostPlatform() {
    std::unique_ptr<ui::PlatformWindow> window(new ui::StubWindow(this));
    window->SetBounds(initial_bounds);
    SetPlatformWindow(std::move(window));
  }

  ~AshWindowTreeHostMus() override {}

  void OnBoundsChanged(const gfx::Rect& bounds) override {
    if (platform_window())
      AshWindowTreeHostPlatform::OnBoundsChanged(bounds);
  }
};

AshWindowTreeHost* CreateWindowTreeHostMus(
    const AshWindowTreeHostInitParams& init_params) {
  return new AshWindowTreeHostMus(init_params.initial_bounds);
}

// Responsible for setting up a RootWindowSettings object for the root-window
// created for the views::Widget objects.
class NativeWidgetFactory {
 public:
  NativeWidgetFactory() {
    ViewsDelegate* views_delegate = ViewsDelegate::GetInstance();
    DCHECK(views_delegate);
    views_delegate->set_native_widget_factory(base::Bind(
        &NativeWidgetFactory::InitNativeWidget, base::Unretained(this)));
  }

  ~NativeWidgetFactory() {
    ViewsDelegate::GetInstance()->set_native_widget_factory(
        ViewsDelegate::NativeWidgetFactory());
  }

 private:
  views::NativeWidget* InitNativeWidget(
      const views::Widget::InitParams& params,
      views::internal::NativeWidgetDelegate* delegate) {
    std::map<std::string, std::vector<uint8_t>> properties;
    if (params.parent) {
      mash::wm::mojom::Container container = GetContainerId(params);
      if (container != mash::wm::mojom::Container::COUNT) {
        properties[mash::wm::mojom::kWindowContainer_Property] =
            mojo::ConvertTo<std::vector<uint8_t>>(
                static_cast<int32_t>(container));
      }
      mash::wm::mojom::AshWindowType type = GetAshWindowType(params.parent);
      if (type != mash::wm::mojom::AshWindowType::COUNT) {
        properties[mash::wm::mojom::kAshWindowType_Property] =
            mojo::ConvertTo<std::vector<uint8_t>>(static_cast<int32_t>(type));
      }
    }

    // AshInit installs a stub implementation of ui::ContextFactory, so that the
    // AshWindowTreeHost instances created do not actually show anything to the
    // user. However, when creating a views::Widget instance, the
    // WindowManagerConnection creates a WindowTreeHostMus instance, which
    // creates a ui::Compositor, and it needs a real ui::ContextFactory
    // implementation. So, unset the context-factory here temporarily when
    // creating NativeWidgetMus. But restore the stub instance afterwards, so
    // that it is used when new AshWindowTreeHost instances are created (e.g.
    // when a new monitor is attached).
    ui::ContextFactory* factory = aura::Env::GetInstance()->context_factory();
    aura::Env::GetInstance()->set_context_factory(nullptr);
    views::NativeWidgetMus* native_widget =
        static_cast<views::NativeWidgetMus*>(
            views::WindowManagerConnection::Get()->CreateNativeWidgetMus(
                properties, params, delegate));
    aura::Env::GetInstance()->set_context_factory(factory);

    // TODO: Set the correct display id here.
    InitRootWindowSettings(native_widget->GetRootWindow())->display_id =
        Shell::GetInstance()
            ->display_manager()
            ->GetPrimaryDisplayCandidate()
            .id();
    return native_widget;
  }

  DISALLOW_COPY_AND_ASSIGN(NativeWidgetFactory);
};

}  // namespace

class AshInit {
 public:
  AshInit() : worker_pool_(new base::SequencedWorkerPool(2, "AshWorkerPool")) {
    ui::RegisterPathProvider();
  }

  ~AshInit() { worker_pool_->Shutdown(); }

  aura::Window* root() { return ash::Shell::GetPrimaryRootWindow(); }

  void Initialize(::shell::Connector* connector,
                  const ::shell::Identity& identity) {
    InitializeResourceBundle(connector);
    aura_init_.reset(new views::AuraInit(connector, "views_mus_resources.pak"));
    views::WindowManagerConnection::Create(connector, identity);

    display::Screen* screen = display::Screen::GetScreen();
    DCHECK(screen);
    gfx::Size size = screen->GetPrimaryDisplay().bounds().size();

    // Uninstall the ScreenMus installed by WindowManagerConnection, so that ash
    // installs and uses the ScreenAsh. This can be removed once ash learns to
    // talk to mus for managing displays.
    // TODO(mfomitchev): We need to fix this. http://crbug.com/607300
    display::Screen::SetScreenInstance(nullptr);

    // Install some hook so that the WindowTreeHostMus created for widgets can
    // be hooked up correctly.
    native_widget_factory_.reset(new NativeWidgetFactory());

    ash::AshWindowTreeHost::SetFactory(base::Bind(&CreateWindowTreeHostMus));

    std::unique_ptr<ash::AppListPresenterMus> app_list_presenter =
        base::WrapUnique(new ash::AppListPresenterMus(connector));
    ash_delegate_ = new ShellDelegateMus(std::move(app_list_presenter));

    InitializeComponents();

    ash::ShellInitParams init_params;
    init_params.delegate = ash_delegate_;
    init_params.context_factory = new StubContextFactory;
    init_params.blocking_pool = worker_pool_.get();
    init_params.in_mus = true;
    init_params.keyboard_factory =
        base::Bind(&KeyboardUIMus::Create, connector);
    ash::Shell::CreateInstance(init_params);
    ash::Shell::GetInstance()->CreateShelf();
    ash::Shell::GetInstance()->UpdateAfterLoginStatusChange(
        ash::user::LOGGED_IN_USER);

    ash::Shell::GetPrimaryRootWindow()->GetHost()->Show();
    SetupWallpaper(SkColorSetARGB(255, 0, 255, 0));
  }

  void InitializeResourceBundle(::shell::Connector* connector) {
    if (ui::ResourceBundle::HasSharedInstance())
      return;

    std::set<std::string> resource_paths;
    resource_paths.insert(kResourceFileStrings);
    resource_paths.insert(kResourceFile100);
    resource_paths.insert(kResourceFile200);

    resource_provider::ResourceLoader loader(connector, resource_paths);
    if (!loader.BlockUntilLoaded())
      return;

    // Load ash resources and en-US strings; not 'common' (Chrome) resources.
    // TODO(msw): Check ResourceBundle::IsScaleFactorSupported; load 300% etc.
    ui::ResourceBundle::InitSharedInstanceWithPakFileRegion(
        loader.ReleaseFile(kResourceFileStrings),
        base::MemoryMappedFile::Region::kWholeFile);
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    rb.AddDataPackFromFile(loader.ReleaseFile(kResourceFile100),
                           ui::SCALE_FACTOR_100P);
    rb.AddDataPackFromFile(loader.ReleaseFile(kResourceFile200),
                           ui::SCALE_FACTOR_200P);
  }

  void SetupWallpaper(SkColor color) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(16, 16);
    bitmap.eraseColor(color);
#if !defined(NDEBUG)
    // In debug builds we generate a simple pattern that allows visually
    // notice if transparency is broken.
    {
      SkAutoLockPixels alp(bitmap);
      *bitmap.getAddr32(0, 0) = SkColorSetRGB(0, 0, 0);
    }
#endif
    gfx::ImageSkia wallpaper = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
    ash::Shell::GetInstance()
        ->desktop_background_controller()
        ->SetWallpaperImage(wallpaper, wallpaper::WALLPAPER_LAYOUT_TILE);
  }

  void InitializeComponents() {
    message_center::MessageCenter::Initialize();

#if defined(OS_CHROMEOS)
    ui::DeviceDataManager::CreateInstance();
    chromeos::DBusThreadManager::Initialize();
    bluez::BluezDBusManager::Initialize(
        chromeos::DBusThreadManager::Get()->GetSystemBus(),
        chromeos::DBusThreadManager::Get()->IsUsingStub(
            chromeos::DBusClientBundle::BLUETOOTH));
    chromeos::CrasAudioHandler::InitializeForTesting();
#endif
  }

 private:
  scoped_refptr<base::SequencedWorkerPool> worker_pool_;
  std::unique_ptr<views::AuraInit> aura_init_;
  ShellDelegateMus* ash_delegate_ = nullptr;
  std::unique_ptr<NativeWidgetFactory> native_widget_factory_;

  DISALLOW_COPY_AND_ASSIGN(AshInit);
};

SysUIApplication::SysUIApplication() {}

SysUIApplication::~SysUIApplication() {}

void SysUIApplication::Initialize(::shell::Connector* connector,
                                  const ::shell::Identity& identity,
                                  uint32_t id) {
  ash_init_.reset(new AshInit());
  ash_init_->Initialize(connector, identity);
}

bool SysUIApplication::AcceptConnection(::shell::Connection* connection) {
  connection->AddInterface<mash::shelf::mojom::ShelfController>(this);
  return true;
}

void SysUIApplication::Create(
    ::shell::Connection* connection,
    mojo::InterfaceRequest<mash::shelf::mojom::ShelfController> request) {
  mash::shelf::mojom::ShelfController* shelf_controller =
      static_cast<ShelfDelegateMus*>(Shell::GetInstance()->GetShelfDelegate());
  shelf_controller_bindings_.AddBinding(shelf_controller, std::move(request));
}

}  // namespace sysui
}  // namespace ash
