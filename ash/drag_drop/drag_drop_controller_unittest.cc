// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/drag_drop_controller.h"

#include "ash/drag_drop/drag_image_view.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/utf_string_conversions.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/event_generator.h"
#include "ui/base/animation/linear_animation.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drag_utils.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/events/event.h"
#include "ui/base/events/event_utils.h"
#include "ui/base/gestures/gesture_types.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/view.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/native_widget_delegate.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace test {

namespace {

// A simple view that makes sure RunShellDrag is invoked on mouse drag.
class DragTestView : public views::View {
 public:
  DragTestView() : views::View() {
    Reset();
  }

  void Reset() {
    num_drag_enters_ = 0;
    num_drag_exits_ = 0;
    num_drag_updates_ = 0;
    num_drops_ = 0;
    drag_done_received_ = false;
    long_tap_received_ = false;
  }

  int VerticalDragThreshold() {
    return views::View::GetVerticalDragThreshold();
  }

  int HorizontalDragThreshold() {
    return views::View::GetHorizontalDragThreshold();
  }

  int num_drag_enters_;
  int num_drag_exits_;
  int num_drag_updates_;
  int num_drops_;
  bool drag_done_received_;
  bool long_tap_received_;

 private:
  // View overrides:
  int GetDragOperations(const gfx::Point& press_pt) OVERRIDE {
    return ui::DragDropTypes::DRAG_COPY;
  }

  virtual void WriteDragData(const gfx::Point& p,
                             OSExchangeData* data) OVERRIDE {
    data->SetString(UTF8ToUTF16("I am being dragged"));
    gfx::ImageSkiaRep image_rep(gfx::Size(10, 20), ui::SCALE_FACTOR_100P);
    gfx::ImageSkia image_skia(image_rep);

    drag_utils::SetDragImageOnDataObject(
        image_skia, image_skia.size(), gfx::Vector2d(), data);
  }

  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE {
    return true;
  }

  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
    if (event->type() == ui::ET_GESTURE_LONG_TAP)
      long_tap_received_ = true;
    return;
  }

  virtual bool GetDropFormats(
      int* formats,
      std::set<OSExchangeData::CustomFormat>* custom_formats) {
    *formats = ui::OSExchangeData::STRING;
    return true;
  }

  virtual bool CanDrop(const OSExchangeData& data) OVERRIDE {
    return true;
  }

  virtual void OnDragEntered(const ui::DropTargetEvent& event) OVERRIDE {
    num_drag_enters_++;
  }

  virtual int OnDragUpdated(const ui::DropTargetEvent& event) OVERRIDE {
    num_drag_updates_++;
    return ui::DragDropTypes::DRAG_COPY;
  }

  virtual void OnDragExited() OVERRIDE {
    num_drag_exits_++;
  }

  virtual int OnPerformDrop(const ui::DropTargetEvent& event) OVERRIDE {
    num_drops_++;
    return ui::DragDropTypes::DRAG_COPY;
  }

  virtual void OnDragDone() OVERRIDE {
    drag_done_received_ = true;
  }

  DISALLOW_COPY_AND_ASSIGN(DragTestView);
};

class CompletableLinearAnimation : public ui::LinearAnimation {
 public:
  CompletableLinearAnimation(int duration,
                             int frame_rate,
                             ui::AnimationDelegate* delegate)
      : ui::LinearAnimation(duration, frame_rate, delegate),
        duration_(duration) {
  }

  void Complete() {
    Step(start_time() + base::TimeDelta::FromMilliseconds(duration_));
  }

 private:
  int duration_;
};

class TestDragDropController : public internal::DragDropController {
 public:
  TestDragDropController() : internal::DragDropController() {
    Reset();
  }

  void Reset() {
    drag_start_received_ = false;
    num_drag_updates_ = 0;
    drop_received_ = false;
    drag_canceled_ = false;
    drag_string_.clear();
  }

  int StartDragAndDrop(const ui::OSExchangeData& data,
                       aura::RootWindow* root_window,
                       aura::Window* source_window,
                       const gfx::Point& location,
                       int operation,
                       ui::DragDropTypes::DragEventSource source) OVERRIDE {
    drag_start_received_ = true;
    data.GetString(&drag_string_);
    return DragDropController::StartDragAndDrop(
        data, root_window, source_window, location, operation, source);
  }

  void DragUpdate(aura::Window* target,
                  const ui::LocatedEvent& event) OVERRIDE {
    DragDropController::DragUpdate(target, event);
    num_drag_updates_++;
  }

  void Drop(aura::Window* target, const ui::LocatedEvent& event) OVERRIDE {
    DragDropController::Drop(target, event);
    drop_received_ = true;
  }

