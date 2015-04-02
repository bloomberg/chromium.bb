// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/window_manager/focus_controller.h"

#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/services/window_manager/basic_focus_rules.h"
#include "mojo/services/window_manager/capture_controller.h"
#include "mojo/services/window_manager/focus_controller_observer.h"
#include "mojo/services/window_manager/view_event_dispatcher.h"
#include "mojo/services/window_manager/view_targeter.h"
#include "mojo/services/window_manager/window_manager_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/geometry/rect.h"

using mojo::View;

namespace window_manager {

// Counts the number of events that occur.
class FocusNotificationObserver : public FocusControllerObserver {
 public:
  FocusNotificationObserver()
      : activation_changed_count_(0),
        focus_changed_count_(0),
        reactivation_count_(0),
        reactivation_requested_view_(NULL),
        reactivation_actual_view_(NULL) {}
  ~FocusNotificationObserver() override {}

  void ExpectCounts(int activation_changed_count, int focus_changed_count) {
    EXPECT_EQ(activation_changed_count, activation_changed_count_);
    EXPECT_EQ(focus_changed_count, focus_changed_count_);
  }
  int reactivation_count() const { return reactivation_count_; }
  View* reactivation_requested_view() const {
    return reactivation_requested_view_;
  }
  View* reactivation_actual_view() const {
    return reactivation_actual_view_;
  }

 protected:
  // Overridden from FocusControllerObserver:
  void OnActivated(View* gained_active) override {
    ++activation_changed_count_;
  }

  void OnFocused(View* gained_focus) override { ++focus_changed_count_; }

  void OnAttemptToReactivateView(View* request_active,
                                 View* actual_active) override {
    ++reactivation_count_;
    reactivation_requested_view_ = request_active;
    reactivation_actual_view_ = actual_active;
  }

 private:
  int activation_changed_count_;
  int focus_changed_count_;
  int reactivation_count_;
  View* reactivation_requested_view_;
  View* reactivation_actual_view_;

  DISALLOW_COPY_AND_ASSIGN(FocusNotificationObserver);
};

class ViewDestroyer {
 public:
  virtual View* GetDestroyedView() = 0;

 protected:
  virtual ~ViewDestroyer() {}
};

// FocusNotificationObserver that keeps track of whether it was notified about
// activation changes or focus changes with a destroyed view.
class RecordingFocusNotificationObserver : public FocusNotificationObserver {
 public:
  RecordingFocusNotificationObserver(FocusController* focus_controller,
                                     ViewDestroyer* destroyer)
      : focus_controller_(focus_controller),
        destroyer_(destroyer),
        active_(nullptr),
        focus_(nullptr),
        was_notified_with_destroyed_view_(false) {
    focus_controller_->AddObserver(this);
  }
  ~RecordingFocusNotificationObserver() override {
    focus_controller_->RemoveObserver(this);
  }

  bool was_notified_with_destroyed_view() const {
    return was_notified_with_destroyed_view_;
  }

  // Overridden from FocusNotificationObserver:
  void OnActivated(View* gained_active) override {
    if (active_ && active_ == destroyer_->GetDestroyedView())
      was_notified_with_destroyed_view_ = true;
    active_ = gained_active;
  }

  void OnFocused(View* gained_focus) override {
    if (focus_ && focus_ == destroyer_->GetDestroyedView())
      was_notified_with_destroyed_view_ = true;
    focus_ = gained_focus;
  }

 private:
  FocusController* focus_controller_;

  // Not owned.
  ViewDestroyer* destroyer_;
  View* active_;
  View* focus_;

  // Whether the observer was notified about the loss of activation or the
  // loss of focus with a view already destroyed by |destroyer_| as the
  // |lost_active| or |lost_focus| parameter.
  bool was_notified_with_destroyed_view_;

  DISALLOW_COPY_AND_ASSIGN(RecordingFocusNotificationObserver);
};

class DestroyOnLoseActivationFocusNotificationObserver
    : public FocusNotificationObserver,
      public ViewDestroyer {
 public:
  DestroyOnLoseActivationFocusNotificationObserver(
      FocusController* focus_controller,
      View* view_to_destroy,
      View* initial_active)
      : focus_controller_(focus_controller),
        view_to_destroy_(view_to_destroy),
        active_(initial_active),
        did_destroy_(false) {
    focus_controller_->AddObserver(this);
  }
  ~DestroyOnLoseActivationFocusNotificationObserver() override {
    focus_controller_->RemoveObserver(this);
  }

  // Overridden from FocusNotificationObserver:
  void OnActivated(View* gained_active) override {
    if (view_to_destroy_ && active_ == view_to_destroy_) {
      view_to_destroy_->Destroy();
      did_destroy_ = true;
    }
    active_ = gained_active;
  }

  // Overridden from ViewDestroyer:
  View* GetDestroyedView() override {
    return did_destroy_ ? view_to_destroy_ : nullptr;
  }

 private:
  FocusController* focus_controller_;
  View* view_to_destroy_;
  View* active_;
  bool did_destroy_;

  DISALLOW_COPY_AND_ASSIGN(DestroyOnLoseActivationFocusNotificationObserver);
};

class ScopedFocusNotificationObserver : public FocusNotificationObserver {
 public:
  ScopedFocusNotificationObserver(FocusController* focus_controller)
      : focus_controller_(focus_controller) {
    focus_controller_->AddObserver(this);
  }
  ~ScopedFocusNotificationObserver() override {
    focus_controller_->RemoveObserver(this);
  }

 private:
  FocusController* focus_controller_;

