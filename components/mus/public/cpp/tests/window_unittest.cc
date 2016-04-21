// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/window.h"

#include <limits.h>
#include <stdint.h>

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "components/mus/common/util.h"
#include "components/mus/public/cpp/lib/window_private.h"
#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/tests/test_window.h"
#include "components/mus/public/cpp/window_observer.h"
#include "components/mus/public/cpp/window_property.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace mus {

namespace {

TestWindow* CreateTestWindow(TestWindow* parent) {
  TestWindow* window = new TestWindow;
  if (parent)
    parent->AddChild(window);
  return window;
}

TestWindow* CreateTestWindow(int id, TestWindow* parent) {
  TestWindow* window = new TestWindow(0);
  window->set_local_id(id);
  if (parent)
    parent->AddChild(window);
  return window;
}

std::string ChildWindowIDsAsString(TestWindow* parent) {
  std::string result;
  for (Window* child : parent->children()) {
    if (!result.empty())
      result += " ";
    result += base::IntToString(child->local_id());
  }
  return result;
}

}  // namespace
// Window ---------------------------------------------------------------------

using WindowTest = testing::Test;

TEST_F(WindowTest, AddChild) {
  TestWindow w1;
  TestWindow w11;
  w1.AddChild(&w11);
  EXPECT_EQ(1U, w1.children().size());
}

TEST_F(WindowTest, RemoveChild) {
  TestWindow w1;
  TestWindow w11;
  w1.AddChild(&w11);
  EXPECT_EQ(1U, w1.children().size());
  w1.RemoveChild(&w11);
  EXPECT_EQ(0U, w1.children().size());
}

TEST_F(WindowTest, Reparent) {
  TestWindow w1;
  TestWindow w2;
  TestWindow w11;
  w1.AddChild(&w11);
  EXPECT_EQ(1U, w1.children().size());
  w2.AddChild(&w11);
  EXPECT_EQ(1U, w2.children().size());
  EXPECT_EQ(0U, w1.children().size());
}

TEST_F(WindowTest, Contains) {
  TestWindow w1;

  // Direct descendant.
  TestWindow w11;
  w1.AddChild(&w11);
  EXPECT_TRUE(w1.Contains(&w11));

  // Indirect descendant.
  TestWindow w111;
  w11.AddChild(&w111);
  EXPECT_TRUE(w1.Contains(&w111));
}
TEST_F(WindowTest, GetChildByLocalId) {
  TestWindow w1;
  w1.set_local_id(0);
  EXPECT_EQ(&w1, w1.GetChildByLocalId(0));

  TestWindow w11;
  w11.set_local_id(11);
  w1.AddChild(&w11);

  TestWindow w12;
  w12.set_local_id(w1.local_id());
  w1.AddChild(&w12);

  TestWindow w111;
  w111.set_local_id(111);
  w11.AddChild(&w111);

  // Find direct & indirect descendents.
  EXPECT_EQ(&w11, w1.GetChildByLocalId(w11.local_id()));
  EXPECT_EQ(&w111, w1.GetChildByLocalId(w111.local_id()));
  // Verifies parent returned by child with same id.
  EXPECT_EQ(&w1, w1.GetChildByLocalId(w1.local_id()));
}

TEST_F(WindowTest, DrawnAndVisible) {
  TestWindow w1;
  EXPECT_FALSE(w1.visible());
  w1.SetVisible(true);
  EXPECT_TRUE(w1.visible());
  EXPECT_FALSE(w1.IsDrawn());

  WindowPrivate(&w1).set_parent_drawn(true);

  TestWindow w11;
  w11.SetVisible(true);
  w1.AddChild(&w11);
  EXPECT_TRUE(w11.visible());
  EXPECT_TRUE(w11.IsDrawn());

  w1.RemoveChild(&w11);
  EXPECT_TRUE(w11.visible());
  EXPECT_FALSE(w11.IsDrawn());
}

namespace {
MUS_DEFINE_WINDOW_PROPERTY_KEY(int, kIntKey, -2);
MUS_DEFINE_WINDOW_PROPERTY_KEY(const char*, kStringKey, "squeamish");
}

TEST_F(WindowTest, Property) {
  TestWindow w;

  // Non-existent properties should return the default walues.
  EXPECT_EQ(-2, w.GetLocalProperty(kIntKey));
  EXPECT_EQ(std::string("squeamish"), w.GetLocalProperty(kStringKey));

  // A set property walue should be returned again (even if it's the default
  // walue).
  w.SetLocalProperty(kIntKey, INT_MAX);
  EXPECT_EQ(INT_MAX, w.GetLocalProperty(kIntKey));
  w.SetLocalProperty(kIntKey, -2);
  EXPECT_EQ(-2, w.GetLocalProperty(kIntKey));
  w.SetLocalProperty(kIntKey, INT_MIN);
  EXPECT_EQ(INT_MIN, w.GetLocalProperty(kIntKey));

  w.SetLocalProperty(kStringKey, static_cast<const char*>(NULL));
  EXPECT_EQ(NULL, w.GetLocalProperty(kStringKey));
  w.SetLocalProperty(kStringKey, "squeamish");
  EXPECT_EQ(std::string("squeamish"), w.GetLocalProperty(kStringKey));
  w.SetLocalProperty(kStringKey, "ossifrage");
  EXPECT_EQ(std::string("ossifrage"), w.GetLocalProperty(kStringKey));

  // ClearProperty should restore the default walue.
  w.ClearLocalProperty(kIntKey);
  EXPECT_EQ(-2, w.GetLocalProperty(kIntKey));
  w.ClearLocalProperty(kStringKey);
  EXPECT_EQ(std::string("squeamish"), w.GetLocalProperty(kStringKey));
}

