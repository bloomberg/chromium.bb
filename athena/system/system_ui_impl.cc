// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/system/public/system_ui.h"

#include "athena/screen/public/screen_manager.h"
#include "athena/system/background_controller.h"
#include "athena/system/orientation_controller.h"
#include "athena/system/shutdown_dialog.h"
#include "athena/system/status_icon_container_view.h"
#include "athena/system/time_view.h"
#include "athena/util/athena_constants.h"
#include "athena/util/container_priorities.h"
#include "athena/util/fill_layout_manager.h"
#include "athena/wm/public/window_manager.h"
#include "athena/wm/public/window_manager_observer.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/window.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/visibility_controller.h"
#include "ui/wm/core/window_animations.h"

namespace athena {
namespace {

SystemUI* instance = nullptr;

// View which positions the TimeView on the left and the StatusIconView on the
// right.
class SystemInfoView : public views::View {
 public:
  SystemInfoView(SystemUI::ColorScheme color_scheme)
      : time_view_(new TimeView(color_scheme)),
        status_icon_view_(new StatusIconContainerView(color_scheme)) {
    AddChildView(time_view_);
    AddChildView(status_icon_view_);
  }

  ~SystemInfoView() override {}

  // views::View:
  virtual gfx::Size GetPreferredSize() const override {
    // The view should be as wide as its parent view.
    return gfx::Size(0,
                     std::max(time_view_->GetPreferredSize().height(),
                              status_icon_view_->GetPreferredSize().height()));
  }

  virtual void Layout() override {
    time_view_->SetBoundsRect(gfx::Rect(time_view_->GetPreferredSize()));
    gfx::Size status_icon_preferred_size =
        status_icon_view_->GetPreferredSize();
    status_icon_view_->SetBoundsRect(
        gfx::Rect(width() - status_icon_preferred_size.width(),
                  0,
                  status_icon_preferred_size.width(),
                  status_icon_preferred_size.height()));
  }

  virtual void ChildPreferredSizeChanged(views::View* child) override {
    // Relayout to take into account changes in |status_icon_view_|'s width.
    // Assume that |time_view_|'s and |status_icon_view_|'s preferred height
    // does not change.
    Layout();
  }

 private:
  views::View* time_view_;
  views::View* status_icon_view_;

  DISALLOW_COPY_AND_ASSIGN(SystemInfoView);
};

class SystemUIImpl : public SystemUI, public WindowManagerObserver {
 public:
  SystemUIImpl(scoped_refptr<base::TaskRunner> blocking_task_runner)
      : orientation_controller_(new OrientationController()),
        background_container_(nullptr),
        system_info_widget_(nullptr) {
    orientation_controller_->InitWith(blocking_task_runner);
    WindowManager::Get()->AddObserver(this);
  }

  ~SystemUIImpl() override {
    WindowManager::Get()->RemoveObserver(this);

    // Stops file watching now if exists. Waiting until message loop shutdon
    // leads to FilePathWatcher crash.
    orientation_controller_->Shutdown();
  }

  void Init() {
    ScreenManager* screen_manager = ScreenManager::Get();
    background_container_ = screen_manager->CreateContainer(
        ScreenManager::ContainerParams("AthenaBackground", CP_BACKGROUND));
    background_container_->SetLayoutManager(
        new FillLayoutManager(background_container_));

    shutdown_dialog_.reset(new ShutdownDialog());
    background_controller_.reset(
        new BackgroundController(background_container_));
  }

 private:
  // SystemUI:
  void SetBackgroundImage(const gfx::ImageSkia& image) override {
    background_controller_->SetImage(image);
  }

  // WindowManagerObserver:
  void OnOverviewModeEnter() override {
    DCHECK(!system_info_widget_);

    ScreenManager* screen_manager = ScreenManager::Get();
    aura::Window* container = screen_manager->CreateContainer(
        ScreenManager::ContainerParams("SystemInfo", CP_SYSTEM_INFO));
    wm::SetChildWindowVisibilityChangesAnimated(container);

    system_info_widget_ = new views::Widget();
    views::Widget::InitParams widget_params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    widget_params.parent = container;
    widget_params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
    widget_params.bounds =
        gfx::Rect(0, 0, container->bounds().width(), kSystemUIHeight);
    system_info_widget_->Init(widget_params);
    system_info_widget_->SetContentsView(
        new SystemInfoView(SystemUI::COLOR_SCHEME_LIGHT));

    wm::SetWindowVisibilityAnimationType(
        system_info_widget_->GetNativeWindow(),
        wm::WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL);
    wm::SetWindowVisibilityAnimationVerticalPosition(
        system_info_widget_->GetNativeWindow(), -kSystemUIHeight);

    system_info_widget_->Show();
  }

  void OnOverviewModeExit() override {
    // Deleting the container deletes its child windows which deletes
    // |system_info_widget_|.
    delete system_info_widget_->GetNativeWindow()->parent();
    system_info_widget_ = nullptr;
  }

  void OnSplitViewModeEnter() override {}

  void OnSplitViewModeExit() override {}

  scoped_ptr<OrientationController> orientation_controller_;
  scoped_ptr<ShutdownDialog> shutdown_dialog_;
  scoped_ptr<BackgroundController> background_controller_;

  // The parent container for the background.
  aura::Window* background_container_;

  views::Widget* system_info_widget_;

  DISALLOW_COPY_AND_ASSIGN(SystemUIImpl);
};

}  // namespace

// static
SystemUI* SystemUI::Create(
    scoped_refptr<base::TaskRunner> blocking_task_runner) {
  SystemUIImpl* system_ui = new SystemUIImpl(blocking_task_runner);
  instance = system_ui;
  system_ui->Init();
  return instance;
}

// static
SystemUI* SystemUI::Get() {
  DCHECK(instance);
  return instance;
}

// static
void SystemUI::Shutdown() {
  CHECK(instance);
  delete instance;
  instance = nullptr;
}

}  // namespace athena