  DISALLOW_COPY_AND_ASSIGN(ScopedFocusNotificationObserver);
};

// Only responds to events if a message contains |target| as a parameter.
class ScopedFilteringFocusNotificationObserver
    : public FocusNotificationObserver {
 public:
  ScopedFilteringFocusNotificationObserver(FocusController* focus_controller,
                                           View* target,
                                           View* initial_active,
                                           View* initial_focus)
      : focus_controller_(focus_controller),
        target_(target),
        active_(initial_active),
        focus_(initial_focus) {
    focus_controller_->AddObserver(this);
  }
  ~ScopedFilteringFocusNotificationObserver() override {
    focus_controller_->RemoveObserver(this);
  }

 private:
  // Overridden from FocusControllerObserver:
  void OnActivated(View* gained_active) override {
    if (gained_active == target_ || active_ == target_)
      FocusNotificationObserver::OnActivated(gained_active);
    active_ = gained_active;
  }

  void OnFocused(View* gained_focus) override {
    if (gained_focus == target_ || focus_ == target_)
      FocusNotificationObserver::OnFocused(gained_focus);
    focus_ = gained_focus;
  }

  void OnAttemptToReactivateView(View* request_active,
                                 View* actual_active) override {
    if (request_active == target_ || actual_active == target_) {
      FocusNotificationObserver::OnAttemptToReactivateView(request_active,
                                                           actual_active);
    }
  }

  FocusController* focus_controller_;
  View* target_;
  View* active_;
  View* focus_;

  DISALLOW_COPY_AND_ASSIGN(ScopedFilteringFocusNotificationObserver);
};

// Used to fake the handling of events in the pre-target phase.
class SimpleEventHandler : public ui::EventHandler {
 public:
  SimpleEventHandler() {}
  ~SimpleEventHandler() override {}

  // Overridden from ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    event->SetHandled();
  }
  void OnGestureEvent(ui::GestureEvent* event) override {
    event->SetHandled();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SimpleEventHandler);
};

class FocusShiftingActivationObserver
    : public FocusControllerObserver {
 public:
  explicit FocusShiftingActivationObserver(FocusController* focus_controller,
                                           View* activated_view)
      : focus_controller_(focus_controller),
        activated_view_(activated_view),
        shift_focus_to_(NULL) {}
  ~FocusShiftingActivationObserver() override {}

  void set_shift_focus_to(View* shift_focus_to) {
    shift_focus_to_ = shift_focus_to;
  }

 private:
  // Overridden from FocusControllerObserver:
  void OnActivated(View* gained_active) override {
    // Shift focus to a child. This should prevent the default focusing from
    // occurring in FocusController::FocusView().
    if (gained_active == activated_view_)
      focus_controller_->FocusView(shift_focus_to_);
  }

  void OnFocused(View* gained_focus) override {}

  FocusController* focus_controller_;
  View* activated_view_;
  View* shift_focus_to_;

  DISALLOW_COPY_AND_ASSIGN(FocusShiftingActivationObserver);
};

// BasicFocusRules subclass that allows basic overrides of focus/activation to
// be tested. This is intended more as a test that the override system works at
// all, rather than as an exhaustive set of use cases, those should be covered
// in tests for those FocusRules implementations.
class TestFocusRules : public BasicFocusRules {
 public:
  TestFocusRules(View* root)
      : BasicFocusRules(root), focus_restriction_(NULL) {}

  // Restricts focus and activation to this view and its child hierarchy.
  void set_focus_restriction(View* focus_restriction) {
    focus_restriction_ = focus_restriction;
  }

  // Overridden from BasicFocusRules:
  bool SupportsChildActivation(View* view) const override {
    // In FocusControllerTests, only the Root has activatable children.
    return view->GetRoot() == view;
  }
  bool CanActivateView(View* view) const override {
    // Restricting focus to a non-activatable child view means the activatable
    // parent outside the focus restriction is activatable.
    bool can_activate =
        CanFocusOrActivate(view) || view->Contains(focus_restriction_);
    return can_activate ? BasicFocusRules::CanActivateView(view) : false;
  }
  bool CanFocusView(View* view) const override {
    return CanFocusOrActivate(view) ? BasicFocusRules::CanFocusView(view)
                                    : false;
  }
  View* GetActivatableView(View* view) const override {
    return BasicFocusRules::GetActivatableView(
        CanFocusOrActivate(view) ? view : focus_restriction_);
  }
  View* GetFocusableView(View* view) const override {
    return BasicFocusRules::GetFocusableView(
        CanFocusOrActivate(view) ? view : focus_restriction_);
  }
  View* GetNextActivatableView(View* ignore) const override {
    View* next_activatable = BasicFocusRules::GetNextActivatableView(ignore);
    return CanFocusOrActivate(next_activatable)
               ? next_activatable
               : GetActivatableView(focus_restriction_);
  }

 private:
  bool CanFocusOrActivate(View* view) const {
    return !focus_restriction_ || focus_restriction_->Contains(view);
  }

  View* focus_restriction_;

  DISALLOW_COPY_AND_ASSIGN(TestFocusRules);
};