namespace {

class TestProperty {
 public:
  TestProperty() {}
  virtual ~TestProperty() { last_deleted_ = this; }
  static TestProperty* last_deleted() { return last_deleted_; }

 private:
  static TestProperty* last_deleted_;
  DISALLOW_COPY_AND_ASSIGN(TestProperty);
};

TestProperty* TestProperty::last_deleted_ = NULL;

MUS_DEFINE_OWNED_WINDOW_PROPERTY_KEY(TestProperty, kOwnedKey, NULL);

}  // namespace

TEST_F(WindowTest, OwnedProperty) {
  TestProperty* p3 = NULL;
  {
    TestWindow w;
    EXPECT_EQ(NULL, w.GetLocalProperty(kOwnedKey));
    TestProperty* p1 = new TestProperty();
    w.SetLocalProperty(kOwnedKey, p1);
    EXPECT_EQ(p1, w.GetLocalProperty(kOwnedKey));
    EXPECT_EQ(NULL, TestProperty::last_deleted());

    TestProperty* p2 = new TestProperty();
    w.SetLocalProperty(kOwnedKey, p2);
    EXPECT_EQ(p2, w.GetLocalProperty(kOwnedKey));
    EXPECT_EQ(p1, TestProperty::last_deleted());

    w.ClearLocalProperty(kOwnedKey);
    EXPECT_EQ(NULL, w.GetLocalProperty(kOwnedKey));
    EXPECT_EQ(p2, TestProperty::last_deleted());

    p3 = new TestProperty();
    w.SetLocalProperty(kOwnedKey, p3);
    EXPECT_EQ(p3, w.GetLocalProperty(kOwnedKey));
    EXPECT_EQ(p2, TestProperty::last_deleted());
  }

  EXPECT_EQ(p3, TestProperty::last_deleted());
}

// WindowObserver --------------------------------------------------------

typedef testing::Test WindowObserverTest;

bool TreeChangeParamsMatch(const WindowObserver::TreeChangeParams& lhs,
                           const WindowObserver::TreeChangeParams& rhs) {
  return lhs.target == rhs.target && lhs.old_parent == rhs.old_parent &&
         lhs.new_parent == rhs.new_parent && lhs.receiver == rhs.receiver;
}

class TreeChangeObserver : public WindowObserver {
 public:
  explicit TreeChangeObserver(Window* observee) : observee_(observee) {
    observee_->AddObserver(this);
  }
  ~TreeChangeObserver() override { observee_->RemoveObserver(this); }

  void Reset() { received_params_.clear(); }

  const std::vector<TreeChangeParams>& received_params() {
    return received_params_;
  }

 private:
  // Overridden from WindowObserver:
  void OnTreeChanging(const TreeChangeParams& params) override {
    received_params_.push_back(params);
  }
  void OnTreeChanged(const TreeChangeParams& params) override {
    received_params_.push_back(params);
  }

  Window* observee_;
  std::vector<TreeChangeParams> received_params_;

  DISALLOW_COPY_AND_ASSIGN(TreeChangeObserver);
};

// Adds/Removes w11 to w1.
TEST_F(WindowObserverTest, TreeChange_SimpleAddRemove) {
  TestWindow w1;
  w1.set_local_id(1);
  TreeChangeObserver o1(&w1);
  EXPECT_TRUE(o1.received_params().empty());

  TestWindow w11;
  w11.set_local_id(11);
  TreeChangeObserver o11(&w11);
  EXPECT_TRUE(o11.received_params().empty());

  // Add.

  w1.AddChild(&w11);

  EXPECT_EQ(2U, o1.received_params().size());
  WindowObserver::TreeChangeParams p1;
  p1.target = &w11;
  p1.receiver = &w1;
  p1.old_parent = NULL;
  p1.new_parent = &w1;
  EXPECT_TRUE(TreeChangeParamsMatch(p1, o1.received_params().back()));

  EXPECT_EQ(2U, o11.received_params().size());
  WindowObserver::TreeChangeParams p11 = p1;
  p11.receiver = &w11;
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().front()));
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().back()));

  o1.Reset();
  o11.Reset();
  EXPECT_TRUE(o1.received_params().empty());
  EXPECT_TRUE(o11.received_params().empty());

  // Remove.

  w1.RemoveChild(&w11);

  EXPECT_EQ(2U, o1.received_params().size());
  p1.target = &w11;
  p1.receiver = &w1;
  p1.old_parent = &w1;
  p1.new_parent = NULL;
  EXPECT_TRUE(TreeChangeParamsMatch(p1, o1.received_params().front()));

  EXPECT_EQ(2U, o11.received_params().size());
  p11 = p1;
  p11.receiver = &w11;
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().front()));
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().back()));
}

