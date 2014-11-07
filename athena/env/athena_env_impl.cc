// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/env/public/athena_env.h"

#include <vector>

#include "athena/util/fill_layout_manager.h"
#include "base/sys_info.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/image_cursors.h"
#include "ui/chromeos/user_activity_power_manager_notifier.h"
#include "ui/display/chromeos/display_configurator.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/gfx/screen.h"
#include "ui/wm/core/compound_event_filter.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/input_method_event_filter.h"
#include "ui/wm/core/native_cursor_manager.h"
#include "ui/wm/core/native_cursor_manager_delegate.h"
#include "ui/wm/core/user_activity_detector.h"

namespace athena {

namespace {

AthenaEnv* instance = nullptr;

// Screen object used during shutdown.
gfx::Screen* screen_for_shutdown = nullptr;

gfx::Transform GetTouchTransform(const ui::DisplaySnapshot& display,
                                 const ui::TouchscreenDevice& touchscreen,
                                 const gfx::SizeF& framebuffer_size) {
  if (!display.current_mode())
    return gfx::Transform();

  gfx::SizeF display_size = display.current_mode()->size();
#if defined(USE_X11)
  gfx::SizeF touchscreen_size = framebuffer_size;
#elif defined(USE_OZONE)
  gfx::SizeF touchscreen_size = touchscreen.size;
#endif

  if (display_size.IsEmpty() || touchscreen_size.IsEmpty())
    return gfx::Transform();

  gfx::Transform transform;
  transform.Scale(display_size.width() / touchscreen_size.width(),
                  display_size.height() / touchscreen_size.height());

  return transform;
}

double GetTouchRadiusScale(const ui::DisplaySnapshot& display,
                           const ui::TouchscreenDevice& touchscreen,
                           const gfx::SizeF& framebuffer_size) {
  if (!display.current_mode())
    return 1;

  gfx::SizeF display_size = display.current_mode()->size();
#if defined(USE_X11)
  gfx::SizeF touchscreen_size = framebuffer_size;
#elif defined(USE_OZONE)
  gfx::SizeF touchscreen_size = touchscreen.size;
#endif

  if (display_size.IsEmpty() || touchscreen_size.IsEmpty())
    return 1;

  return std::sqrt(display_size.GetArea() / touchscreen_size.GetArea());
}

// TODO(flackr:oshima): Remove this once athena switches to share
// ash::DisplayManager.
class ScreenForShutdown : public gfx::Screen {
 public:
  // Creates and sets the screen for shutdown. Deletes existing one if any.
  static void Create(const gfx::Screen* screen) {
    delete screen_for_shutdown;
    screen_for_shutdown = new ScreenForShutdown(screen->GetPrimaryDisplay());
    gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE,
                                   screen_for_shutdown);
  }

 private:
  explicit ScreenForShutdown(const gfx::Display& primary_display)
      : primary_display_(primary_display) {}

  // gfx::Screen overrides:
  gfx::Point GetCursorScreenPoint() override { return gfx::Point(); }
  gfx::NativeWindow GetWindowUnderCursor() override { return NULL; }
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override {
    return nullptr;
  }
  int GetNumDisplays() const override { return 1; }
  std::vector<gfx::Display> GetAllDisplays() const override {
    std::vector<gfx::Display> displays(1, primary_display_);
    return displays;
  }
  gfx::Display GetDisplayNearestWindow(gfx::NativeView view) const override {
    return primary_display_;
  }
  gfx::Display GetDisplayNearestPoint(const gfx::Point& point) const override {
    return primary_display_;
  }
  gfx::Display GetDisplayMatching(const gfx::Rect& match_rect) const override {
    return primary_display_;
  }
  gfx::Display GetPrimaryDisplay() const override { return primary_display_; }
  void AddObserver(gfx::DisplayObserver* observer) override {
    NOTREACHED() << "Observer should not be added during shutdown";
  }
  void RemoveObserver(gfx::DisplayObserver* observer) override {}

  const gfx::Display primary_display_;

  DISALLOW_COPY_AND_ASSIGN(ScreenForShutdown);
};

// A class that bridges the gap between CursorManager and Aura. It borrows
// heavily from AshNativeCursorManager.
class AthenaNativeCursorManager : public wm::NativeCursorManager {
 public:
  explicit AthenaNativeCursorManager(aura::WindowTreeHost* host)
      : host_(host), image_cursors_(new ui::ImageCursors) {}
  ~AthenaNativeCursorManager() override {}

  // wm::NativeCursorManager overrides.
  void SetDisplay(const gfx::Display& display,
                  wm::NativeCursorManagerDelegate* delegate) override {
    if (image_cursors_->SetDisplay(display, display.device_scale_factor()))
      SetCursor(delegate->GetCursor(), delegate);
  }