// Common infrastructure shared by all FocusController test types.
class FocusControllerTestBase : public testing::Test {
 protected:
  // Hierarchy used by all tests:
  // root_view
  //       +-- w1
  //       |    +-- w11
  //       |    +-- w12
  //       +-- w2
  //       |    +-- w21
  //       |         +-- w211
  //       +-- w3
  FocusControllerTestBase()
      : root_view_(TestView::Build(0, gfx::Rect(0, 0, 800, 600))),
        v1(TestView::Build(1, gfx::Rect(0, 0, 50, 50), root_view())),
        v11(TestView::Build(11, gfx::Rect(5, 5, 10, 10), v1)),
        v12(TestView::Build(12, gfx::Rect(15, 15, 10, 10), v1)),
        v2(TestView::Build(2, gfx::Rect(75, 75, 50, 50), root_view())),
        v21(TestView::Build(21, gfx::Rect(5, 5, 10, 10), v2)),
        v211(TestView::Build(211, gfx::Rect(1, 1, 5, 5), v21)),
        v3(TestView::Build(3, gfx::Rect(125, 125, 50, 50), root_view())) {}

  // Overridden from testing::Test:
  void SetUp() override {
    testing::Test::SetUp();

    test_focus_rules_ = new TestFocusRules(root_view());
    focus_controller_.reset(
        new FocusController(scoped_ptr<FocusRules>(test_focus_rules_)));
    SetFocusController(root_view(), focus_controller_.get());

    capture_controller_.reset(new CaptureController);
    SetCaptureController(root_view(), capture_controller_.get());

    ViewTarget* root_target = root_view_->target();
    root_target->SetEventTargeter(scoped_ptr<ViewTargeter>(new ViewTargeter()));
    view_event_dispatcher_.reset(new ViewEventDispatcher());
    view_event_dispatcher_->SetRootViewTarget(root_target);

    GetRootViewTarget()->AddPreTargetHandler(focus_controller_.get());
  }

  void TearDown() override {
    GetRootViewTarget()->RemovePreTargetHandler(focus_controller_.get());
    view_event_dispatcher_.reset();

    root_view_->Destroy();

    capture_controller_.reset();
    test_focus_rules_ = nullptr;  // Owned by FocusController.
    focus_controller_.reset();

    testing::Test::TearDown();
  }

  void FocusView(View* view) { focus_controller_->FocusView(view); }
  View* GetFocusedView() { return focus_controller_->GetFocusedView(); }
  int GetFocusedViewId() {
    View* focused_view = GetFocusedView();
    return focused_view ? focused_view->id() : -1;
  }
  void ActivateView(View* view) { focus_controller_->ActivateView(view); }
  void DeactivateView(View* view) { focus_controller_->DeactivateView(view); }
  View* GetActiveView() { return focus_controller_->GetActiveView(); }
  int GetActiveViewId() {
    View* active_view = GetActiveView();
    return active_view ? active_view->id() : -1;
  }

  View* GetViewById(int id) { return root_view_->GetChildById(id); }

  void ClickLeftButton(View* view) {
    // Get the center bounds of |target| in |root_view_| coordinate space.
    gfx::Point center =
        gfx::Rect(view->bounds().To<gfx::Rect>().size()).CenterPoint();
    ViewTarget::ConvertPointToTarget(ViewTarget::TargetFromView(view),
                                     root_view_->target(), &center);

    ui::MouseEvent button_down(ui::ET_MOUSE_PRESSED, center, center,
                               ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_NONE);
    ui::EventDispatchDetails details =
        view_event_dispatcher_->OnEventFromSource(&button_down);
    CHECK(!details.dispatcher_destroyed);

    ui::MouseEvent button_up(ui::ET_MOUSE_RELEASED, center, center,
                             ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_NONE);
    details = view_event_dispatcher_->OnEventFromSource(&button_up);
    CHECK(!details.dispatcher_destroyed);
  }

  ViewTarget* GetRootViewTarget() {
    return ViewTarget::TargetFromView(root_view());
  }

  View* root_view() { return root_view_; }
  TestFocusRules* test_focus_rules() { return test_focus_rules_; }
  FocusController* focus_controller() { return focus_controller_.get(); }
  CaptureController* capture_controller() { return capture_controller_.get(); }

  // Test functions.
  virtual void BasicFocus() = 0;
  virtual void BasicActivation() = 0;
  virtual void FocusEvents() = 0;
  virtual void DuplicateFocusEvents() {}
  virtual void ActivationEvents() = 0;
  virtual void ReactivationEvents() {}
  virtual void DuplicateActivationEvents() {}
  virtual void ShiftFocusWithinActiveView() {}
  virtual void ShiftFocusToChildOfInactiveView() {}
  virtual void ShiftFocusToParentOfFocusedView() {}
  virtual void FocusRulesOverride() = 0;
  virtual void ActivationRulesOverride() = 0;
  virtual void ShiftFocusOnActivation() {}
  virtual void ShiftFocusOnActivationDueToHide() {}
  virtual void NoShiftActiveOnActivation() {}
  virtual void ChangeFocusWhenNothingFocusedAndCaptured() {}
  virtual void DontPassDestroyedView() {}
  // TODO(erg): Also, void FocusedTextInputClient() once we build the IME.

 private:
  TestView* root_view_;
  scoped_ptr<FocusController> focus_controller_;
  TestFocusRules* test_focus_rules_;
  scoped_ptr<CaptureController> capture_controller_;
  // TODO(erg): The aura version of this class also keeps track of WMState. Do
  // we need something analogous here?

  scoped_ptr<ViewEventDispatcher> view_event_dispatcher_;

  TestView* v1;
  TestView* v11;
  TestView* v12;
  TestView* v2;
  TestView* v21;
  TestView* v211;
  TestView* v3;

  DISALLOW_COPY_AND_ASSIGN(FocusControllerTestBase);
};

// Test base for tests where focus is directly set to a target view.
class FocusControllerDirectTestBase : public FocusControllerTestBase {
 protected:
  FocusControllerDirectTestBase() {}