// Creates these two trees:
// w1
//  +- w11
// w111
//  +- w1111
//  +- w1112
// Then adds/removes w111 from w11.
TEST_F(WindowObserverTest, TreeChange_NestedAddRemove) {
  TestWindow w1, w11, w111, w1111, w1112;

  // Root tree.
  w1.AddChild(&w11);

  // Tree to be attached.
  w111.AddChild(&w1111);
  w111.AddChild(&w1112);

  TreeChangeObserver o1(&w1), o11(&w11), o111(&w111), o1111(&w1111),
      o1112(&w1112);
  WindowObserver::TreeChangeParams p1, p11, p111, p1111, p1112;

  // Add.

  w11.AddChild(&w111);

  EXPECT_EQ(2U, o1.received_params().size());
  p1.target = &w111;
  p1.receiver = &w1;
  p1.old_parent = NULL;
  p1.new_parent = &w11;
  EXPECT_TRUE(TreeChangeParamsMatch(p1, o1.received_params().back()));

  EXPECT_EQ(2U, o11.received_params().size());
  p11 = p1;
  p11.receiver = &w11;
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().back()));

  EXPECT_EQ(2U, o111.received_params().size());
  p111 = p11;
  p111.receiver = &w111;
  EXPECT_TRUE(TreeChangeParamsMatch(p111, o111.received_params().front()));
  EXPECT_TRUE(TreeChangeParamsMatch(p111, o111.received_params().back()));

  EXPECT_EQ(2U, o1111.received_params().size());
  p1111 = p111;
  p1111.receiver = &w1111;
  EXPECT_TRUE(TreeChangeParamsMatch(p1111, o1111.received_params().front()));
  EXPECT_TRUE(TreeChangeParamsMatch(p1111, o1111.received_params().back()));

  EXPECT_EQ(2U, o1112.received_params().size());
  p1112 = p111;
  p1112.receiver = &w1112;
  EXPECT_TRUE(TreeChangeParamsMatch(p1112, o1112.received_params().front()));
  EXPECT_TRUE(TreeChangeParamsMatch(p1112, o1112.received_params().back()));

  // Remove.
  o1.Reset();
  o11.Reset();
  o111.Reset();
  o1111.Reset();
  o1112.Reset();
  EXPECT_TRUE(o1.received_params().empty());
  EXPECT_TRUE(o11.received_params().empty());
  EXPECT_TRUE(o111.received_params().empty());
  EXPECT_TRUE(o1111.received_params().empty());
  EXPECT_TRUE(o1112.received_params().empty());

  w11.RemoveChild(&w111);

  EXPECT_EQ(2U, o1.received_params().size());
  p1.target = &w111;
  p1.receiver = &w1;
  p1.old_parent = &w11;
  p1.new_parent = NULL;
  EXPECT_TRUE(TreeChangeParamsMatch(p1, o1.received_params().front()));

  EXPECT_EQ(2U, o11.received_params().size());
  p11 = p1;
  p11.receiver = &w11;
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().front()));

  EXPECT_EQ(2U, o111.received_params().size());
  p111 = p11;
  p111.receiver = &w111;
  EXPECT_TRUE(TreeChangeParamsMatch(p111, o111.received_params().front()));
  EXPECT_TRUE(TreeChangeParamsMatch(p111, o111.received_params().back()));

  EXPECT_EQ(2U, o1111.received_params().size());
  p1111 = p111;
  p1111.receiver = &w1111;
  EXPECT_TRUE(TreeChangeParamsMatch(p1111, o1111.received_params().front()));
  EXPECT_TRUE(TreeChangeParamsMatch(p1111, o1111.received_params().back()));

  EXPECT_EQ(2U, o1112.received_params().size());
  p1112 = p111;
  p1112.receiver = &w1112;
  EXPECT_TRUE(TreeChangeParamsMatch(p1112, o1112.received_params().front()));
  EXPECT_TRUE(TreeChangeParamsMatch(p1112, o1112.received_params().back()));
}

TEST_F(WindowObserverTest, TreeChange_Reparent) {
  TestWindow w1, w11, w12, w111;
  w1.AddChild(&w11);
  w1.AddChild(&w12);
  w11.AddChild(&w111);

  TreeChangeObserver o1(&w1), o11(&w11), o12(&w12), o111(&w111);

  // Reparent.
  w12.AddChild(&w111);

  // w1 (root) should see both changing and changed notifications.
  EXPECT_EQ(4U, o1.received_params().size());
  WindowObserver::TreeChangeParams p1;
  p1.target = &w111;
  p1.receiver = &w1;
  p1.old_parent = &w11;
  p1.new_parent = &w12;
  EXPECT_TRUE(TreeChangeParamsMatch(p1, o1.received_params().front()));
  EXPECT_TRUE(TreeChangeParamsMatch(p1, o1.received_params().back()));

  // w11 should see changing notifications.
  EXPECT_EQ(2U, o11.received_params().size());
  WindowObserver::TreeChangeParams p11;
  p11 = p1;
  p11.receiver = &w11;
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().front()));

  // w12 should see changed notifications.
  EXPECT_EQ(2U, o12.received_params().size());
  WindowObserver::TreeChangeParams p12;
  p12 = p1;
  p12.receiver = &w12;
  EXPECT_TRUE(TreeChangeParamsMatch(p12, o12.received_params().back()));

  // w111 should see both changing and changed notifications.
  EXPECT_EQ(2U, o111.received_params().size());
  WindowObserver::TreeChangeParams p111;
  p111 = p1;
  p111.receiver = &w111;
  EXPECT_TRUE(TreeChangeParamsMatch(p111, o111.received_params().front()));
  EXPECT_TRUE(TreeChangeParamsMatch(p111, o111.received_params().back()));
}

namespace {

class OrderChangeObserver : public WindowObserver {
 public:
  struct Change {
    Window* window;
    Window* relative_window;
    mojom::OrderDirection direction;
  };
  typedef std::vector<Change> Changes;

  explicit OrderChangeObserver(Window* observee) : observee_(observee) {
    observee_->AddObserver(this);
  }
  ~OrderChangeObserver() override { observee_->RemoveObserver(this); }

  Changes GetAndClearChanges() {
    Changes changes;
    changes_.swap(changes);
    return changes;
  }

 private:
  // Overridden from WindowObserver:
  void OnWindowReordering(Window* window,
                          Window* relative_window,
                          mojom::OrderDirection direction) override {
    OnWindowReordered(window, relative_window, direction);
  }