  void SetCursor(gfx::NativeCursor cursor,
                 wm::NativeCursorManagerDelegate* delegate) override {
    image_cursors_->SetPlatformCursor(&cursor);
    cursor.set_device_scale_factor(image_cursors_->GetScale());
    delegate->CommitCursor(cursor);

    if (delegate->IsCursorVisible())
      ApplyCursor(cursor);
  }

  void SetVisibility(bool visible,
                     wm::NativeCursorManagerDelegate* delegate) override {
    delegate->CommitVisibility(visible);

    if (visible) {
      SetCursor(delegate->GetCursor(), delegate);
    } else {
      gfx::NativeCursor invisible_cursor(ui::kCursorNone);
      image_cursors_->SetPlatformCursor(&invisible_cursor);
      ApplyCursor(invisible_cursor);
    }
  }

  void SetCursorSet(ui::CursorSetType cursor_set,
                    wm::NativeCursorManagerDelegate* delegate) override {
    image_cursors_->SetCursorSet(cursor_set);
    delegate->CommitCursorSet(cursor_set);
    if (delegate->IsCursorVisible())
      SetCursor(delegate->GetCursor(), delegate);
  }

  void SetMouseEventsEnabled(
      bool enabled,
      wm::NativeCursorManagerDelegate* delegate) override {
    delegate->CommitMouseEventsEnabled(enabled);
    SetVisibility(delegate->IsCursorVisible(), delegate);
  }

 private:
  // Sets |cursor| as the active cursor within Aura.
  void ApplyCursor(gfx::NativeCursor cursor) { host_->SetCursor(cursor); }

  aura::WindowTreeHost* host_;  // Not owned.

  scoped_ptr<ui::ImageCursors> image_cursors_;

  DISALLOW_COPY_AND_ASSIGN(AthenaNativeCursorManager);
};

class AthenaEnvImpl : public AthenaEnv,
                      public aura::WindowTreeHostObserver,
                      public ui::DisplayConfigurator::Observer,
                      public ui::InputDeviceEventObserver {
 public:
  AthenaEnvImpl() : display_configurator_(new ui::DisplayConfigurator) {
    display_configurator_->Init(false);
    display_configurator_->ForceInitialConfigure(0);
    display_configurator_->AddObserver(this);

    ui::DeviceDataManager::GetInstance()->AddObserver(this);

    gfx::Size screen_size = GetPrimaryDisplaySize();
    if (screen_size.IsEmpty()) {
      // TODO(oshima): Remove this hack.
      if (base::SysInfo::IsRunningOnChromeOS())
        screen_size.SetSize(2560, 1600);
      else
        screen_size.SetSize(1280, 720);
    }
    screen_.reset(aura::TestScreen::Create(screen_size));

    gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, screen_.get());
    host_.reset(screen_->CreateHostForPrimaryDisplay());
    host_->InitHost();

    aura::Window* root_window = GetHost()->window();
    input_method_filter_.reset(
        new wm::InputMethodEventFilter(host_->GetAcceleratedWidget()));
    input_method_filter_->SetInputMethodPropertyInRootWindow(root_window);

    root_window_event_filter_.reset(new wm::CompoundEventFilter);
    host_->window()->AddPreTargetHandler(root_window_event_filter_.get());

    root_window_event_filter_->AddHandler(input_method_filter_.get());

    capture_client_.reset(
        new aura::client::DefaultCaptureClient(host_->window()));

    // Ensure new windows fill the display.
    root_window->SetLayoutManager(new FillLayoutManager(root_window));

    cursor_manager_.reset(
        new wm::CursorManager(scoped_ptr<wm::NativeCursorManager>(
            new AthenaNativeCursorManager(host_.get()))));
    cursor_manager_->SetDisplay(
        gfx::Screen::GetNativeScreen()->GetPrimaryDisplay());
    cursor_manager_->SetCursor(ui::kCursorPointer);
    aura::client::SetCursorClient(host_->window(), cursor_manager_.get());

    user_activity_detector_.reset(new wm::UserActivityDetector);
    host_->event_processor()->GetRootTarget()->AddPreTargetHandler(
        user_activity_detector_.get());
    user_activity_notifier_.reset(new ui::UserActivityPowerManagerNotifier(
        user_activity_detector_.get()));

    host_->AddObserver(this);
    host_->Show();

    DCHECK(!instance);
    instance = this;
  }

  ~AthenaEnvImpl() override {
    instance = nullptr;

    host_->RemoveObserver(this);
    if (input_method_filter_)
      root_window_event_filter_->RemoveHandler(input_method_filter_.get());
    if (user_activity_detector_) {
      host_->event_processor()->GetRootTarget()->RemovePreTargetHandler(
          user_activity_detector_.get());
    }
    root_window_event_filter_.reset();
    capture_client_.reset();
    input_method_filter_.reset();
    cursor_manager_.reset();
    user_activity_notifier_.reset();
    user_activity_detector_.reset();

    input_method_filter_.reset();
    host_.reset();

    ScreenForShutdown::Create(screen_.get());
    screen_.reset();
    aura::Env::DeleteInstance();

    ui::DeviceDataManager::GetInstance()->RemoveObserver(this);

    display_configurator_->RemoveObserver(this);
    display_configurator_.reset();
  }