  void DragCancel() OVERRIDE {
    DragDropController::DragCancel();
    drag_canceled_ = true;
  }

  ui::LinearAnimation* CreateCancelAnimation(
      int duration,
      int frame_rate,
      ui::AnimationDelegate* delegate) OVERRIDE {
    return new CompletableLinearAnimation(duration, frame_rate, delegate);
  }

  void DoDragCancel(int animation_duration_ms) OVERRIDE {
    DragDropController::DoDragCancel(animation_duration_ms);
    drag_canceled_ = true;
  }

  bool drag_start_received_;
  int num_drag_updates_;
  bool drop_received_;
  bool drag_canceled_;
  string16 drag_string_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestDragDropController);
};

class TestNativeWidgetAura : public views::NativeWidgetAura {
 public:
  explicit TestNativeWidgetAura(views::internal::NativeWidgetDelegate* delegate)
      : NativeWidgetAura(delegate),
        check_if_capture_lost_(false) {
  }

  void set_check_if_capture_lost(bool value) {
    check_if_capture_lost_ = value;
  }

  virtual void OnCaptureLost() OVERRIDE {
    DCHECK(!check_if_capture_lost_);
    views::NativeWidgetAura::OnCaptureLost();
  }

 private:
  bool check_if_capture_lost_;

  DISALLOW_COPY_AND_ASSIGN(TestNativeWidgetAura);
};

// TODO(sky): this is for debugging, remove when track down failure.
void SetCheckIfCaptureLost(views::Widget* widget, bool value) {
  // On Windows, the DCHECK triggers when running on bot or locally through RDP,
  // but not when logged in locally.
#if !defined(OS_WIN)
  static_cast<TestNativeWidgetAura*>(widget->native_widget())->
      set_check_if_capture_lost(value);
#endif
}

views::Widget* CreateNewWidget() {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.accept_events = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = Shell::GetPrimaryRootWindow();
  params.child = true;
  params.native_widget = new TestNativeWidgetAura(widget);
  widget->Init(params);
  widget->Show();
  return widget;
}

void AddViewToWidgetAndResize(views::Widget* widget, views::View* view) {
  if (!widget->GetContentsView()) {
    views::View* contents_view = new views::View;
    widget->SetContentsView(contents_view);
  }

  views::View* contents_view = widget->GetContentsView();
  contents_view->AddChildView(view);
  view->SetBounds(contents_view->width(), 0, 100, 100);
  gfx::Rect contents_view_bounds = contents_view->bounds();
  contents_view_bounds.Union(view->bounds());
  contents_view->SetBoundsRect(contents_view_bounds);
  widget->SetBounds(contents_view_bounds);
}

void DispatchGesture(ui::EventType gesture_type, gfx::Point location) {
  ui::GestureEvent gesture_event(
      gesture_type,
      location.x(),
      location.y(),
      0,
      ui::EventTimeForNow(),
      ui::GestureEventDetails(gesture_type, 0, 0),
      1);
  Shell::GetPrimaryRootWindow()->DispatchGestureEvent(&gesture_event);
}

}  // namespace

class DragDropControllerTest : public AshTestBase {
 public:
  DragDropControllerTest() : AshTestBase() {}
  virtual ~DragDropControllerTest() {}

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();
    drag_drop_controller_.reset(new TestDragDropController);
    drag_drop_controller_->set_should_block_during_drag_drop(false);
    aura::client::SetDragDropClient(Shell::GetPrimaryRootWindow(),
                                    drag_drop_controller_.get());
    views_delegate_.reset(new views::TestViewsDelegate);
  }

  virtual void TearDown() OVERRIDE {
    aura::client::SetDragDropClient(Shell::GetPrimaryRootWindow(), NULL);
    drag_drop_controller_.reset();
    AshTestBase::TearDown();
  }

  void UpdateDragData(ui::OSExchangeData* data) {
    drag_drop_controller_->drag_data_ = data;
  }

  aura::Window* GetDragWindow() {
    return drag_drop_controller_->drag_window_;
  }

  aura::Window* GetDragSourceWindow() {
    return drag_drop_controller_->drag_source_window_;
  }

  void SetDragSourceWindow(aura::Window* drag_source_window) {
    drag_drop_controller_->drag_source_window_ = drag_source_window;
    drag_source_window->AddObserver(drag_drop_controller_.get());
  }

  aura::Window* GetDragImageWindow() {
    return drag_drop_controller_->drag_image_.get() ?
        drag_drop_controller_->drag_image_->GetWidget()->GetNativeWindow() :
        NULL;
  }

  void CompleteCancelAnimation() {
    CompletableLinearAnimation* animation =
        static_cast<CompletableLinearAnimation*>(
            drag_drop_controller_->cancel_animation_.get());
    animation->Complete();
  }

 protected:
  scoped_ptr<TestDragDropController> drag_drop_controller_;
  scoped_ptr<views::TestViewsDelegate> views_delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DragDropControllerTest);
};