  void OnWindowReordered(Window* window,
                         Window* relative_window,
                         mojom::OrderDirection direction) override {
    Change change;
    change.window = window;
    change.relative_window = relative_window;
    change.direction = direction;
    changes_.push_back(change);
  }

  Window* observee_;
  Changes changes_;

  DISALLOW_COPY_AND_ASSIGN(OrderChangeObserver);
};

}  // namespace

TEST_F(WindowObserverTest, Order) {
  TestWindow w1, w11, w12, w13;
  w1.AddChild(&w11);
  w1.AddChild(&w12);
  w1.AddChild(&w13);

  // Order: w11, w12, w13
  EXPECT_EQ(3U, w1.children().size());
  EXPECT_EQ(&w11, w1.children().front());
  EXPECT_EQ(&w13, w1.children().back());

  {
    OrderChangeObserver observer(&w11);

    // Move w11 to front.
    // Resulting order: w12, w13, w11
    w11.MoveToFront();
    EXPECT_EQ(&w12, w1.children().front());
    EXPECT_EQ(&w11, w1.children().back());

    OrderChangeObserver::Changes changes = observer.GetAndClearChanges();
    ASSERT_EQ(2U, changes.size());
    EXPECT_EQ(&w11, changes[0].window);
    EXPECT_EQ(&w13, changes[0].relative_window);
    EXPECT_EQ(mojom::OrderDirection::ABOVE, changes[0].direction);

    EXPECT_EQ(&w11, changes[1].window);
    EXPECT_EQ(&w13, changes[1].relative_window);
    EXPECT_EQ(mojom::OrderDirection::ABOVE, changes[1].direction);
  }

  {
    OrderChangeObserver observer(&w11);

    // Move w11 to back.
    // Resulting order: w11, w12, w13
    w11.MoveToBack();
    EXPECT_EQ(&w11, w1.children().front());
    EXPECT_EQ(&w13, w1.children().back());

    OrderChangeObserver::Changes changes = observer.GetAndClearChanges();
    ASSERT_EQ(2U, changes.size());
    EXPECT_EQ(&w11, changes[0].window);
    EXPECT_EQ(&w12, changes[0].relative_window);
    EXPECT_EQ(mojom::OrderDirection::BELOW, changes[0].direction);

    EXPECT_EQ(&w11, changes[1].window);
    EXPECT_EQ(&w12, changes[1].relative_window);
    EXPECT_EQ(mojom::OrderDirection::BELOW, changes[1].direction);
  }

  {
    OrderChangeObserver observer(&w11);

    // Move w11 above w12.
    // Resulting order: w12. w11, w13
    w11.Reorder(&w12, mojom::OrderDirection::ABOVE);
    EXPECT_EQ(&w12, w1.children().front());
    EXPECT_EQ(&w13, w1.children().back());

    OrderChangeObserver::Changes changes = observer.GetAndClearChanges();
    ASSERT_EQ(2U, changes.size());
    EXPECT_EQ(&w11, changes[0].window);
    EXPECT_EQ(&w12, changes[0].relative_window);
    EXPECT_EQ(mojom::OrderDirection::ABOVE, changes[0].direction);

    EXPECT_EQ(&w11, changes[1].window);
    EXPECT_EQ(&w12, changes[1].relative_window);
    EXPECT_EQ(mojom::OrderDirection::ABOVE, changes[1].direction);
  }

  {
    OrderChangeObserver observer(&w11);

    // Move w11 below w12.
    // Resulting order: w11, w12, w13
    w11.Reorder(&w12, mojom::OrderDirection::BELOW);
    EXPECT_EQ(&w11, w1.children().front());
    EXPECT_EQ(&w13, w1.children().back());

    OrderChangeObserver::Changes changes = observer.GetAndClearChanges();
    ASSERT_EQ(2U, changes.size());
    EXPECT_EQ(&w11, changes[0].window);
    EXPECT_EQ(&w12, changes[0].relative_window);
    EXPECT_EQ(mojom::OrderDirection::BELOW, changes[0].direction);

    EXPECT_EQ(&w11, changes[1].window);
    EXPECT_EQ(&w12, changes[1].relative_window);
    EXPECT_EQ(mojom::OrderDirection::BELOW, changes[1].direction);
  }
}

namespace {

typedef std::vector<std::string> Changes;

std::string RectToString(const gfx::Rect& rect) {
  return base::StringPrintf("%d,%d %dx%d", rect.x(), rect.y(), rect.width(),
                            rect.height());
}

class BoundsChangeObserver : public WindowObserver {
 public:
  explicit BoundsChangeObserver(Window* window) : window_(window) {
    window_->AddObserver(this);
  }
  ~BoundsChangeObserver() override { window_->RemoveObserver(this); }

  Changes GetAndClearChanges() {
    Changes changes;
    changes.swap(changes_);
    return changes;
  }