 private:
  struct Finder {
    explicit Finder(const base::Closure& c) : closure(c) {}
    bool operator()(const base::Closure& other) {
      return closure.Equals(other);
    }
    base::Closure closure;
  };

  // AthenaEnv:
  aura::WindowTreeHost* GetHost() override { return host_.get(); }

  void SetDisplayWorkAreaInsets(const gfx::Insets& insets) override {
    screen_->SetWorkAreaInsets(insets);
  }

  void AddTerminatingCallback(const base::Closure& closure) override {
    if (closure.is_null())
      return;
    DCHECK(terminating_callbacks_.end() ==
           std::find_if(terminating_callbacks_.begin(),
                        terminating_callbacks_.end(),
                        Finder(closure)));
    terminating_callbacks_.push_back(closure);
  }

  void RemoveTerminatingCallback(const base::Closure& closure) override {
    std::vector<base::Closure>::iterator iter =
        std::find_if(terminating_callbacks_.begin(),
                     terminating_callbacks_.end(),
                     Finder(closure));
    if (iter != terminating_callbacks_.end())
      terminating_callbacks_.erase(iter);
  }

  void OnTerminating() override {
    for (std::vector<base::Closure>::iterator iter =
             terminating_callbacks_.begin();
         iter != terminating_callbacks_.end();
         ++iter) {
      iter->Run();
    }
  }

  // ui::DisplayConfigurator::Observer:
  void OnDisplayModeChanged(
      const std::vector<ui::DisplayConfigurator::DisplayState>& displays)
      override {
    MapTouchscreenToDisplay();

    gfx::Size size = GetPrimaryDisplaySize();
    if (!size.IsEmpty())
      host_->UpdateRootWindowSize(size);
  }

  // ui::InputDeviceEventObserver:
  void OnTouchscreenDeviceConfigurationChanged() override {
    MapTouchscreenToDisplay();
  }

  void OnKeyboardDeviceConfigurationChanged() override {}

  // aura::WindowTreeHostObserver:
  void OnHostCloseRequested(const aura::WindowTreeHost* host) override {
    base::MessageLoopForUI::current()->PostTask(
        FROM_HERE, base::MessageLoop::QuitClosure());
  }

  gfx::Size GetPrimaryDisplaySize() const {
    const std::vector<ui::DisplayConfigurator::DisplayState>& displays =
        display_configurator_->cached_displays();
    if (displays.empty())
      return gfx::Size();
    const ui::DisplayMode* mode = displays[0].display->current_mode();
    return mode ? mode->size() : gfx::Size();
  }

  void MapTouchscreenToDisplay() const {
    auto device_manager = ui::DeviceDataManager::GetInstance();
    auto displays = display_configurator_->cached_displays();
    auto touchscreens = device_manager->touchscreen_devices();

    if (displays.empty() || touchscreens.empty())
      return;

    gfx::SizeF framebuffer_size = display_configurator_->framebuffer_size();
    device_manager->ClearTouchTransformerRecord();
    device_manager->UpdateTouchInfoForDisplay(
        displays[0].display->display_id(),
        touchscreens[0].id,
        GetTouchTransform(*displays[0].display,
                          touchscreens[0],
                          framebuffer_size));
    device_manager->UpdateTouchRadiusScale(
        touchscreens[0].id,
        GetTouchRadiusScale(*displays[0].display,
                            touchscreens[0],
                            framebuffer_size));
  }

  scoped_ptr<aura::TestScreen> screen_;
  scoped_ptr<aura::WindowTreeHost> host_;

  scoped_ptr<wm::InputMethodEventFilter> input_method_filter_;
  scoped_ptr<wm::CompoundEventFilter> root_window_event_filter_;
  scoped_ptr<aura::client::DefaultCaptureClient> capture_client_;
  scoped_ptr<wm::CursorManager> cursor_manager_;
  scoped_ptr<wm::UserActivityDetector> user_activity_detector_;
  scoped_ptr<ui::DisplayConfigurator> display_configurator_;
  scoped_ptr<ui::UserActivityPowerManagerNotifier> user_activity_notifier_;

  std::vector<base::Closure> terminating_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(AthenaEnvImpl);
};

}  // namespace

// static
void AthenaEnv::Create() {
  DCHECK(!instance);
  new AthenaEnvImpl();
}

AthenaEnv* AthenaEnv::Get() {
  DCHECK(instance);
  return instance;
}

// static

// static
void AthenaEnv::Shutdown() {
  DCHECK(instance);
  delete instance;
}

}  // namespace athena