TEST_F(DragDropControllerTest, DragDropInSingleViewTest) {
  scoped_ptr<views::Widget> widget(CreateNewWidget());
  DragTestView* drag_view = new DragTestView;
  AddViewToWidgetAndResize(widget.get(), drag_view);
  ui::OSExchangeData data;
  data.SetString(UTF8ToUTF16("I am being dragged"));
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       widget->GetNativeView());
  generator.PressLeftButton();

  int num_drags = 17;
  SetCheckIfCaptureLost(widget.get(), true);
  for (int i = 0; i < num_drags; ++i) {
    // Because we are not doing a blocking drag and drop, the original
    // OSDragExchangeData object is lost as soon as we return from the drag
    // initiation in DragDropController::StartDragAndDrop(). Hence we set the
    // drag_data_ to a fake drag data object that we created.
    if (i > 0)
      UpdateDragData(&data);
    // 7 comes from views::View::GetVerticalDragThreshold()).
    if (i >= 7)
      SetCheckIfCaptureLost(widget.get(), false);

    generator.MoveMouseBy(0, 1);

    // Execute any scheduled draws to process deferred mouse events.
    RunAllPendingInMessageLoop();
  }

  generator.ReleaseLeftButton();

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_EQ(num_drags - 1 - drag_view->VerticalDragThreshold(),
      drag_drop_controller_->num_drag_updates_);
  EXPECT_TRUE(drag_drop_controller_->drop_received_);
  EXPECT_EQ(UTF8ToUTF16("I am being dragged"),
      drag_drop_controller_->drag_string_);

  EXPECT_EQ(1, drag_view->num_drag_enters_);
  EXPECT_EQ(num_drags - 1 - drag_view->VerticalDragThreshold(),
      drag_view->num_drag_updates_);
  EXPECT_EQ(1, drag_view->num_drops_);
  EXPECT_EQ(0, drag_view->num_drag_exits_);
  EXPECT_TRUE(drag_view->drag_done_received_);
}

TEST_F(DragDropControllerTest, DragDropWithZeroDragUpdates) {
  scoped_ptr<views::Widget> widget(CreateNewWidget());
  DragTestView* drag_view = new DragTestView;
  AddViewToWidgetAndResize(widget.get(), drag_view);
  ui::OSExchangeData data;
  data.SetString(UTF8ToUTF16("I am being dragged"));
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       widget->GetNativeView());
  generator.PressLeftButton();

  int num_drags = drag_view->VerticalDragThreshold() + 1;
  for (int i = 0; i < num_drags; ++i) {
    // Because we are not doing a blocking drag and drop, the original
    // OSDragExchangeData object is lost as soon as we return from the drag
    // initiation in DragDropController::StartDragAndDrop(). Hence we set the
    // drag_data_ to a fake drag data object that we created.
    if (i > 0)
      UpdateDragData(&data);
    generator.MoveMouseBy(0, 1);
  }

  UpdateDragData(&data);

  generator.ReleaseLeftButton();

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_EQ(num_drags - 1 - drag_view->VerticalDragThreshold() + 1,
      drag_drop_controller_->num_drag_updates_);
  EXPECT_TRUE(drag_drop_controller_->drop_received_);

  EXPECT_EQ(1, drag_view->num_drag_enters_);
  EXPECT_EQ(num_drags - 1 - drag_view->VerticalDragThreshold() + 1,
      drag_view->num_drag_updates_);
  EXPECT_EQ(1, drag_view->num_drops_);
  EXPECT_EQ(0, drag_view->num_drag_exits_);
  EXPECT_TRUE(drag_view->drag_done_received_);
}

