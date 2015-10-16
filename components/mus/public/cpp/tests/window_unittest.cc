// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/window.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "components/mus/public/cpp/lib/window_private.h"
#include "components/mus/public/cpp/util.h"
#include "components/mus/public/cpp/window_observer.h"
#include "components/mus/public/cpp/window_property.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mus {

// Window ---------------------------------------------------------------------

typedef testing::Test WindowTest;

// Subclass with public ctor/dtor.
class TestWindow : public Window {
 public:
  TestWindow() { WindowPrivate(this).set_id(1); }
  ~TestWindow() {}

 private:
  MOJO_DISALLOW_COPY_AND_ASSIGN(TestWindow);
};

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

TEST_F(WindowTest, GetChildById) {
  TestWindow w1;
  WindowPrivate(&w1).set_id(1);
  TestWindow w11;
  WindowPrivate(&w11).set_id(11);
  w1.AddChild(&w11);
  TestWindow w111;
  WindowPrivate(&w111).set_id(111);
  w11.AddChild(&w111);

  // Find direct & indirect descendents.
  EXPECT_EQ(&w11, w1.GetChildById(w11.id()));
  EXPECT_EQ(&w111, w1.GetChildById(w111.id()));
}

TEST_F(WindowTest, DrawnAndVisible) {
  TestWindow w1;
  EXPECT_TRUE(w1.visible());
  EXPECT_FALSE(w1.IsDrawn());

  WindowPrivate(&w1).set_drawn(true);

  TestWindow w11;
  w1.AddChild(&w11);
  EXPECT_TRUE(w11.visible());
  EXPECT_TRUE(w11.IsDrawn());

  w1.RemoveChild(&w11);
  EXPECT_TRUE(w11.visible());
  EXPECT_FALSE(w11.IsDrawn());
}

namespace {
DEFINE_WINDOW_PROPERTY_KEY(int, kIntKey, -2);
DEFINE_WINDOW_PROPERTY_KEY(const char*, kStringKey, "squeamish");
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
  MOJO_DISALLOW_COPY_AND_ASSIGN(TestProperty);
};

TestProperty* TestProperty::last_deleted_ = NULL;

DEFINE_OWNED_WINDOW_PROPERTY_KEY(TestProperty, kOwnedKey, NULL);

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

  MOJO_DISALLOW_COPY_AND_ASSIGN(TreeChangeObserver);
};