  // Different test types shift focus in different ways.
  virtual void FocusViewDirect(View* view) = 0;
  virtual void ActivateViewDirect(View* view) = 0;
  virtual void DeactivateViewDirect(View* view) = 0;

  // Input events do not change focus if the view can not be focused.
  virtual bool IsInputEvent() = 0;

  void FocusViewById(int id) {
    View* view = root_view()->GetChildById(id);
    DCHECK(view);
    FocusViewDirect(view);
  }
  void ActivateViewById(int id) {
    View* view = root_view()->GetChildById(id);
    DCHECK(view);
    ActivateViewDirect(view);
  }

  // Overridden from FocusControllerTestBase:
  void BasicFocus() override {
    EXPECT_EQ(nullptr, GetFocusedView());
    FocusViewById(1);
    EXPECT_EQ(1, GetFocusedViewId());
    FocusViewById(2);
    EXPECT_EQ(2, GetFocusedViewId());
  }
  void BasicActivation() override {
    EXPECT_EQ(nullptr, GetActiveView());
    ActivateViewById(1);
    EXPECT_EQ(1, GetActiveViewId());
    ActivateViewById(2);
    EXPECT_EQ(2, GetActiveViewId());
    // Verify that attempting to deactivate NULL does not crash and does not
    // change activation.
    DeactivateView(nullptr);
    EXPECT_EQ(2, GetActiveViewId());
    DeactivateView(GetActiveView());
    EXPECT_EQ(1, GetActiveViewId());
  }
  void FocusEvents() override {
    ScopedFocusNotificationObserver root_observer(focus_controller());
    ScopedFilteringFocusNotificationObserver observer1(
        focus_controller(), GetViewById(1), GetActiveView(), GetFocusedView());
    ScopedFilteringFocusNotificationObserver observer2(
        focus_controller(), GetViewById(2), GetActiveView(), GetFocusedView());

    {
      SCOPED_TRACE("initial state");
      root_observer.ExpectCounts(0, 0);
      observer1.ExpectCounts(0, 0);
      observer2.ExpectCounts(0, 0);
    }

    FocusViewById(1);
    {
      SCOPED_TRACE("FocusViewById(1)");
      root_observer.ExpectCounts(1, 1);
      observer1.ExpectCounts(1, 1);
      observer2.ExpectCounts(0, 0);
    }

    FocusViewById(2);
    {
      SCOPED_TRACE("FocusViewById(2)");
      root_observer.ExpectCounts(2, 2);
      observer1.ExpectCounts(2, 2);
      observer2.ExpectCounts(1, 1);
    }
  }
  void DuplicateFocusEvents() override {
    // Focusing an existing focused view should not resend focus events.
    ScopedFocusNotificationObserver root_observer(focus_controller());
    ScopedFilteringFocusNotificationObserver observer1(
        focus_controller(), GetViewById(1), GetActiveView(), GetFocusedView());

    root_observer.ExpectCounts(0, 0);
    observer1.ExpectCounts(0, 0);

    FocusViewById(1);
    root_observer.ExpectCounts(1, 1);
    observer1.ExpectCounts(1, 1);

    FocusViewById(1);
    root_observer.ExpectCounts(1, 1);
    observer1.ExpectCounts(1, 1);
  }
  void ActivationEvents() override {
    ActivateViewById(1);

    ScopedFocusNotificationObserver root_observer(focus_controller());
    ScopedFilteringFocusNotificationObserver observer1(
        focus_controller(), GetViewById(1), GetActiveView(), GetFocusedView());
    ScopedFilteringFocusNotificationObserver observer2(
        focus_controller(), GetViewById(2), GetActiveView(), GetFocusedView());

    root_observer.ExpectCounts(0, 0);
    observer1.ExpectCounts(0, 0);
    observer2.ExpectCounts(0, 0);

    ActivateViewById(2);
    root_observer.ExpectCounts(1, 1);
    observer1.ExpectCounts(1, 1);
    observer2.ExpectCounts(1, 1);
  }
  void ReactivationEvents() override {
    ActivateViewById(1);
    ScopedFocusNotificationObserver root_observer(focus_controller());
    EXPECT_EQ(0, root_observer.reactivation_count());
    GetViewById(2)->SetVisible(false);
    // When we attempt to activate "2", which cannot be activated because it
    // is not visible, "1" will be reactivated.
    ActivateViewById(2);
    EXPECT_EQ(1, root_observer.reactivation_count());
    EXPECT_EQ(GetViewById(2),
              root_observer.reactivation_requested_view());
    EXPECT_EQ(GetViewById(1),
              root_observer.reactivation_actual_view());
  }
  void DuplicateActivationEvents() override {
    ActivateViewById(1);

    ScopedFocusNotificationObserver root_observer(focus_controller());
    ScopedFilteringFocusNotificationObserver observer1(
        focus_controller(), GetViewById(1), GetActiveView(), GetFocusedView());
    ScopedFilteringFocusNotificationObserver observer2(
        focus_controller(), GetViewById(2), GetActiveView(), GetFocusedView());

    root_observer.ExpectCounts(0, 0);
    observer1.ExpectCounts(0, 0);
    observer2.ExpectCounts(0, 0);

    ActivateViewById(2);
    root_observer.ExpectCounts(1, 1);
    observer1.ExpectCounts(1, 1);
    observer2.ExpectCounts(1, 1);

    // Activating an existing active view should not resend activation events.
    ActivateViewById(2);
    root_observer.ExpectCounts(1, 1);
    observer1.ExpectCounts(1, 1);
    observer2.ExpectCounts(1, 1);
  }
  void ShiftFocusWithinActiveView() override {
    ActivateViewById(1);
    EXPECT_EQ(1, GetActiveViewId());
    EXPECT_EQ(1, GetFocusedViewId());
    FocusViewById(11);
    EXPECT_EQ(11, GetFocusedViewId());
    FocusViewById(12);
    EXPECT_EQ(12, GetFocusedViewId());
  }
  void ShiftFocusToChildOfInactiveView() override {
    ActivateViewById(2);
    EXPECT_EQ(2, GetActiveViewId());
    EXPECT_EQ(2, GetFocusedViewId());
    FocusViewById(11);
    EXPECT_EQ(1, GetActiveViewId());
    EXPECT_EQ(11, GetFocusedViewId());
  }
  void ShiftFocusToParentOfFocusedView() override {
    ActivateViewById(1);
    EXPECT_EQ(1, GetFocusedViewId());
    FocusViewById(11);
    EXPECT_EQ(11, GetFocusedViewId());
    FocusViewById(1);
    // Focus should _not_ shift to the parent of the already-focused view.
    EXPECT_EQ(11, GetFocusedViewId());
  }
  void FocusRulesOverride() override {
    EXPECT_EQ(NULL, GetFocusedView());
    FocusViewById(11);
    EXPECT_EQ(11, GetFocusedViewId());

    test_focus_rules()->set_focus_restriction(GetViewById(211));
    FocusViewById(12);
    // Input events leave focus unchanged; direct API calls will change focus
    // to the restricted view.
    int focused_view = IsInputEvent() ? 11 : 211;
    EXPECT_EQ(focused_view, GetFocusedViewId());

    test_focus_rules()->set_focus_restriction(NULL);
    FocusViewById(12);
    EXPECT_EQ(12, GetFocusedViewId());
  }
  void ActivationRulesOverride() override {
    ActivateViewById(1);
    EXPECT_EQ(1, GetActiveViewId());
    EXPECT_EQ(1, GetFocusedViewId());

    View* v3 = GetViewById(3);
    test_focus_rules()->set_focus_restriction(v3);

    ActivateViewById(2);
    // Input events leave activation unchanged; direct API calls will activate
    // the restricted view.
    int active_view = IsInputEvent() ? 1 : 3;
    EXPECT_EQ(active_view, GetActiveViewId());
    EXPECT_EQ(active_view, GetFocusedViewId());

    test_focus_rules()->set_focus_restriction(NULL);
    ActivateViewById(2);
    EXPECT_EQ(2, GetActiveViewId());
    EXPECT_EQ(2, GetFocusedViewId());
  }
  void ShiftFocusOnActivation() override {
    // When a view is activated, by default that view is also focused.
    // An ActivationChangeObserver may shift focus to another view within the
    // same activatable view.
    ActivateViewById(2);
    EXPECT_EQ(2, GetFocusedViewId());
    ActivateViewById(1);
    EXPECT_EQ(1, GetFocusedViewId());

    ActivateViewById(2);

    View* target = GetViewById(1);

    scoped_ptr<FocusShiftingActivationObserver> observer(
        new FocusShiftingActivationObserver(focus_controller(), target));
    observer->set_shift_focus_to(target->GetChildById(11));
    focus_controller()->AddObserver(observer.get());

    ActivateViewById(1);

    // w1's ActivationChangeObserver shifted focus to this child, pre-empting
    // FocusController's default setting.
    EXPECT_EQ(11, GetFocusedViewId());

    ActivateViewById(2);
    EXPECT_EQ(2, GetFocusedViewId());

    // Simulate a focus reset by the ActivationChangeObserver. This should
    // trigger the default setting in FocusController.
    observer->set_shift_focus_to(nullptr);
    ActivateViewById(1);
    EXPECT_EQ(1, GetFocusedViewId());

    focus_controller()->RemoveObserver(observer.get());

    ActivateViewById(2);
    EXPECT_EQ(2, GetFocusedViewId());
    ActivateViewById(1);
    EXPECT_EQ(1, GetFocusedViewId());
  }
  void ShiftFocusOnActivationDueToHide() override {
    // Similar to ShiftFocusOnActivation except the activation change is
    // triggered by hiding the active view.
    ActivateViewById(1);
    EXPECT_EQ(1, GetFocusedViewId());

    // Removes view 3 as candidate for next activatable view.
    root_view()->GetChildById(3)->SetVisible(false);
    EXPECT_EQ(1, GetFocusedViewId());

    View* target = root_view()->GetChildById(2);

    scoped_ptr<FocusShiftingActivationObserver> observer(
        new FocusShiftingActivationObserver(focus_controller(), target));
    observer->set_shift_focus_to(target->GetChildById(21));
    focus_controller()->AddObserver(observer.get());

    // Hide the active view.
    root_view()->GetChildById(1)->SetVisible(false);

    EXPECT_EQ(21, GetFocusedViewId());

    focus_controller()->RemoveObserver(observer.get());
  }
  void NoShiftActiveOnActivation() override {
    // When a view is activated, we need to prevent any change to activation
    // from being made in response to an activation change notification.
  }