TEST_F(DragDropControllerTest, DragDropInMultipleViewsSingleWidgetTest) {
  scoped_ptr<views::Widget> widget(CreateNewWidget());
  DragTestView* drag_view1 = new DragTestView;
  AddViewToWidgetAndResize(widget.get(), drag_view1);
  DragTestView* drag_view2 = new DragTestView;
  AddViewToWidgetAndResize(widget.get(), drag_view2);

  ui::OSExchangeData data;
  data.SetString(UTF8ToUTF16("I am being dragged"));

  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.MoveMouseRelativeTo(widget->GetNativeView(),
                                drag_view1->bounds().CenterPoint());
  generator.PressLeftButton();

  int num_drags = drag_view1->width();
  for (int i = 0; i < num_drags; ++i) {
    // Because we are not doing a blocking drag and drop, the original
    // OSDragExchangeData object is lost as soon as we return from the drag
    // initiation in DragDropController::StartDragAndDrop(). Hence we set the
    // drag_data_ to a fake drag data object that we created.
    if (i > 0)
      UpdateDragData(&data);
    generator.MoveMouseBy(1, 0);

    // Execute any scheduled draws to process deferred mouse events.
    RunAllPendingInMessageLoop();
  }

  generator.ReleaseLeftButton();

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_EQ(num_drags - 1 - drag_view1->HorizontalDragThreshold(),
      drag_drop_controller_->num_drag_updates_);
  EXPECT_TRUE(drag_drop_controller_->drop_received_);
  EXPECT_EQ(UTF8ToUTF16("I am being dragged"),
      drag_drop_controller_->drag_string_);

  EXPECT_EQ(1, drag_view1->num_drag_enters_);
  int num_expected_updates = drag_view1->bounds().width() -
      drag_view1->bounds().CenterPoint().x() - 2;
  EXPECT_EQ(num_expected_updates - drag_view1->HorizontalDragThreshold(),
      drag_view1->num_drag_updates_);
  EXPECT_EQ(0, drag_view1->num_drops_);
  EXPECT_EQ(1, drag_view1->num_drag_exits_);
  EXPECT_TRUE(drag_view1->drag_done_received_);

  EXPECT_EQ(1, drag_view2->num_drag_enters_);
  num_expected_updates = num_drags - num_expected_updates - 1;
  EXPECT_EQ(num_expected_updates, drag_view2->num_drag_updates_);
  EXPECT_EQ(1, drag_view2->num_drops_);
  EXPECT_EQ(0, drag_view2->num_drag_exits_);
  EXPECT_FALSE(drag_view2->drag_done_received_);
}

TEST_F(DragDropControllerTest, DragDropInMultipleViewsMultipleWidgetsTest) {
  scoped_ptr<views::Widget> widget1(CreateNewWidget());
  DragTestView* drag_view1 = new DragTestView;
  AddViewToWidgetAndResize(widget1.get(), drag_view1);
  scoped_ptr<views::Widget> widget2(CreateNewWidget());
  DragTestView* drag_view2 = new DragTestView;
  AddViewToWidgetAndResize(widget2.get(), drag_view2);
  gfx::Rect widget1_bounds = widget1->GetClientAreaBoundsInScreen();
  gfx::Rect widget2_bounds = widget2->GetClientAreaBoundsInScreen();
  widget2->SetBounds(gfx::Rect(widget1_bounds.width(), 0,
      widget2_bounds.width(), widget2_bounds.height()));

  ui::OSExchangeData data;
  data.SetString(UTF8ToUTF16("I am being dragged"));

  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       widget1->GetNativeView());
  generator.PressLeftButton();

  int num_drags = drag_view1->width();
  for (int i = 0; i < num_drags; ++i) {
    // Because we are not doing a blocking drag and drop, the original
    // OSDragExchangeData object is lost as soon as we return from the drag
    // initiation in DragDropController::StartDragAndDrop(). Hence we set the
    // drag_data_ to a fake drag data object that we created.
    if (i > 0)
      UpdateDragData(&data);
    generator.MoveMouseBy(1, 0);

    // Execute any scheduled draws to process deferred mouse events.
    RunAllPendingInMessageLoop();
  }

  generator.ReleaseLeftButton();

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_EQ(num_drags - 1 - drag_view1->HorizontalDragThreshold(),
      drag_drop_controller_->num_drag_updates_);
  EXPECT_TRUE(drag_drop_controller_->drop_received_);
  EXPECT_EQ(UTF8ToUTF16("I am being dragged"),
      drag_drop_controller_->drag_string_);

  EXPECT_EQ(1, drag_view1->num_drag_enters_);
  int num_expected_updates = drag_view1->bounds().width() -
      drag_view1->bounds().CenterPoint().x() - 2;
  EXPECT_EQ(num_expected_updates - drag_view1->HorizontalDragThreshold(),
      drag_view1->num_drag_updates_);
  EXPECT_EQ(0, drag_view1->num_drops_);
  EXPECT_EQ(1, drag_view1->num_drag_exits_);
  EXPECT_TRUE(drag_view1->drag_done_received_);

  EXPECT_EQ(1, drag_view2->num_drag_enters_);
  num_expected_updates = num_drags - num_expected_updates - 1;
  EXPECT_EQ(num_expected_updates, drag_view2->num_drag_updates_);
  EXPECT_EQ(1, drag_view2->num_drops_);
  EXPECT_EQ(0, drag_view2->num_drag_exits_);
  EXPECT_FALSE(drag_view2->drag_done_received_);
}