// Adds/Removes w11 to w1.
TEST_F(WindowObserverTest, TreeChange_SimpleAddRemove) {
  TestWindow w1;
  TreeChangeObserver o1(&w1);
  EXPECT_TRUE(o1.received_params().empty());

  TestWindow w11;
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
    mojo::OrderDirection direction;
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
                          mojo::OrderDirection direction) override {
    OnWindowReordered(window, relative_window, direction);
  }

  void OnWindowReordered(Window* window,
                         Window* relative_window,
                         mojo::OrderDirection direction) override {
    Change change;
    change.window = window;
    change.relative_window = relative_window;
    change.direction = direction;
    changes_.push_back(change);
  }

  Window* observee_;
  Changes changes_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(OrderChangeObserver);
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
    EXPECT_EQ(mojo::ORDER_DIRECTION_ABOVE, changes[0].direction);

    EXPECT_EQ(&w11, changes[1].window);
    EXPECT_EQ(&w13, changes[1].relative_window);
    EXPECT_EQ(mojo::ORDER_DIRECTION_ABOVE, changes[1].direction);
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
    EXPECT_EQ(mojo::ORDER_DIRECTION_BELOW, changes[0].direction);

    EXPECT_EQ(&w11, changes[1].window);
    EXPECT_EQ(&w12, changes[1].relative_window);
    EXPECT_EQ(mojo::ORDER_DIRECTION_BELOW, changes[1].direction);
  }

  {
    OrderChangeObserver observer(&w11);

    // Move w11 above w12.
    // Resulting order: w12. w11, w13
    w11.Reorder(&w12, mojo::ORDER_DIRECTION_ABOVE);
    EXPECT_EQ(&w12, w1.children().front());
    EXPECT_EQ(&w13, w1.children().back());

    OrderChangeObserver::Changes changes = observer.GetAndClearChanges();
    ASSERT_EQ(2U, changes.size());
    EXPECT_EQ(&w11, changes[0].window);
    EXPECT_EQ(&w12, changes[0].relative_window);
    EXPECT_EQ(mojo::ORDER_DIRECTION_ABOVE, changes[0].direction);

    EXPECT_EQ(&w11, changes[1].window);
    EXPECT_EQ(&w12, changes[1].relative_window);
    EXPECT_EQ(mojo::ORDER_DIRECTION_ABOVE, changes[1].direction);
  }

  {
    OrderChangeObserver observer(&w11);

    // Move w11 below w12.
    // Resulting order: w11, w12, w13
    w11.Reorder(&w12, mojo::ORDER_DIRECTION_BELOW);
    EXPECT_EQ(&w11, w1.children().front());
    EXPECT_EQ(&w13, w1.children().back());

    OrderChangeObserver::Changes changes = observer.GetAndClearChanges();
    ASSERT_EQ(2U, changes.size());
    EXPECT_EQ(&w11, changes[0].window);
    EXPECT_EQ(&w12, changes[0].relative_window);
    EXPECT_EQ(mojo::ORDER_DIRECTION_BELOW, changes[0].direction);

    EXPECT_EQ(&w11, changes[1].window);
    EXPECT_EQ(&w12, changes[1].relative_window);
    EXPECT_EQ(mojo::ORDER_DIRECTION_BELOW, changes[1].direction);
  }
}

namespace {

typedef std::vector<std::string> Changes;

std::string WindowIdToString(Id id) {
  return (id == 0) ? "null"
                   : base::StringPrintf("%d,%d", HiWord(id), LoWord(id));
}

std::string RectToString(const mojo::Rect& rect) {
  return base::StringPrintf("%d,%d %dx%d", rect.x, rect.y, rect.width,
                            rect.height);
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
                              const mojo::Rect& old_bounds,
                              const mojo::Rect& new_bounds) override {
    changes_.push_back(base::StringPrintf(
        "window=%s old_bounds=%s new_bounds=%s phase=changing",
        WindowIdToString(window->id()).c_str(),
        RectToString(old_bounds).c_str(), RectToString(new_bounds).c_str()));
  }
  void OnWindowBoundsChanged(Window* window,
                             const mojo::Rect& old_bounds,
                             const mojo::Rect& new_bounds) override {
    changes_.push_back(base::StringPrintf(
        "window=%s old_bounds=%s new_bounds=%s phase=changed",
        WindowIdToString(window->id()).c_str(),
        RectToString(old_bounds).c_str(), RectToString(new_bounds).c_str()));
  }

  Window* window_;
  Changes changes_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(BoundsChangeObserver);
};

}  // namespace

TEST_F(WindowObserverTest, SetBounds) {
  TestWindow w1;
  {
    BoundsChangeObserver observer(&w1);
    mojo::Rect rect;
    rect.width = rect.height = 100;
    w1.SetBounds(rect);

    Changes changes = observer.GetAndClearChanges();
    ASSERT_EQ(2U, changes.size());
    EXPECT_EQ(
        "window=0,1 old_bounds=0,0 0x0 new_bounds=0,0 100x100 phase=changing",
        changes[0]);
    EXPECT_EQ(
        "window=0,1 old_bounds=0,0 0x0 new_bounds=0,0 100x100 phase=changed",
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
    changes_.push_back(
        base::StringPrintf("window=%s phase=changing wisibility=%s",
                           WindowIdToString(window->id()).c_str(),
                           window->visible() ? "true" : "false"));
  }
  void OnWindowVisibilityChanged(Window* window) override {
    changes_.push_back(
        base::StringPrintf("window=%s phase=changed wisibility=%s",
                           WindowIdToString(window->id()).c_str(),
                           window->visible() ? "true" : "false"));
  }

  Window* window_;
  Changes changes_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(VisibilityChangeObserver);
};

}  // namespace