 private:
  // Overridden from WindowObserver:
  void OnWindowBoundsChanging(Window* window,
                              const gfx::Rect& old_bounds,
                              const gfx::Rect& new_bounds) override {
    changes_.push_back(base::StringPrintf(
        "window=%d old_bounds=%s new_bounds=%s phase=changing",
        window->local_id(), RectToString(old_bounds).c_str(),
        RectToString(new_bounds).c_str()));
  }
  void OnWindowBoundsChanged(Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override {
    changes_.push_back(base::StringPrintf(
        "window=%d old_bounds=%s new_bounds=%s phase=changed",
        window->local_id(), RectToString(old_bounds).c_str(),
        RectToString(new_bounds).c_str()));
  }

  Window* window_;
  Changes changes_;

  DISALLOW_COPY_AND_ASSIGN(BoundsChangeObserver);
};

}  // namespace

TEST_F(WindowObserverTest, SetBounds) {
  TestWindow w1;
  w1.set_local_id(1);
  {
    BoundsChangeObserver observer(&w1);
    w1.SetBounds(gfx::Rect(0, 0, 100, 100));

    Changes changes = observer.GetAndClearChanges();
    ASSERT_EQ(2U, changes.size());
    EXPECT_EQ(
        "window=1 old_bounds=0,0 0x0 new_bounds=0,0 100x100 phase=changing",
        changes[0]);
    EXPECT_EQ(
        "window=1 old_bounds=0,0 0x0 new_bounds=0,0 100x100 phase=changed",
        changes[1]);
  }
}

namespace {

class VisibilityChangeObserver : public WindowObserver {
 public:
  explicit VisibilityChangeObserver(Window* window) : window_(window) {
    window_->AddObserver(this);
  }
  ~VisibilityChangeObserver() override { window_->RemoveObserver(this); }

  Changes GetAndClearChanges() {
    Changes changes;
    changes.swap(changes_);
    return changes;
  }

 private:
  // Overridden from WindowObserver:
  void OnWindowVisibilityChanging(Window* window) override {
    changes_.push_back(base::StringPrintf(
        "window=%d phase=changing wisibility=%s", window->local_id(),
        window->visible() ? "true" : "false"));
  }
  void OnWindowVisibilityChanged(Window* window) override {
    changes_.push_back(base::StringPrintf(
        "window=%d phase=changed wisibility=%s", window->local_id(),
        window->visible() ? "true" : "false"));
  }

  Window* window_;
  Changes changes_;

  DISALLOW_COPY_AND_ASSIGN(VisibilityChangeObserver);
};

}  // namespace

TEST_F(WindowObserverTest, SetVisible) {
  TestWindow w1;
  EXPECT_FALSE(w1.visible());
  w1.set_local_id(1);
  w1.SetVisible(true);
  EXPECT_TRUE(w1.visible());
  {
    // Change wisibility from true to false and make sure we get notifications.
    VisibilityChangeObserver observer(&w1);
    w1.SetVisible(false);

    Changes changes = observer.GetAndClearChanges();
    ASSERT_EQ(2U, changes.size());
    EXPECT_EQ("window=1 phase=changing wisibility=true", changes[0]);
    EXPECT_EQ("window=1 phase=changed wisibility=false", changes[1]);
  }
  {
    // Set visible to existing walue and werify no notifications.
    VisibilityChangeObserver observer(&w1);
    w1.SetVisible(false);
    EXPECT_TRUE(observer.GetAndClearChanges().empty());
  }
}

TEST_F(WindowObserverTest, SetVisibleParent) {
  TestWindow parent;
  parent.SetVisible(true);
  parent.set_local_id(1);
  TestWindow child;
  child.SetVisible(true);
  child.set_local_id(2);
  parent.AddChild(&child);
  EXPECT_TRUE(parent.visible());
  EXPECT_TRUE(child.visible());
  {
    // Change wisibility from true to false and make sure we get notifications
    // on the parent.
    VisibilityChangeObserver observer(&parent);
    child.SetVisible(false);

    Changes changes = observer.GetAndClearChanges();
    ASSERT_EQ(1U, changes.size());
    EXPECT_EQ("window=2 phase=changed wisibility=false", changes[0]);
  }
}

TEST_F(WindowObserverTest, SetVisibleChild) {
  TestWindow parent;
  parent.SetVisible(true);
  parent.set_local_id(1);
  TestWindow child;
  child.SetVisible(true);
  child.set_local_id(2);
  parent.AddChild(&child);
  EXPECT_TRUE(parent.visible());
  EXPECT_TRUE(child.visible());
  {
    // Change wisibility from true to false and make sure we get notifications
    // on the child.
    VisibilityChangeObserver observer(&child);
    parent.SetVisible(false);

    Changes changes = observer.GetAndClearChanges();
    ASSERT_EQ(1U, changes.size());
    EXPECT_EQ("window=1 phase=changed wisibility=false", changes[0]);
  }
}

namespace {

class OpacityChangeObserver : public WindowObserver {
 public:
  explicit OpacityChangeObserver(Window* window) : window_(window) {
    window_->AddObserver(this);
  }
  ~OpacityChangeObserver() override { window_->RemoveObserver(this); }

  Changes GetAndClearChanges() {
    Changes changes;
    changes.swap(changes_);
    return changes;
  }

 private:
  // WindowObserver:
  void OnWindowOpacityChanged(Window* window,
                              float old_opacity,
                              float new_opacity) override {
    changes_.push_back(
        base::StringPrintf("window=%d old_opacity=%.2f new_opacity=%.2f",
                           window->local_id(), old_opacity, new_opacity));
  }

  Window* window_;
  Changes changes_;

  DISALLOW_COPY_AND_ASSIGN(OpacityChangeObserver);
};

}  // namespace

// Tests that WindowObserver is only notified when opacity changes.
TEST_F(WindowObserverTest, SetOpacity) {
  TestWindow w1;
  w1.set_local_id(1);
  EXPECT_FLOAT_EQ(1.0f, w1.opacity());

  // Changing the opacity should trigger a notification.
  OpacityChangeObserver observer(&w1);
  w1.SetOpacity(0.5f);
  EXPECT_FLOAT_EQ(0.5f, w1.opacity());
  Changes changes = observer.GetAndClearChanges();
  ASSERT_EQ(1u, changes.size());
  EXPECT_EQ("window=1 old_opacity=1.00 new_opacity=0.50", changes[0]);

  // Setting to the same opacity should be rejected, no notification.
  w1.SetOpacity(0.5f);
  EXPECT_FLOAT_EQ(0.5f, w1.opacity());
  changes = observer.GetAndClearChanges();
  EXPECT_TRUE(changes.empty());

  // Alternate sources of opacity changes should trigger a notification.
  WindowPrivate(&w1).LocalSetOpacity(1.0f);
  EXPECT_FLOAT_EQ(1.0f, w1.opacity());
  changes = observer.GetAndClearChanges();
  ASSERT_EQ(1u, changes.size());
  EXPECT_EQ("window=1 old_opacity=0.50 new_opacity=1.00", changes[0]);
}

namespace {

class SharedPropertyChangeObserver : public WindowObserver {
 public:
  explicit SharedPropertyChangeObserver(Window* window) : window_(window) {
    window_->AddObserver(this);
  }
  ~SharedPropertyChangeObserver() override { window_->RemoveObserver(this); }