  // Verifies focus change is honored while capture held.
  void ChangeFocusWhenNothingFocusedAndCaptured() override {
    View* v1 = root_view()->GetChildById(1);
    capture_controller()->SetCapture(v1);

    EXPECT_EQ(-1, GetActiveViewId());
    EXPECT_EQ(-1, GetFocusedViewId());

    FocusViewById(1);

    EXPECT_EQ(1, GetActiveViewId());
    EXPECT_EQ(1, GetFocusedViewId());

    capture_controller()->ReleaseCapture(v1);
  }

  // Verifies if a view that loses activation or focus is destroyed during
  // observer notification we don't pass the destroyed view to other observers.
  void DontPassDestroyedView() override {
    FocusViewById(1);

    EXPECT_EQ(1, GetActiveViewId());
    EXPECT_EQ(1, GetFocusedViewId());

    {
      View* to_destroy = root_view()->GetChildById(1);
      DestroyOnLoseActivationFocusNotificationObserver observer1(
          focus_controller(), to_destroy, GetActiveView());
      RecordingFocusNotificationObserver observer2(focus_controller(),
                                                   &observer1);

      FocusViewById(2);

      EXPECT_EQ(2, GetActiveViewId());
      EXPECT_EQ(2, GetFocusedViewId());

      EXPECT_EQ(to_destroy, observer1.GetDestroyedView());
      EXPECT_FALSE(observer2.was_notified_with_destroyed_view());
    }

    {
      View* to_destroy = root_view()->GetChildById(2);
      DestroyOnLoseActivationFocusNotificationObserver observer1(
          focus_controller(), to_destroy, GetActiveView());
      RecordingFocusNotificationObserver observer2(focus_controller(),
                                                   &observer1);

      FocusViewById(3);

      EXPECT_EQ(3, GetActiveViewId());
      EXPECT_EQ(3, GetFocusedViewId());

      EXPECT_EQ(to_destroy, observer1.GetDestroyedView());
      EXPECT_FALSE(observer2.was_notified_with_destroyed_view());
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FocusControllerDirectTestBase);
};

// Focus and Activation changes via the FocusController API.
class FocusControllerApiTest : public FocusControllerDirectTestBase {
 public:
  FocusControllerApiTest() {}