TEST_F(DragDropControllerTest, ViewRemovedWhileInDragDropTest) {
  scoped_ptr<views::Widget> widget(CreateNewWidget());
  scoped_ptr<DragTestView> drag_view(new DragTestView);
  AddViewToWidgetAndResize(widget.get(), drag_view.get());
  gfx::Point point = gfx::Rect(drag_view->bounds()).CenterPoint();
  ui::OSExchangeData data;
  data.SetString(UTF8ToUTF16("I am being dragged"));

  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.MoveMouseToCenterOf(widget->GetNativeView());
  generator.PressLeftButton();

  int num_drags_1 = 17;
  for (int i = 0; i < num_drags_1; ++i) {
    // Because we are not doing a blocking drag and drop, the original
    // OSDragExchangeData object is lost as soon as we return from the drag
    // initiation in DragDropController::StartDragAndDrop(). Hence we set the
    // drag_data_ to a fake drag data object that we created.
    if (i > 0)
      UpdateDragData(&data);
    generator.MoveMouseBy(0, 1);

    // Execute any scheduled draws to process deferred mouse events.
    RunAllPendingInMessageLoop();
  }

  drag_view->parent()->RemoveChildView(drag_view.get());
  // View has been removed. We will not get any of the following drag updates.
  int num_drags_2 = 23;
  for (int i = 0; i < num_drags_2; ++i) {
    UpdateDragData(&data);
    generator.MoveMouseBy(0, 1);

    // Execute any scheduled draws to process deferred mouse events.
    RunAllPendingInMessageLoop();
  }

  generator.ReleaseLeftButton();

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_EQ(num_drags_1 + num_drags_2 - 1 - drag_view->VerticalDragThreshold(),
      drag_drop_controller_->num_drag_updates_);
  EXPECT_TRUE(drag_drop_controller_->drop_received_);
  EXPECT_EQ(UTF8ToUTF16("I am being dragged"),
      drag_drop_controller_->drag_string_);

  EXPECT_EQ(1, drag_view->num_drag_enters_);
  EXPECT_EQ(num_drags_1 - 1 - drag_view->VerticalDragThreshold(),
      drag_view->num_drag_updates_);
  EXPECT_EQ(0, drag_view->num_drops_);
  EXPECT_EQ(0, drag_view->num_drag_exits_);
  EXPECT_TRUE(drag_view->drag_done_received_);
}

TEST_F(DragDropControllerTest, DragLeavesClipboardAloneTest) {
  ui::Clipboard* cb = ui::Clipboard::GetForCurrentThread();
  std::string clip_str("I am on the clipboard");
  {
    // We first copy some text to the clipboard.
    ui::ScopedClipboardWriter scw(cb, ui::Clipboard::BUFFER_STANDARD);
    scw.WriteText(ASCIIToUTF16(clip_str));
  }
  EXPECT_TRUE(cb->IsFormatAvailable(ui::Clipboard::GetPlainTextFormatType(),
      ui::Clipboard::BUFFER_STANDARD));

  scoped_ptr<views::Widget> widget(CreateNewWidget());
  DragTestView* drag_view = new DragTestView;
  AddViewToWidgetAndResize(widget.get(), drag_view);

  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       widget->GetNativeView());
  ui::OSExchangeData data;
  std::string data_str("I am being dragged");
  data.SetString(ASCIIToUTF16(data_str));

  generator.PressLeftButton();
  generator.MoveMouseBy(0, drag_view->VerticalDragThreshold() + 1);

  // Execute any scheduled draws to process deferred mouse events.
  RunAllPendingInMessageLoop();

  // Verify the clipboard contents haven't changed
  std::string result;
  EXPECT_TRUE(cb->IsFormatAvailable(ui::Clipboard::GetPlainTextFormatType(),
      ui::Clipboard::BUFFER_STANDARD));
  cb->ReadAsciiText(ui::Clipboard::BUFFER_STANDARD, &result);
  EXPECT_EQ(clip_str, result);
  // Destory the clipboard here because ash doesn't delete it.
  // crbug.com/158150.
  ui::Clipboard::DestroyClipboardForCurrentThread();
}

