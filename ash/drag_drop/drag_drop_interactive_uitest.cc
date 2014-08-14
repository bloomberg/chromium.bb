// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/drag_drop_controller.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gl/gl_surface.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

class DraggableView : public views::View {
 public:
  DraggableView() {}
  virtual ~DraggableView() {}

  // views::View overrides:
  virtual int GetDragOperations(const gfx::Point& press_pt) OVERRIDE {
    return ui::DragDropTypes::DRAG_MOVE;
  }
  virtual void WriteDragData(const gfx::Point& press_pt,
                             OSExchangeData* data)OVERRIDE {
    data->SetString(base::UTF8ToUTF16("test"));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DraggableView);
};

class TargetView : public views::View {
 public:
  TargetView() : dropped_(false) {}
  virtual ~TargetView() {}

  // views::View overrides:
  virtual bool GetDropFormats(
      int* formats,
      std::set<OSExchangeData::CustomFormat>* custom_formats) OVERRIDE {
    *formats = ui::OSExchangeData::STRING;
    return true;
  }
  virtual bool AreDropTypesRequired() OVERRIDE {
    return false;
  }
  virtual bool CanDrop(const OSExchangeData& data) OVERRIDE {
    return true;
  }
  virtual int OnDragUpdated(const ui::DropTargetEvent& event) OVERRIDE {
    return ui::DragDropTypes::DRAG_MOVE;
  }
  virtual int OnPerformDrop(const ui::DropTargetEvent& event) OVERRIDE {
    dropped_ = true;
    return ui::DragDropTypes::DRAG_MOVE;
  }

  bool dropped() const { return dropped_; }

 private:
  bool dropped_;

  DISALLOW_COPY_AND_ASSIGN(TargetView);
};

views::Widget* CreateWidget(views::View* contents_view,
                            const gfx::Rect& bounds) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.accept_events = true;
  params.context = Shell::GetPrimaryRootWindow();
  params.bounds = bounds;
  widget->Init(params);

  widget->SetContentsView(contents_view);
  widget->Show();
  return widget;
}

void QuitLoop() {
  base::MessageLoop::current()->Quit();
}

void DragDropAcrossMultiDisplay_Step4() {
  ui_controls::SendMouseEventsNotifyWhenDone(
      ui_controls::LEFT, ui_controls::UP,
      base::Bind(&QuitLoop));
}

void DragDropAcrossMultiDisplay_Step3() {
  // Move to the edge of the 1st display so that the mouse
  // is moved to 2nd display by ash.
  ui_controls::SendMouseMoveNotifyWhenDone(
      399, 10,
      base::Bind(&DragDropAcrossMultiDisplay_Step4));
}

void DragDropAcrossMultiDisplay_Step2() {
  ui_controls::SendMouseMoveNotifyWhenDone(
      20, 10,
      base::Bind(&DragDropAcrossMultiDisplay_Step3));
}

void DragDropAcrossMultiDisplay_Step1() {
  ui_controls::SendMouseEventsNotifyWhenDone(
      ui_controls::LEFT, ui_controls::DOWN,
      base::Bind(&DragDropAcrossMultiDisplay_Step2));
}

}  // namespace

class DragDropTest : public test::AshTestBase {
 public:
  DragDropTest() {}
  virtual ~DragDropTest() {}

  virtual void SetUp() OVERRIDE {
    gfx::GLSurface::InitializeOneOffForTests();

    ui::RegisterPathProvider();
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        "en-US", NULL, ui::ResourceBundle::LOAD_COMMON_RESOURCES);
    base::FilePath resources_pack_path;
    PathService::Get(base::DIR_MODULE, &resources_pack_path);
    resources_pack_path =
        resources_pack_path.Append(FILE_PATH_LITERAL("resources.pak"));
    ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        resources_pack_path, ui::SCALE_FACTOR_NONE);

    test::AshTestBase::SetUp();
  }
};

#if !defined(OS_CHROMEOS)
#define MAYBE_DragDropAcrossMultiDisplay DISABLED_DragDropAcrossMultiDisplay
#else
#define MAYBE_DragDropAcrossMultiDisplay DragDropAcrossMultiDisplay
#endif

// Test if the mouse gets moved properly to another display
// during drag & drop operation.
TEST_F(DragDropTest, MAYBE_DragDropAcrossMultiDisplay) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("400x400,400x400");
  aura::Window::Windows root_windows =
      Shell::GetInstance()->GetAllRootWindows();
  views::View* draggable_view = new DraggableView();
  draggable_view->set_drag_controller(NULL);
  draggable_view->SetBounds(0, 0, 100, 100);
  views::Widget* source =
      CreateWidget(draggable_view, gfx::Rect(0, 0, 100, 100));

  TargetView* target_view = new TargetView();
  target_view->SetBounds(0, 0, 100, 100);
  views::Widget* target =
      CreateWidget(target_view, gfx::Rect(400, 0, 100, 100));

  // Make sure they're on the different root windows.
  EXPECT_EQ(root_windows[0], source->GetNativeView()->GetRootWindow());
  EXPECT_EQ(root_windows[1], target->GetNativeView()->GetRootWindow());

  ui_controls::SendMouseMoveNotifyWhenDone(
      10, 10, base::Bind(&DragDropAcrossMultiDisplay_Step1));

  base::MessageLoop::current()->Run();

  EXPECT_TRUE(target_view->dropped());

  source->Close();
  target->Close();
}

}  // namespace ash