 private:
  // Overridden from FocusControllerTestBase:
  void FocusViewDirect(View* view) override { FocusView(view); }
  void ActivateViewDirect(View* view) override { ActivateView(view); }
  void DeactivateViewDirect(View* view) override { DeactivateView(view); }
  bool IsInputEvent() override { return false; }

  DISALLOW_COPY_AND_ASSIGN(FocusControllerApiTest);
};

// Focus and Activation changes via input events.
class FocusControllerMouseEventTest : public FocusControllerDirectTestBase {
 public:
  FocusControllerMouseEventTest() {}

  // Tests that a handled mouse event does not trigger a view activation.
  void IgnoreHandledEvent() {
    EXPECT_EQ(NULL, GetActiveView());
    View* v1 = root_view()->GetChildById(1);
    SimpleEventHandler handler;
    GetRootViewTarget()->PrependPreTargetHandler(&handler);
    ClickLeftButton(v1);
    EXPECT_EQ(NULL, GetActiveView());
    // TODO(erg): Add gesture testing when we get gestures working.
    GetRootViewTarget()->RemovePreTargetHandler(&handler);
    ClickLeftButton(v1);
    EXPECT_EQ(1, GetActiveViewId());
  }

 private:
  // Overridden from FocusControllerTestBase:
  void FocusViewDirect(View* view) override { ClickLeftButton(view); }
  void ActivateViewDirect(View* view) override { ClickLeftButton(view); }
  void DeactivateViewDirect(View* view) override {
    View* next_activatable = test_focus_rules()->GetNextActivatableView(view);
    ClickLeftButton(next_activatable);
  }
  bool IsInputEvent() override { return true; }

  DISALLOW_COPY_AND_ASSIGN(FocusControllerMouseEventTest);
};

// TODO(erg): Add a FocusControllerGestureEventTest once we have working
// gesture forwarding and handling.

// Test base for tests where focus is implicitly set to a window as the result
// of a disposition change to the focused window or the hierarchy that contains
// it.
class FocusControllerImplicitTestBase : public FocusControllerTestBase {
 protected:
  explicit FocusControllerImplicitTestBase(bool parent) : parent_(parent) {}

  View* GetDispositionView(View* view) {
    return parent_ ? view->parent() : view;
  }

  // Change the disposition of |view| in such a way as it will lose focus.
  virtual void ChangeViewDisposition(View* view) = 0;

  // Allow each disposition change test to add additional post-disposition
  // change expectations.
  virtual void PostDispostionChangeExpectations() {}

  // Overridden from FocusControllerTestBase:
  void BasicFocus() override {
    EXPECT_EQ(NULL, GetFocusedView());

    View* w211 = root_view()->GetChildById(211);
    FocusView(w211);
    EXPECT_EQ(211, GetFocusedViewId());

    ChangeViewDisposition(w211);
    // BasicFocusRules passes focus to the parent.
    EXPECT_EQ(parent_ ? 2 : 21, GetFocusedViewId());
  }

  void BasicActivation() override {
    DCHECK(!parent_) << "Activation tests don't support parent changes.";

    EXPECT_EQ(NULL, GetActiveView());

    View* w2 = root_view()->GetChildById(2);
    ActivateView(w2);
    EXPECT_EQ(2, GetActiveViewId());

    ChangeViewDisposition(w2);
    EXPECT_EQ(3, GetActiveViewId());
    PostDispostionChangeExpectations();
  }