TEST_F(DragDropControllerTest, WindowDestroyedDuringDragDrop) {
  scoped_ptr<views::Widget> widget(CreateNewWidget());
  DragTestView* drag_view = new DragTestView;
  AddViewToWidgetAndResize(widget.get(), drag_view);
  aura::Window* window = widget->GetNativeView();

  ui::OSExchangeData data;
  data.SetString(UTF8ToUTF16("I am being dragged"));
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       widget->GetNativeView());
  generator.PressLeftButton();

  int num_drags = 17;
  for (int i = 0; i < num_drags; ++i) {
    // Because we are not doing a blocking drag and drop, the original
    // OSDragExchangeData object is lost as soon as we return from the drag
    // initiation in DragDropController::StartDragAndDrop(). Hence we set the
    // drag_data_ to a fake drag data object that we created.
    if (i > 0)
      UpdateDragData(&data);
    generator.MoveMouseBy(0, 1);

    // Execute any scheduled draws to process deferred mouse events.
    RunAllPendingInMessageLoop();

    if (i > drag_view->VerticalDragThreshold())
      EXPECT_EQ(window, GetDragWindow());
  }

  widget->CloseNow();
  EXPECT_FALSE(GetDragWindow());

  num_drags = 23;
  for (int i = 0; i < num_drags; ++i) {
    if (i > 0)
      UpdateDragData(&data);
    generator.MoveMouseBy(0, 1);
    // We should not crash here.
  }

  generator.ReleaseLeftButton();

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_TRUE(drag_drop_controller_->drop_received_);
}

TEST_F(DragDropControllerTest, SyntheticEventsDuringDragDrop) {
  scoped_ptr<views::Widget> widget(CreateNewWidget());
  DragTestView* drag_view = new DragTestView;
  AddViewToWidgetAndResize(widget.get(), drag_view);
  ui::OSExchangeData data;
  data.SetString(UTF8ToUTF16("I am being dragged"));
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       widget->GetNativeView());
  generator.PressLeftButton();

  int num_drags = 17;
  for (int i = 0; i < num_drags; ++i) {
    // Because we are not doing a blocking drag and drop, the original
    // OSDragExchangeData object is lost as soon as we return from the drag
    // initiation in DragDropController::StartDragAndDrop(). Hence we set the
    // drag_data_ to a fake drag data object that we created.
    if (i > 0)
      UpdateDragData(&data);
    generator.MoveMouseBy(0, 1);

    // We send a unexpected mouse move event. Note that we cannot use
    // EventGenerator since it implicitly turns these into mouse drag events.
    // The DragDropController should simply ignore these events.
    gfx::Point mouse_move_location = drag_view->bounds().CenterPoint();
    ui::MouseEvent mouse_move(ui::ET_MOUSE_MOVED, mouse_move_location,
                              mouse_move_location, 0);
    Shell::GetPrimaryRootWindow()->AsRootWindowHostDelegate()->OnHostMouseEvent(
        &mouse_move);
  }

  generator.ReleaseLeftButton();

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_EQ(num_drags - 1 - drag_view->VerticalDragThreshold(),
      drag_drop_controller_->num_drag_updates_);
  EXPECT_TRUE(drag_drop_controller_->drop_received_);
  EXPECT_EQ(UTF8ToUTF16("I am being dragged"),
      drag_drop_controller_->drag_string_);

  EXPECT_EQ(1, drag_view->num_drag_enters_);
  EXPECT_EQ(num_drags - 1 - drag_view->VerticalDragThreshold(),
      drag_view->num_drag_updates_);
  EXPECT_EQ(1, drag_view->num_drops_);
  EXPECT_EQ(0, drag_view->num_drag_exits_);
  EXPECT_TRUE(drag_view->drag_done_received_);
}

TEST_F(DragDropControllerTest, PressingEscapeCancelsDragDrop) {
  scoped_ptr<views::Widget> widget(CreateNewWidget());
  DragTestView* drag_view = new DragTestView;
  AddViewToWidgetAndResize(widget.get(), drag_view);
  ui::OSExchangeData data;
  data.SetString(UTF8ToUTF16("I am being dragged"));
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       widget->GetNativeView());
  generator.PressLeftButton();

  int num_drags = 17;
  for (int i = 0; i < num_drags; ++i) {
    // Because we are not doing a blocking drag and drop, the original
    // OSDragExchangeData object is lost as soon as we return from the drag
    // initiation in DragDropController::StartDragAndDrop(). Hence we set the
    // drag_data_ to a fake drag data object that we created.
    if (i > 0)
      UpdateDragData(&data);
    generator.MoveMouseBy(0, 1);

    // Execute any scheduled draws to process deferred mouse events.
    RunAllPendingInMessageLoop();
  }

  generator.PressKey(ui::VKEY_ESCAPE, 0);

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_EQ(num_drags - 1 - drag_view->VerticalDragThreshold(),
      drag_drop_controller_->num_drag_updates_);
  EXPECT_FALSE(drag_drop_controller_->drop_received_);
  EXPECT_TRUE(drag_drop_controller_->drag_canceled_);
  EXPECT_EQ(UTF8ToUTF16("I am being dragged"),
      drag_drop_controller_->drag_string_);

  EXPECT_EQ(1, drag_view->num_drag_enters_);
  EXPECT_EQ(num_drags - 1 - drag_view->VerticalDragThreshold(),
      drag_view->num_drag_updates_);
  EXPECT_EQ(0, drag_view->num_drops_);
  EXPECT_EQ(1, drag_view->num_drag_exits_);
  EXPECT_TRUE(drag_view->drag_done_received_);
}