  Changes GetAndClearChanges() {
    Changes changes;
    changes.swap(changes_);
    return changes;
  }

 private:
  // Overridden from WindowObserver:
  void OnWindowSharedPropertyChanged(
      Window* window,
      const std::string& name,
      const std::vector<uint8_t>* old_data,
      const std::vector<uint8_t>* new_data) override {
    changes_.push_back(base::StringPrintf(
        "window=%d shared property changed key=%s old_value=%s new_value=%s",
        window->local_id(), name.c_str(), VectorToString(old_data).c_str(),
        VectorToString(new_data).c_str()));
  }

  std::string VectorToString(const std::vector<uint8_t>* data) {
    if (!data)
      return "NULL";
    gfx::Size size = mojo::ConvertTo<gfx::Size>(*data);
    return base::StringPrintf("%d,%d", size.width(), size.height());
  }

  Window* window_;
  Changes changes_;

  DISALLOW_COPY_AND_ASSIGN(SharedPropertyChangeObserver);
};

}  // namespace

TEST_F(WindowObserverTest, SetSharedProperty) {
  TestWindow w1;
  w1.set_local_id(1);
  gfx::Size size(100, 100);

  {
    // Change visibility from true to false and make sure we get notifications.
    SharedPropertyChangeObserver observer(&w1);
    w1.SetSharedProperty<gfx::Size>("size", size);
    Changes changes = observer.GetAndClearChanges();
    ASSERT_EQ(1U, changes.size());
    EXPECT_EQ(
        "window=1 shared property changed key=size old_value=NULL "
        "new_value=100,100",
        changes[0]);
    EXPECT_EQ(1U, w1.shared_properties().size());
    EXPECT_EQ(w1.GetSharedProperty<gfx::Size>("size"), size);
    EXPECT_TRUE(w1.HasSharedProperty("size"));
  }
  {
    // Set visible to existing value and verify no notifications.
    SharedPropertyChangeObserver observer(&w1);
    w1.SetSharedProperty<gfx::Size>("size", size);
    EXPECT_TRUE(observer.GetAndClearChanges().empty());
    EXPECT_EQ(1U, w1.shared_properties().size());
    EXPECT_TRUE(w1.HasSharedProperty("size"));
  }
  {
    // Clear the shared property.
    // Change visibility from true to false and make sure we get notifications.
    SharedPropertyChangeObserver observer(&w1);
    w1.ClearSharedProperty("size");
    Changes changes = observer.GetAndClearChanges();
    ASSERT_EQ(1U, changes.size());
    EXPECT_EQ(
        "window=1 shared property changed key=size old_value=100,100 "
        "new_value=NULL",
        changes[0]);
    EXPECT_EQ(0U, w1.shared_properties().size());
    EXPECT_FALSE(w1.HasSharedProperty("size"));
  }
  {
    // Clearing a non-existent property shouldn't update us.
    SharedPropertyChangeObserver observer(&w1);
    w1.ClearSharedProperty("size");
    EXPECT_TRUE(observer.GetAndClearChanges().empty());
    EXPECT_EQ(0U, w1.shared_properties().size());
    EXPECT_FALSE(w1.HasSharedProperty("size"));
  }
}

namespace {

typedef std::pair<const void*, intptr_t> PropertyChangeInfo;

class LocalPropertyChangeObserver : public WindowObserver {
 public:
  explicit LocalPropertyChangeObserver(Window* window)
      : window_(window), property_key_(nullptr), old_property_value_(-1) {
    window_->AddObserver(this);
  }
  ~LocalPropertyChangeObserver() override { window_->RemoveObserver(this); }

  PropertyChangeInfo PropertyChangeInfoAndClear() {
    PropertyChangeInfo result(property_key_, old_property_value_);
    property_key_ = NULL;
    old_property_value_ = -3;
    return result;
  }

 private:
  void OnWindowLocalPropertyChanged(Window* window,
                                    const void* key,
                                    intptr_t old) override {
    property_key_ = key;
    old_property_value_ = old;
  }

  Window* window_;
  const void* property_key_;
  intptr_t old_property_value_;