  void FocusEvents() override {
    View* w211 = root_view()->GetChildById(211);
    FocusView(w211);

    ScopedFocusNotificationObserver root_observer(focus_controller());
    ScopedFilteringFocusNotificationObserver observer211(
        focus_controller(), GetViewById(211), GetActiveView(),
        GetFocusedView());

    {
      SCOPED_TRACE("first");
      root_observer.ExpectCounts(0, 0);
      observer211.ExpectCounts(0, 0);
    }

    ChangeViewDisposition(w211);
    {
      SCOPED_TRACE("second");
      {
        SCOPED_TRACE("root_observer");
        root_observer.ExpectCounts(0, 1);
      }
      {
        SCOPED_TRACE("observer211");
        observer211.ExpectCounts(0, 1);
      }
    }
  }

  void ActivationEvents() override {
    DCHECK(!parent_) << "Activation tests don't support parent changes.";

    View* w2 = root_view()->GetChildById(2);
    ActivateView(w2);

    ScopedFocusNotificationObserver root_observer(focus_controller());
    ScopedFilteringFocusNotificationObserver observer2(
        focus_controller(), GetViewById(2), GetActiveView(), GetFocusedView());
    ScopedFilteringFocusNotificationObserver observer3(
        focus_controller(), GetViewById(3), GetActiveView(), GetFocusedView());
    root_observer.ExpectCounts(0, 0);
    observer2.ExpectCounts(0, 0);
    observer3.ExpectCounts(0, 0);

    ChangeViewDisposition(w2);
    root_observer.ExpectCounts(1, 1);
    observer2.ExpectCounts(1, 1);
    observer3.ExpectCounts(1, 1);
  }

  void FocusRulesOverride() override {
    EXPECT_EQ(NULL, GetFocusedView());
    View* w211 = root_view()->GetChildById(211);
    FocusView(w211);
    EXPECT_EQ(211, GetFocusedViewId());

    test_focus_rules()->set_focus_restriction(root_view()->GetChildById(11));
    ChangeViewDisposition(w211);
    // Normally, focus would shift to the parent (w21) but the override shifts
    // it to 11.
    EXPECT_EQ(11, GetFocusedViewId());

    test_focus_rules()->set_focus_restriction(NULL);
  }

  void ActivationRulesOverride() override {
    DCHECK(!parent_) << "Activation tests don't support parent changes.";

    View* w1 = root_view()->GetChildById(1);
    ActivateView(w1);

    EXPECT_EQ(1, GetActiveViewId());
    EXPECT_EQ(1, GetFocusedViewId());

    View* w3 = root_view()->GetChildById(3);
    test_focus_rules()->set_focus_restriction(w3);

    // Normally, activation/focus would move to w2, but since we have a focus
    // restriction, it should move to w3 instead.
    ChangeViewDisposition(w1);
    EXPECT_EQ(3, GetActiveViewId());
    EXPECT_EQ(3, GetFocusedViewId());

    test_focus_rules()->set_focus_restriction(NULL);
    ActivateView(root_view()->GetChildById(2));
    EXPECT_EQ(2, GetActiveViewId());
    EXPECT_EQ(2, GetFocusedViewId());
  }

 private:
  // When true, the disposition change occurs to the parent of the window
  // instead of to the window. This verifies that changes occurring in the
  // hierarchy that contains the window affect the window's focus.
  bool parent_;

  DISALLOW_COPY_AND_ASSIGN(FocusControllerImplicitTestBase);
};

// Focus and Activation changes in response to window visibility changes.
class FocusControllerHideTest : public FocusControllerImplicitTestBase {
 public:
  FocusControllerHideTest() : FocusControllerImplicitTestBase(false) {}

 protected:
  FocusControllerHideTest(bool parent)
      : FocusControllerImplicitTestBase(parent) {}

  // Overridden from FocusControllerImplicitTestBase:
  void ChangeViewDisposition(View* view) override {
    GetDispositionView(view)->SetVisible(false);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FocusControllerHideTest);
};

// Focus and Activation changes in response to window parent visibility
// changes.
class FocusControllerParentHideTest : public FocusControllerHideTest {
 public:
  FocusControllerParentHideTest() : FocusControllerHideTest(true) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FocusControllerParentHideTest);
};

// Focus and Activation changes in response to window destruction.
class FocusControllerDestructionTest : public FocusControllerImplicitTestBase {
 public:
  FocusControllerDestructionTest() : FocusControllerImplicitTestBase(false) {}

 protected:
  FocusControllerDestructionTest(bool parent)
      : FocusControllerImplicitTestBase(parent) {}

  // Overridden from FocusControllerImplicitTestBase:
  void ChangeViewDisposition(View* view) override {
    GetDispositionView(view)->Destroy();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FocusControllerDestructionTest);
};

// Focus and Activation changes in response to window removal.
class FocusControllerRemovalTest : public FocusControllerImplicitTestBase {
 public:
  FocusControllerRemovalTest()
      : FocusControllerImplicitTestBase(false),
        window_to_destroy_(nullptr) {}

 protected:
  FocusControllerRemovalTest(bool parent)
      : FocusControllerImplicitTestBase(parent),
        window_to_destroy_(nullptr) {}

  // Overridden from FocusControllerImplicitTestBase:
  void ChangeViewDisposition(View* view) override {
    View* disposition_view = GetDispositionView(view);
    disposition_view->parent()->RemoveChild(disposition_view);
    window_to_destroy_ = disposition_view;
  }
  void TearDown() override {
    if (window_to_destroy_)
      window_to_destroy_->Destroy();

    FocusControllerImplicitTestBase::TearDown();
  }

 private:
  View* window_to_destroy_;

  DISALLOW_COPY_AND_ASSIGN(FocusControllerRemovalTest);
};