TEST_F(DragDropControllerTest, TouchDragDropInMultipleWindows) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableTouchDragDrop);
  scoped_ptr<views::Widget> widget1(CreateNewWidget());
  DragTestView* drag_view1 = new DragTestView;
  AddViewToWidgetAndResize(widget1.get(), drag_view1);
  scoped_ptr<views::Widget> widget2(CreateNewWidget());
  DragTestView* drag_view2 = new DragTestView;
  AddViewToWidgetAndResize(widget2.get(), drag_view2);
  gfx::Rect widget1_bounds = widget1->GetClientAreaBoundsInScreen();
  gfx::Rect widget2_bounds = widget2->GetClientAreaBoundsInScreen();
  widget2->SetBounds(gfx::Rect(widget1_bounds.width(), 0,
      widget2_bounds.width(), widget2_bounds.height()));

  ui::OSExchangeData data;
  data.SetString(UTF8ToUTF16("I am being dragged"));

  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       widget1->GetNativeView());
  generator.PressTouch();
  gfx::Point point = gfx::Rect(drag_view1->bounds()).CenterPoint();
  DispatchGesture(ui::ET_GESTURE_LONG_PRESS, point);
  // Because we are not doing a blocking drag and drop, the original
  // OSDragExchangeData object is lost as soon as we return from the drag
  // initiation in DragDropController::StartDragAndDrop(). Hence we set the
  // drag_data_ to a fake drag data object that we created.
  UpdateDragData(&data);
  gfx::Point gesture_location = point;
  int num_drags = drag_view1->width();
  for (int i = 0; i < num_drags; ++i) {
    gesture_location.Offset(1, 0);
    DispatchGesture(ui::ET_GESTURE_SCROLL_UPDATE, gesture_location);

    // Execute any scheduled draws to process deferred mouse events.
    RunAllPendingInMessageLoop();
  }

  DispatchGesture(ui::ET_GESTURE_SCROLL_END, gesture_location);

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_EQ(num_drags, drag_drop_controller_->num_drag_updates_);
  EXPECT_TRUE(drag_drop_controller_->drop_received_);
  EXPECT_EQ(UTF8ToUTF16("I am being dragged"),
      drag_drop_controller_->drag_string_);

  EXPECT_EQ(1, drag_view1->num_drag_enters_);
  int num_expected_updates = drag_view1->bounds().width() -
      drag_view1->bounds().CenterPoint().x() - 1;
  EXPECT_EQ(num_expected_updates, drag_view1->num_drag_updates_);
  EXPECT_EQ(0, drag_view1->num_drops_);
  EXPECT_EQ(1, drag_view1->num_drag_exits_);
  EXPECT_TRUE(drag_view1->drag_done_received_);

  EXPECT_EQ(1, drag_view2->num_drag_enters_);
  num_expected_updates = num_drags - num_expected_updates;
  EXPECT_EQ(num_expected_updates, drag_view2->num_drag_updates_);
  EXPECT_EQ(1, drag_view2->num_drops_);
  EXPECT_EQ(0, drag_view2->num_drag_exits_);
  EXPECT_FALSE(drag_view2->drag_done_received_);
}

TEST_F(DragDropControllerTest, TouchDragDropCancelsOnLongTap) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableTouchDragDrop);
  scoped_ptr<views::Widget> widget(CreateNewWidget());
  DragTestView* drag_view = new DragTestView;
  AddViewToWidgetAndResize(widget.get(), drag_view);
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       widget->GetNativeView());

  generator.PressTouch();
  gfx::Point point = gfx::Rect(drag_view->bounds()).CenterPoint();
  DispatchGesture(ui::ET_GESTURE_LONG_PRESS, point);
  DispatchGesture(ui::ET_GESTURE_LONG_TAP, point);

  EXPECT_TRUE(drag_drop_controller_->drag_start_received_);
  EXPECT_TRUE(drag_drop_controller_->drag_canceled_);
  EXPECT_EQ(0, drag_drop_controller_->num_drag_updates_);
  EXPECT_FALSE(drag_drop_controller_->drop_received_);
  EXPECT_EQ(UTF8ToUTF16("I am being dragged"),
            drag_drop_controller_->drag_string_);
  EXPECT_EQ(0, drag_view->num_drag_enters_);
  EXPECT_EQ(0, drag_view->num_drops_);
  EXPECT_EQ(0, drag_view->num_drag_exits_);
  EXPECT_TRUE(drag_view->drag_done_received_);
}