TEST_F(WindowObserverTest, SetVisible) {
  TestWindow w1;
  EXPECT_TRUE(w1.visible());
  {
    // Change wisibility from true to false and make sure we get notifications.
    VisibilityChangeObserver observer(&w1);
    w1.SetVisible(false);

    Changes changes = observer.GetAndClearChanges();
    ASSERT_EQ(2U, changes.size());
    EXPECT_EQ("window=0,1 phase=changing wisibility=true", changes[0]);
    EXPECT_EQ("window=0,1 phase=changed wisibility=false", changes[1]);
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
  WindowPrivate(&parent).set_id(1);
  TestWindow child;
  WindowPrivate(&child).set_id(2);
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
    EXPECT_EQ("window=0,2 phase=changed wisibility=false", changes[0]);
  }
}

TEST_F(WindowObserverTest, SetVisibleChild) {
  TestWindow parent;
  WindowPrivate(&parent).set_id(1);
  TestWindow child;
  WindowPrivate(&child).set_id(2);
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
    EXPECT_EQ("window=0,1 phase=changed wisibility=false", changes[0]);
  }
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
        "window=%s shared property changed key=%s old_value=%s new_value=%s",
        WindowIdToString(window->id()).c_str(), name.c_str(),
        VectorToString(old_data).c_str(), VectorToString(new_data).c_str()));
  }

  std::string VectorToString(const std::vector<uint8_t>* data) {
    if (!data)
      return "NULL";
    std::string s;
    for (char c : *data)
      s += c;
    return s;
  }

  Window* window_;
  Changes changes_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(SharedPropertyChangeObserver);
};

}  // namespace

TEST_F(WindowObserverTest, SetLocalProperty) {
  TestWindow w1;
  std::vector<uint8_t> one(1, '1');

  {
    // Change wisibility from true to false and make sure we get notifications.
    SharedPropertyChangeObserver observer(&w1);
    w1.SetSharedProperty("one", &one);
    Changes changes = observer.GetAndClearChanges();
    ASSERT_EQ(1U, changes.size());
    EXPECT_EQ(
        "window=0,1 shared property changed key=one old_value=NULL new_value=1",
        changes[0]);
    EXPECT_EQ(1U, w1.shared_properties().size());
  }
  {
    // Set visible to existing walue and werify no notifications.
    SharedPropertyChangeObserver observer(&w1);
    w1.SetSharedProperty("one", &one);
    EXPECT_TRUE(observer.GetAndClearChanges().empty());
    EXPECT_EQ(1U, w1.shared_properties().size());
  }
  {
    // Set the walue to NULL to delete it.
    // Change wisibility from true to false and make sure we get notifications.
    SharedPropertyChangeObserver observer(&w1);
    w1.SetSharedProperty("one", NULL);
    Changes changes = observer.GetAndClearChanges();
    ASSERT_EQ(1U, changes.size());
    EXPECT_EQ(
        "window=0,1 shared property changed key=one old_value=1 new_value=NULL",
        changes[0]);
    EXPECT_EQ(0U, w1.shared_properties().size());
  }
  {
    // Setting a null property to null shouldn't update us.
    SharedPropertyChangeObserver observer(&w1);
    w1.SetSharedProperty("one", NULL);
    EXPECT_TRUE(observer.GetAndClearChanges().empty());
    EXPECT_EQ(0U, w1.shared_properties().size());
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

  MOJO_DISALLOW_COPY_AND_ASSIGN(LocalPropertyChangeObserver);
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

}  // namespace mus