// Focus and Activation changes in response to window parent removal.
class FocusControllerParentRemovalTest : public FocusControllerRemovalTest {
 public:
  FocusControllerParentRemovalTest() : FocusControllerRemovalTest(true) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FocusControllerParentRemovalTest);
};

#define FOCUS_CONTROLLER_TEST(TESTCLASS, TESTNAME) \
  TEST_F(TESTCLASS, TESTNAME) { TESTNAME(); }

// Runs direct focus change tests (input events and API calls).
//
// TODO(erg): Enable gesture events in the future.
#define DIRECT_FOCUS_CHANGE_TESTS(TESTNAME)               \
  FOCUS_CONTROLLER_TEST(FocusControllerApiTest, TESTNAME) \
  FOCUS_CONTROLLER_TEST(FocusControllerMouseEventTest, TESTNAME)

// Runs implicit focus change tests for disposition changes to target.
#define IMPLICIT_FOCUS_CHANGE_TARGET_TESTS(TESTNAME) \
    FOCUS_CONTROLLER_TEST(FocusControllerHideTest, TESTNAME) \
    FOCUS_CONTROLLER_TEST(FocusControllerDestructionTest, TESTNAME) \
    FOCUS_CONTROLLER_TEST(FocusControllerRemovalTest, TESTNAME)

// Runs implicit focus change tests for disposition changes to target's parent
// hierarchy.
#define IMPLICIT_FOCUS_CHANGE_PARENT_TESTS(TESTNAME) \
    FOCUS_CONTROLLER_TEST(FocusControllerParentHideTest, TESTNAME) \
    FOCUS_CONTROLLER_TEST(FocusControllerParentRemovalTest, TESTNAME)
// TODO(erg): FocusControllerParentDestructionTest were commented out in the
// aura version of this file, and don't work when I tried porting things over.

// Runs all implicit focus change tests (changes to the target and target's
// parent hierarchy)
#define IMPLICIT_FOCUS_CHANGE_TESTS(TESTNAME) \
    IMPLICIT_FOCUS_CHANGE_TARGET_TESTS(TESTNAME) \
    IMPLICIT_FOCUS_CHANGE_PARENT_TESTS(TESTNAME)

// Runs all possible focus change tests.
#define ALL_FOCUS_TESTS(TESTNAME) \
    DIRECT_FOCUS_CHANGE_TESTS(TESTNAME) \
    IMPLICIT_FOCUS_CHANGE_TESTS(TESTNAME)

// Runs focus change tests that apply only to the target. For example,
// implicit activation changes caused by window disposition changes do not
// occur when changes to the containing hierarchy happen.
#define TARGET_FOCUS_TESTS(TESTNAME) \
    DIRECT_FOCUS_CHANGE_TESTS(TESTNAME) \
    IMPLICIT_FOCUS_CHANGE_TARGET_TESTS(TESTNAME)

// - Focuses a window, verifies that focus changed.
ALL_FOCUS_TESTS(BasicFocus);

// - Activates a window, verifies that activation changed.
TARGET_FOCUS_TESTS(BasicActivation);

// - Focuses a window, verifies that focus events were dispatched.
ALL_FOCUS_TESTS(FocusEvents);

// - Focuses or activates a window multiple times, verifies that events are only
//   dispatched when focus/activation actually changes.
DIRECT_FOCUS_CHANGE_TESTS(DuplicateFocusEvents);
DIRECT_FOCUS_CHANGE_TESTS(DuplicateActivationEvents);

// - Activates a window, verifies that activation events were dispatched.
TARGET_FOCUS_TESTS(ActivationEvents);

// - Attempts to active a hidden view, verifies that current view is
//   attempted to be reactivated and the appropriate event dispatched.
FOCUS_CONTROLLER_TEST(FocusControllerApiTest, ReactivationEvents);

// - Input events/API calls shift focus between focusable views within the
//   active view.
DIRECT_FOCUS_CHANGE_TESTS(ShiftFocusWithinActiveView);

// - Input events/API calls to a child view of an inactive view shifts
//   activation to the activatable parent and focuses the child.
DIRECT_FOCUS_CHANGE_TESTS(ShiftFocusToChildOfInactiveView);

// - Input events/API calls to focus the parent of the focused view do not
//   shift focus away from the child.
DIRECT_FOCUS_CHANGE_TESTS(ShiftFocusToParentOfFocusedView);

// - Verifies that FocusRules determine what can be focused.
ALL_FOCUS_TESTS(FocusRulesOverride);

// - Verifies that FocusRules determine what can be activated.
TARGET_FOCUS_TESTS(ActivationRulesOverride);

// - Verifies that attempts to change focus or activation from a focus or
//   activation change observer are ignored.
DIRECT_FOCUS_CHANGE_TESTS(ShiftFocusOnActivation);
DIRECT_FOCUS_CHANGE_TESTS(ShiftFocusOnActivationDueToHide);
DIRECT_FOCUS_CHANGE_TESTS(NoShiftActiveOnActivation);

FOCUS_CONTROLLER_TEST(FocusControllerApiTest,
                      ChangeFocusWhenNothingFocusedAndCaptured);

// See description above DontPassDestroyedView() for details.
FOCUS_CONTROLLER_TEST(FocusControllerApiTest, DontPassDestroyedView);

// TODO(erg): Add the TextInputClient tests here.

// If a mouse event was handled, it should not activate a view.
FOCUS_CONTROLLER_TEST(FocusControllerMouseEventTest, IgnoreHandledEvent);

}  // namespace window_manager
