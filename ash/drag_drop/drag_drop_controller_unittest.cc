// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/drag_drop_controller.h"

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/location.h"
#include "base/utf_string_conversions.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"
#include "ui/aura/shared/root_window_event_filter.h"
#include "ui/aura/test/event_generator.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/views/events/event.h"
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

 private:
  // View overrides:
  int GetDragOperations(const gfx::Point& press_pt) OVERRIDE {
    return ui::DragDropTypes::DRAG_COPY;
  }

  void WriteDragData(const gfx::Point& p, OSExchangeData* data) OVERRIDE {
    data->SetString(UTF8ToUTF16("I am being dragged"));
  }

  bool OnMousePressed(const views::MouseEvent& event) OVERRIDE {
    return true;
  }

  bool GetDropFormats(int* formats,
                      std::set<OSExchangeData::CustomFormat>* custom_formats) {
    *formats = ui::OSExchangeData::STRING;
    return true;
  }

  bool CanDrop(const OSExchangeData& data) OVERRIDE {
    return true;
  }

  void OnDragEntered(const views::DropTargetEvent& event) OVERRIDE {
    num_drag_enters_++;
  }

  int OnDragUpdated(const views::DropTargetEvent& event) OVERRIDE {
    num_drag_updates_++;
    return ui::DragDropTypes::DRAG_COPY;
  }

  void OnDragExited() OVERRIDE {
    num_drag_exits_++;
  }

  int OnPerformDrop(const views::DropTargetEvent& event) OVERRIDE {
    num_drops_++;
    return ui::DragDropTypes::DRAG_COPY;
  }

  void OnDragDone() OVERRIDE {
    drag_done_received_ = true;
  }

  DISALLOW_COPY_AND_ASSIGN(DragTestView);
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
    drag_string_.clear();
  }

  bool drag_start_received_;
  int num_drag_updates_;
  bool drop_received_;
  string16 drag_string_;

 private:
  int StartDragAndDrop(const ui::OSExchangeData& data,
                       const gfx::Point& location,
                       int operation) OVERRIDE {
    drag_start_received_ = true;
    data.GetString(&drag_string_);
    return DragDropController::StartDragAndDrop(data, location, operation);
  }

  void DragUpdate(aura::Window* target,
                  const aura::LocatedEvent& event) OVERRIDE {
    DragDropController::DragUpdate(target, event);
    num_drag_updates_++;
  }

  void Drop(aura::Window* target, const aura::LocatedEvent& event) OVERRIDE {
    DragDropController::Drop(target, event);
    drop_received_ = true;
  }

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
  static_cast<TestNativeWidgetAura*>(widget->native_widget())->
      set_check_if_capture_lost(value);
}

views::Widget* CreateNewWidget() {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.accept_events = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = Shell::GetRootWindow();
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
  contents_view_bounds = contents_view_bounds.Union(view->bounds());
  contents_view->SetBoundsRect(contents_view_bounds);
  widget->SetBounds(contents_view_bounds);
}

}  // namespace

class DragDropControllerTest : public AshTestBase {
 public:
  DragDropControllerTest() : AshTestBase() {}
  virtual ~DragDropControllerTest() {}

  void SetUp() OVERRIDE {
    AshTestBase::SetUp();
    drag_drop_controller_.reset(new TestDragDropController);
    drag_drop_controller_->set_should_block_during_drag_drop(false);
    aura::client::SetDragDropClient(Shell::GetRootWindow(),
                                    drag_drop_controller_.get());
    views_delegate_.reset(new views::TestViewsDelegate);
  }

  void TearDown() OVERRIDE {
    aura::client::SetDragDropClient(Shell::GetRootWindow(), NULL);
    drag_drop_controller_.reset();
    AshTestBase::TearDown();
  }

  void UpdateDragData(ui::OSExchangeData* data) {
    drag_drop_controller_->drag_data_ = data;
  }

  aura::Window* GetDragWindow() {
    return drag_drop_controller_->drag_window_;
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
  aura::test::EventGenerator generator(Shell::GetRootWindow(),
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
  aura::test::EventGenerator generator(Shell::GetRootWindow(),
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

  aura::test::EventGenerator generator(Shell::GetRootWindow());
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
  gfx::Rect widget1_bounds = widget1->GetClientAreaScreenBounds();
  gfx::Rect widget2_bounds = widget2->GetClientAreaScreenBounds();
  widget2->SetBounds(gfx::Rect(widget1_bounds.width(), 0,
      widget2_bounds.width(), widget2_bounds.height()));

  ui::OSExchangeData data;
  data.SetString(UTF8ToUTF16("I am being dragged"));

  aura::test::EventGenerator generator(Shell::GetRootWindow(),
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

  aura::test::EventGenerator generator(Shell::GetRootWindow());
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
  ui::Clipboard* cb = views::ViewsDelegate::views_delegate->GetClipboard();
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

  aura::test::EventGenerator generator(Shell::GetRootWindow(),
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
}

TEST_F(DragDropControllerTest, WindowDestroyedDuringDragDrop) {
  scoped_ptr<views::Widget> widget(CreateNewWidget());
  DragTestView* drag_view = new DragTestView;
  AddViewToWidgetAndResize(widget.get(), drag_view);
  aura::Window* window = widget->GetNativeView();

  ui::OSExchangeData data;
  data.SetString(UTF8ToUTF16("I am being dragged"));
  aura::test::EventGenerator generator(Shell::GetRootWindow(),
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
  aura::test::EventGenerator generator(Shell::GetRootWindow(),
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
    aura::MouseEvent mouse_move(ui::ET_MOUSE_MOVED,
                                mouse_move_location, mouse_move_location, 0);
    Shell::GetRootWindow()->DispatchMouseEvent(&mouse_move);
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

}  // namespace test
}  // namespace aura