TEST_F(DragDropControllerTest, TouchDragDropLongTapGestureIsForwarded) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableTouchDragDrop);
  scoped_ptr<views::Widget> widget(CreateNewWidget());
  DragTestView* drag_view = new DragTestView;
  AddViewToWidgetAndResize(widget.get(), drag_view);
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                       widget->GetNativeView());

  generator.PressTouch();
  gfx::Point point = gfx::Rect(drag_view->bounds()).CenterPoint();
  DispatchGesture(ui::ET_GESTURE_LONG_PRESS, point);

  // Since we are not running inside a nested loop, the |drag_source_window_|
  // will get destroyed immediately. Hence we reassign it.
  EXPECT_EQ(NULL, GetDragSourceWindow());
  SetDragSourceWindow(widget->GetNativeView());
  EXPECT_FALSE(drag_view->long_tap_received_);
  DispatchGesture(ui::ET_GESTURE_LONG_TAP, point);
  CompleteCancelAnimation();
  EXPECT_TRUE(drag_view->long_tap_received_);
}

namespace {

class DragImageWindowObserver : public aura::WindowObserver {
 public:
  virtual void OnWindowDestroying(aura::Window* window) {
    window_location_on_destroying_ = window->GetBoundsInScreen().origin();
  }

  gfx::Point window_location_on_destroying() const {
    return window_location_on_destroying_;
  }

 public:
  gfx::Point window_location_on_destroying_;
};

}

// Verifies the drag image moves back to the position where drag is started
// across displays when drag is cancelled.
TEST_F(DragDropControllerTest, DragCancelAcrossDisplays) {
  UpdateDisplay("400x400,400x400");
  Shell::RootWindowList root_windows =
      Shell::GetInstance()->GetAllRootWindows();
  for (Shell::RootWindowList::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    aura::client::SetDragDropClient(*iter, drag_drop_controller_.get());
  }

  ui::OSExchangeData data;
  data.SetString(UTF8ToUTF16("I am being dragged"));
  {
    scoped_ptr<views::Widget> widget(CreateNewWidget());
    aura::Window* window = widget->GetNativeWindow();
    drag_drop_controller_->StartDragAndDrop(
        data,
        window->GetRootWindow(),
        window,
        gfx::Point(5, 5),
        ui::DragDropTypes::DRAG_MOVE,
        ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE);

    DragImageWindowObserver observer;
    ASSERT_TRUE(GetDragImageWindow());
    GetDragImageWindow()->AddObserver(&observer);

    {
      ui::MouseEvent e(ui::ET_MOUSE_DRAGGED,
                       gfx::Point(200, 0),
                       gfx::Point(200, 0),
                       ui::EF_NONE);
      drag_drop_controller_->DragUpdate(window, e);
    }
    {
      ui::MouseEvent e(ui::ET_MOUSE_DRAGGED,
                       gfx::Point(600, 0),
                       gfx::Point(600, 0),
                       ui::EF_NONE);
      drag_drop_controller_->DragUpdate(window, e);
    }

    drag_drop_controller_->DragCancel();
    CompleteCancelAnimation();

    EXPECT_EQ("5,5", observer.window_location_on_destroying().ToString());
  }

  {
    scoped_ptr<views::Widget> widget(CreateNewWidget());
    aura::Window* window = widget->GetNativeWindow();
    drag_drop_controller_->StartDragAndDrop(
        data,
        window->GetRootWindow(),
        window,
        gfx::Point(405, 405),
        ui::DragDropTypes::DRAG_MOVE,
        ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE);
    DragImageWindowObserver observer;
    ASSERT_TRUE(GetDragImageWindow());
    GetDragImageWindow()->AddObserver(&observer);

    {
      ui::MouseEvent e(ui::ET_MOUSE_DRAGGED,
                       gfx::Point(600, 0),
                       gfx::Point(600, 0),
                       ui::EF_NONE);
      drag_drop_controller_->DragUpdate(window, e);
    }
    {
      ui::MouseEvent e(ui::ET_MOUSE_DRAGGED,
                       gfx::Point(200, 0),
                       gfx::Point(200, 0),
                       ui::EF_NONE);
      drag_drop_controller_->DragUpdate(window, e);
    }

    drag_drop_controller_->DragCancel();
    CompleteCancelAnimation();

    EXPECT_EQ("405,405", observer.window_location_on_destroying().ToString());
  }
  for (Shell::RootWindowList::iterator iter = root_windows.begin();
       iter != root_windows.end(); ++iter) {
    aura::client::SetDragDropClient(*iter, NULL);
  }
}

}  // namespace test
}  // namespace aura