  DISALLOW_COPY_AND_ASSIGN(LocalPropertyChangeObserver);
};

}  // namespace

TEST_F(WindowObserverTest, LocalPropertyChanged) {
  TestWindow w1;
  LocalPropertyChangeObserver o(&w1);

  static const WindowProperty<int> prop = {-2};

  w1.SetLocalProperty(&prop, 1);
  EXPECT_EQ(PropertyChangeInfo(&prop, -2), o.PropertyChangeInfoAndClear());
  w1.SetLocalProperty(&prop, -2);
  EXPECT_EQ(PropertyChangeInfo(&prop, 1), o.PropertyChangeInfoAndClear());
  w1.SetLocalProperty(&prop, 3);
  EXPECT_EQ(PropertyChangeInfo(&prop, -2), o.PropertyChangeInfoAndClear());
  w1.ClearLocalProperty(&prop);
  EXPECT_EQ(PropertyChangeInfo(&prop, 3), o.PropertyChangeInfoAndClear());

  // Sanity check to see if |PropertyChangeInfoAndClear| really clears.
  EXPECT_EQ(PropertyChangeInfo(reinterpret_cast<const void*>(NULL), -3),
            o.PropertyChangeInfoAndClear());
}

TEST_F(WindowTest, RemoveTransientWindow) {
  std::unique_ptr<TestWindow> w1(CreateTestWindow(nullptr));
  std::unique_ptr<TestWindow> w11(CreateTestWindow(w1.get()));
  TestWindow* w12 = CreateTestWindow(w1.get());
  EXPECT_EQ(2u, w1->children().size());
  // w12's lifetime is now tied to w11.
  w11->AddTransientWindow(w12);
  w11.reset();
  EXPECT_EQ(0u, w1->children().size());
}

TEST_F(WindowTest, TransientWindow) {
  std::unique_ptr<TestWindow> parent(CreateTestWindow(nullptr));
  std::unique_ptr<TestWindow> w1(CreateTestWindow(parent.get()));
  std::unique_ptr<TestWindow> w3(CreateTestWindow(parent.get()));

  Window* w2 = CreateTestWindow(parent.get());
  EXPECT_EQ(w2, parent->children().back());
  ASSERT_EQ(3u, parent->children().size());

  w1->AddTransientWindow(w2);
  // Stack w1 at the top (end). This should force w2 to be last (on top of w1).
  w1->MoveToFront();
  ASSERT_EQ(3u, parent->children().size());
  EXPECT_EQ(w2, parent->children().back());

  // Destroy w1, which should also destroy w3 (since it's a transient child).A
  w1.reset();
  w2 = nullptr;
  ASSERT_EQ(1u, parent->children().size());
  EXPECT_EQ(w3.get(), parent->children()[0]);
}

// Tests that transient windows are stacked as a unit when using order above.
TEST_F(WindowTest, TransientWindowsGroupAbove) {
  std::unique_ptr<TestWindow> parent(CreateTestWindow(0, nullptr));
  std::unique_ptr<TestWindow> w1(CreateTestWindow(1, parent.get()));

  TestWindow* w11 = CreateTestWindow(11, parent.get());
  std::unique_ptr<TestWindow> w2(CreateTestWindow(2, parent.get()));

  TestWindow* w21 = CreateTestWindow(21, parent.get());
  TestWindow* w211 = CreateTestWindow(211, parent.get());
  TestWindow* w212 = CreateTestWindow(212, parent.get());
  TestWindow* w213 = CreateTestWindow(213, parent.get());
  TestWindow* w22 = CreateTestWindow(22, parent.get());
  ASSERT_EQ(8u, parent->children().size());

  // w11 is now owned by w1.
  w1->AddTransientWindow(w11);
  // w21 is now owned by w2.
  w2->AddTransientWindow(w21);
  // w22 is now owned by w2.
  w2->AddTransientWindow(w22);
  // w211 is now owned by w21.
  w21->AddTransientWindow(w211);
  // w212 is now owned by w21.
  w21->AddTransientWindow(w212);
  // w213 is now owned by w21.
  w21->AddTransientWindow(w213);
  EXPECT_EQ("1 11 2 21 211 212 213 22", ChildWindowIDsAsString(parent.get()));

  // Stack w1 at the top (end), this should force w11 to be last (on top of w1).
  w1->MoveToFront();
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 21 211 212 213 22 1 11", ChildWindowIDsAsString(parent.get()));

  // This tests that the order in children_ array rather than in
  // transient_children_ array is used when reinserting transient children.
  // If transient_children_ array was used '22' would be following '21'.
  w2->MoveToFront();
  EXPECT_EQ(w22, parent->children().back());
  EXPECT_EQ("1 11 2 21 211 212 213 22", ChildWindowIDsAsString(parent.get()));

  w11->Reorder(w2.get(), mojom::OrderDirection::ABOVE);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 21 211 212 213 22 1 11", ChildWindowIDsAsString(parent.get()));

  w21->Reorder(w1.get(), mojom::OrderDirection::ABOVE);
  EXPECT_EQ(w22, parent->children().back());
  EXPECT_EQ("1 11 2 21 211 212 213 22", ChildWindowIDsAsString(parent.get()));

  w21->Reorder(w22, mojom::OrderDirection::ABOVE);
  EXPECT_EQ(w213, parent->children().back());
  EXPECT_EQ("1 11 2 22 21 211 212 213", ChildWindowIDsAsString(parent.get()));

  w11->Reorder(w21, mojom::OrderDirection::ABOVE);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 22 21 211 212 213 1 11", ChildWindowIDsAsString(parent.get()));

  w213->Reorder(w21, mojom::OrderDirection::ABOVE);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 22 21 213 211 212 1 11", ChildWindowIDsAsString(parent.get()));

  // No change when stacking a transient parent above its transient child.
  w21->Reorder(w211, mojom::OrderDirection::ABOVE);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 22 21 213 211 212 1 11", ChildWindowIDsAsString(parent.get()));

  // This tests that the order in children_ array rather than in
  // transient_children_ array is used when reinserting transient children.
  // If transient_children_ array was used '22' would be following '21'.
  w2->Reorder(w1.get(), mojom::OrderDirection::ABOVE);
  EXPECT_EQ(w212, parent->children().back());
  EXPECT_EQ("1 11 2 22 21 213 211 212", ChildWindowIDsAsString(parent.get()));

  w11->Reorder(w213, mojom::OrderDirection::ABOVE);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 22 21 213 211 212 1 11", ChildWindowIDsAsString(parent.get()));
}

// Tests that transient children are stacked as a unit when using order below.
TEST_F(WindowTest, TransientWindowsGroupBelow) {
  std::unique_ptr<TestWindow> parent(CreateTestWindow(0, nullptr));
  std::unique_ptr<TestWindow> w1(CreateTestWindow(1, parent.get()));

  TestWindow* w11 = CreateTestWindow(11, parent.get());
  std::unique_ptr<TestWindow> w2(CreateTestWindow(2, parent.get()));

  TestWindow* w21 = CreateTestWindow(21, parent.get());
  TestWindow* w211 = CreateTestWindow(211, parent.get());
  TestWindow* w212 = CreateTestWindow(212, parent.get());
  TestWindow* w213 = CreateTestWindow(213, parent.get());
  TestWindow* w22 = CreateTestWindow(22, parent.get());
  ASSERT_EQ(8u, parent->children().size());

  // w11 is now owned by w1.
  w1->AddTransientWindow(w11);
  // w21 is now owned by w2.
  w2->AddTransientWindow(w21);
  // w22 is now owned by w2.
  w2->AddTransientWindow(w22);
  // w211 is now owned by w21.
  w21->AddTransientWindow(w211);
  // w212 is now owned by w21.
  w21->AddTransientWindow(w212);
  // w213 is now owned by w21.
  w21->AddTransientWindow(w213);
  EXPECT_EQ("1 11 2 21 211 212 213 22", ChildWindowIDsAsString(parent.get()));

  // Stack w2 at the bottom, this should force w11 to be last (on top of w1).
  // This also tests that the order in children_ array rather than in
  // transient_children_ array is used when reinserting transient children.
  // If transient_children_ array was used '22' would be following '21'.
  w2->MoveToBack();
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 21 211 212 213 22 1 11", ChildWindowIDsAsString(parent.get()));

  w1->MoveToBack();
  EXPECT_EQ(w22, parent->children().back());
  EXPECT_EQ("1 11 2 21 211 212 213 22", ChildWindowIDsAsString(parent.get()));

  w21->Reorder(w1.get(), mojom::OrderDirection::BELOW);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 21 211 212 213 22 1 11", ChildWindowIDsAsString(parent.get()));

  w11->Reorder(w2.get(), mojom::OrderDirection::BELOW);
  EXPECT_EQ(w22, parent->children().back());
  EXPECT_EQ("1 11 2 21 211 212 213 22", ChildWindowIDsAsString(parent.get()));

  w22->Reorder(w21, mojom::OrderDirection::BELOW);
  EXPECT_EQ(w213, parent->children().back());
  EXPECT_EQ("1 11 2 22 21 211 212 213", ChildWindowIDsAsString(parent.get()));

  w21->Reorder(w11, mojom::OrderDirection::BELOW);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 22 21 211 212 213 1 11", ChildWindowIDsAsString(parent.get()));

  w213->Reorder(w211, mojom::OrderDirection::BELOW);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 22 21 213 211 212 1 11", ChildWindowIDsAsString(parent.get()));

  // No change when stacking a transient parent below its transient child.
  w21->Reorder(w211, mojom::OrderDirection::BELOW);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 22 21 213 211 212 1 11", ChildWindowIDsAsString(parent.get()));

  w1->Reorder(w2.get(), mojom::OrderDirection::BELOW);
  EXPECT_EQ(w212, parent->children().back());
  EXPECT_EQ("1 11 2 22 21 213 211 212", ChildWindowIDsAsString(parent.get()));

  w213->Reorder(w11, mojom::OrderDirection::BELOW);
  EXPECT_EQ(w11, parent->children().back());
  EXPECT_EQ("2 22 21 213 211 212 1 11", ChildWindowIDsAsString(parent.get()));
}

// Tests that windows are restacked properly after a call to
// AddTransientWindow() or RemoveTransientWindow).
TEST_F(WindowTest, RestackUponAddOrRemoveTransientWindow) {
  std::unique_ptr<TestWindow> parent(CreateTestWindow(0, nullptr));
  std::unique_ptr<TestWindow> windows[4];
  for (int i = 0; i < 4; i++)
    windows[i].reset(CreateTestWindow(i, parent.get()));

  EXPECT_EQ("0 1 2 3", ChildWindowIDsAsString(parent.get()));

  windows[0]->AddTransientWindow(windows[2].get());
  EXPECT_EQ("0 2 1 3", ChildWindowIDsAsString(parent.get()));

  windows[0]->AddTransientWindow(windows[3].get());
  EXPECT_EQ("0 2 3 1", ChildWindowIDsAsString(parent.get()));

  windows[0]->RemoveTransientWindow(windows[2].get());
  EXPECT_EQ("0 3 2 1", ChildWindowIDsAsString(parent.get()));

  windows[0]->RemoveTransientWindow(windows[3].get());
  EXPECT_EQ("0 3 2 1", ChildWindowIDsAsString(parent.get()));
}

// Tests that transient windows are stacked properly when created.
TEST_F(WindowTest, StackUponCreation) {
  std::unique_ptr<TestWindow> parent(CreateTestWindow(0, nullptr));
  std::unique_ptr<TestWindow> window0(CreateTestWindow(1, parent.get()));
  std::unique_ptr<TestWindow> window1(CreateTestWindow(2, parent.get()));

  TestWindow* window2 = CreateTestWindow(3, parent.get());

  window0->AddTransientWindow(window2);
  EXPECT_EQ("1 3 2", ChildWindowIDsAsString(parent.get()));
}

}  // namespace mus
