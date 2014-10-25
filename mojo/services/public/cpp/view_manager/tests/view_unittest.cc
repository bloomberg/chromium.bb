// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/view.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "mojo/services/public/cpp/view_manager/lib/view_private.h"
#include "mojo/services/public/cpp/view_manager/util.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {

// View ------------------------------------------------------------------------

typedef testing::Test ViewTest;

// Subclass with public ctor/dtor.
class TestView : public View {
 public:
  TestView() {
    ViewPrivate(this).set_id(1);
  }
  ~TestView() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestView);
};

TEST_F(ViewTest, AddChild) {
  TestView v1;
  TestView v11;
  v1.AddChild(&v11);
  EXPECT_EQ(1U, v1.children().size());
}

TEST_F(ViewTest, RemoveChild) {
  TestView v1;
  TestView v11;
  v1.AddChild(&v11);
  EXPECT_EQ(1U, v1.children().size());
  v1.RemoveChild(&v11);
  EXPECT_EQ(0U, v1.children().size());
}

TEST_F(ViewTest, Reparent) {
  TestView v1;
  TestView v2;
  TestView v11;
  v1.AddChild(&v11);
  EXPECT_EQ(1U, v1.children().size());
  v2.AddChild(&v11);
  EXPECT_EQ(1U, v2.children().size());
  EXPECT_EQ(0U, v1.children().size());
}

TEST_F(ViewTest, Contains) {
  TestView v1;

  // Direct descendant.
  TestView v11;
  v1.AddChild(&v11);
  EXPECT_TRUE(v1.Contains(&v11));

  // Indirect descendant.
  TestView v111;
  v11.AddChild(&v111);
  EXPECT_TRUE(v1.Contains(&v111));
}

TEST_F(ViewTest, GetChildById) {
  TestView v1;
  ViewPrivate(&v1).set_id(1);
  TestView v11;
  ViewPrivate(&v11).set_id(11);
  v1.AddChild(&v11);
  TestView v111;
  ViewPrivate(&v111).set_id(111);
  v11.AddChild(&v111);

  // Find direct & indirect descendents.
  EXPECT_EQ(&v11, v1.GetChildById(v11.id()));
  EXPECT_EQ(&v111, v1.GetChildById(v111.id()));
}

TEST_F(ViewTest, DrawnAndVisible) {
  TestView v1;
  EXPECT_TRUE(v1.visible());
  EXPECT_FALSE(v1.IsDrawn());

  ViewPrivate(&v1).set_drawn(true);

  TestView v11;
  v1.AddChild(&v11);
  EXPECT_TRUE(v11.visible());
  EXPECT_TRUE(v11.IsDrawn());

  v1.RemoveChild(&v11);
  EXPECT_TRUE(v11.visible());
  EXPECT_FALSE(v11.IsDrawn());
}

// ViewObserver --------------------------------------------------------

typedef testing::Test ViewObserverTest;

bool TreeChangeParamsMatch(const ViewObserver::TreeChangeParams& lhs,
                           const ViewObserver::TreeChangeParams& rhs) {
  return lhs.target == rhs.target &&  lhs.old_parent == rhs.old_parent &&
      lhs.new_parent == rhs.new_parent && lhs.receiver == rhs.receiver;
}

class TreeChangeObserver : public ViewObserver {
 public:
  explicit TreeChangeObserver(View* observee) : observee_(observee) {
    observee_->AddObserver(this);
  }
  ~TreeChangeObserver() override { observee_->RemoveObserver(this); }

  void Reset() {
    received_params_.clear();
  }

  const std::vector<TreeChangeParams>& received_params() {
    return received_params_;
  }

 private:
  // Overridden from ViewObserver:
  void OnTreeChanging(const TreeChangeParams& params) override {
     received_params_.push_back(params);
   }
   void OnTreeChanged(const TreeChangeParams& params) override {
    received_params_.push_back(params);
  }

  View* observee_;
  std::vector<TreeChangeParams> received_params_;

  DISALLOW_COPY_AND_ASSIGN(TreeChangeObserver);
};

// Adds/Removes v11 to v1.
TEST_F(ViewObserverTest, TreeChange_SimpleAddRemove) {
  TestView v1;
  TreeChangeObserver o1(&v1);
  EXPECT_TRUE(o1.received_params().empty());

  TestView v11;
  TreeChangeObserver o11(&v11);
  EXPECT_TRUE(o11.received_params().empty());

  // Add.

  v1.AddChild(&v11);

  EXPECT_EQ(2U, o1.received_params().size());
  ViewObserver::TreeChangeParams p1;
  p1.target = &v11;
  p1.receiver = &v1;
  p1.old_parent = NULL;
  p1.new_parent = &v1;
  EXPECT_TRUE(TreeChangeParamsMatch(p1, o1.received_params().back()));

  EXPECT_EQ(2U, o11.received_params().size());
  ViewObserver::TreeChangeParams p11 = p1;
  p11.receiver = &v11;
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().front()));
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().back()));

  o1.Reset();
  o11.Reset();
  EXPECT_TRUE(o1.received_params().empty());
  EXPECT_TRUE(o11.received_params().empty());

  // Remove.

  v1.RemoveChild(&v11);

  EXPECT_EQ(2U, o1.received_params().size());
  p1.target = &v11;
  p1.receiver = &v1;
  p1.old_parent = &v1;
  p1.new_parent = NULL;
  EXPECT_TRUE(TreeChangeParamsMatch(p1, o1.received_params().front()));

  EXPECT_EQ(2U, o11.received_params().size());
  p11 = p1;
  p11.receiver = &v11;
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().front()));
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().back()));
}

// Creates these two trees:
// v1
//  +- v11
// v111
//  +- v1111
//  +- v1112
// Then adds/removes v111 from v11.
TEST_F(ViewObserverTest, TreeChange_NestedAddRemove) {
  TestView v1, v11, v111, v1111, v1112;

  // Root tree.
  v1.AddChild(&v11);

  // Tree to be attached.
  v111.AddChild(&v1111);
  v111.AddChild(&v1112);

  TreeChangeObserver o1(&v1), o11(&v11), o111(&v111), o1111(&v1111),
      o1112(&v1112);
  ViewObserver::TreeChangeParams p1, p11, p111, p1111, p1112;

  // Add.

  v11.AddChild(&v111);

  EXPECT_EQ(2U, o1.received_params().size());
  p1.target = &v111;
  p1.receiver = &v1;
  p1.old_parent = NULL;
  p1.new_parent = &v11;
  EXPECT_TRUE(TreeChangeParamsMatch(p1, o1.received_params().back()));

  EXPECT_EQ(2U, o11.received_params().size());
  p11 = p1;
  p11.receiver = &v11;
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().back()));

  EXPECT_EQ(2U, o111.received_params().size());
  p111 = p11;
  p111.receiver = &v111;
  EXPECT_TRUE(TreeChangeParamsMatch(p111, o111.received_params().front()));
  EXPECT_TRUE(TreeChangeParamsMatch(p111, o111.received_params().back()));

  EXPECT_EQ(2U, o1111.received_params().size());
  p1111 = p111;
  p1111.receiver = &v1111;
  EXPECT_TRUE(TreeChangeParamsMatch(p1111, o1111.received_params().front()));
  EXPECT_TRUE(TreeChangeParamsMatch(p1111, o1111.received_params().back()));

  EXPECT_EQ(2U, o1112.received_params().size());
  p1112 = p111;
  p1112.receiver = &v1112;
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

  v11.RemoveChild(&v111);

  EXPECT_EQ(2U, o1.received_params().size());
  p1.target = &v111;
  p1.receiver = &v1;
  p1.old_parent = &v11;
  p1.new_parent = NULL;
  EXPECT_TRUE(TreeChangeParamsMatch(p1, o1.received_params().front()));

  EXPECT_EQ(2U, o11.received_params().size());
  p11 = p1;
  p11.receiver = &v11;
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().front()));

  EXPECT_EQ(2U, o111.received_params().size());
  p111 = p11;
  p111.receiver = &v111;
  EXPECT_TRUE(TreeChangeParamsMatch(p111, o111.received_params().front()));
  EXPECT_TRUE(TreeChangeParamsMatch(p111, o111.received_params().back()));

  EXPECT_EQ(2U, o1111.received_params().size());
  p1111 = p111;
  p1111.receiver = &v1111;
  EXPECT_TRUE(TreeChangeParamsMatch(p1111, o1111.received_params().front()));
  EXPECT_TRUE(TreeChangeParamsMatch(p1111, o1111.received_params().back()));

  EXPECT_EQ(2U, o1112.received_params().size());
  p1112 = p111;
  p1112.receiver = &v1112;
  EXPECT_TRUE(TreeChangeParamsMatch(p1112, o1112.received_params().front()));
  EXPECT_TRUE(TreeChangeParamsMatch(p1112, o1112.received_params().back()));
}

TEST_F(ViewObserverTest, TreeChange_Reparent) {
  TestView v1, v11, v12, v111;
  v1.AddChild(&v11);
  v1.AddChild(&v12);
  v11.AddChild(&v111);

  TreeChangeObserver o1(&v1), o11(&v11), o12(&v12), o111(&v111);

  // Reparent.
  v12.AddChild(&v111);

  // v1 (root) should see both changing and changed notifications.
  EXPECT_EQ(4U, o1.received_params().size());
  ViewObserver::TreeChangeParams p1;
  p1.target = &v111;
  p1.receiver = &v1;
  p1.old_parent = &v11;
  p1.new_parent = &v12;
  EXPECT_TRUE(TreeChangeParamsMatch(p1, o1.received_params().front()));
  EXPECT_TRUE(TreeChangeParamsMatch(p1, o1.received_params().back()));

  // v11 should see changing notifications.
  EXPECT_EQ(2U, o11.received_params().size());
  ViewObserver::TreeChangeParams p11;
  p11 = p1;
  p11.receiver = &v11;
  EXPECT_TRUE(TreeChangeParamsMatch(p11, o11.received_params().front()));

  // v12 should see changed notifications.
  EXPECT_EQ(2U, o12.received_params().size());
  ViewObserver::TreeChangeParams p12;
  p12 = p1;
  p12.receiver = &v12;
  EXPECT_TRUE(TreeChangeParamsMatch(p12, o12.received_params().back()));

  // v111 should see both changing and changed notifications.
  EXPECT_EQ(2U, o111.received_params().size());
  ViewObserver::TreeChangeParams p111;
  p111 = p1;
  p111.receiver = &v111;
  EXPECT_TRUE(TreeChangeParamsMatch(p111, o111.received_params().front()));
  EXPECT_TRUE(TreeChangeParamsMatch(p111, o111.received_params().back()));
}

namespace {

class OrderChangeObserver : public ViewObserver {
 public:
  struct Change {
    View* view;
    View* relative_view;
    OrderDirection direction;
  };
  typedef std::vector<Change> Changes;

  explicit OrderChangeObserver(View* observee) : observee_(observee) {
    observee_->AddObserver(this);
  }
  ~OrderChangeObserver() override { observee_->RemoveObserver(this); }

  Changes GetAndClearChanges() {
    Changes changes;
    changes_.swap(changes);
    return changes;
  }

 private:
  // Overridden from ViewObserver:
  void OnViewReordering(View* view,
                        View* relative_view,
                        OrderDirection direction) override {
    OnViewReordered(view, relative_view, direction);
  }

  void OnViewReordered(View* view,
                       View* relative_view,
                       OrderDirection direction) override {
    Change change;
    change.view = view;
    change.relative_view = relative_view;
    change.direction = direction;
    changes_.push_back(change);
  }

  View* observee_;
  Changes changes_;

  DISALLOW_COPY_AND_ASSIGN(OrderChangeObserver);
};

}  // namespace

TEST_F(ViewObserverTest, Order) {
  TestView v1, v11, v12, v13;
  v1.AddChild(&v11);
  v1.AddChild(&v12);
  v1.AddChild(&v13);

  // Order: v11, v12, v13
  EXPECT_EQ(3U, v1.children().size());
  EXPECT_EQ(&v11, v1.children().front());
  EXPECT_EQ(&v13, v1.children().back());

  {
    OrderChangeObserver observer(&v11);

    // Move v11 to front.
    // Resulting order: v12, v13, v11
    v11.MoveToFront();
    EXPECT_EQ(&v12, v1.children().front());
    EXPECT_EQ(&v11, v1.children().back());

    OrderChangeObserver::Changes changes = observer.GetAndClearChanges();
    ASSERT_EQ(2U, changes.size());
    EXPECT_EQ(&v11, changes[0].view);
    EXPECT_EQ(&v13, changes[0].relative_view);
    EXPECT_EQ(ORDER_DIRECTION_ABOVE, changes[0].direction);

    EXPECT_EQ(&v11, changes[1].view);
    EXPECT_EQ(&v13, changes[1].relative_view);
    EXPECT_EQ(ORDER_DIRECTION_ABOVE, changes[1].direction);
  }

  {
    OrderChangeObserver observer(&v11);

    // Move v11 to back.
    // Resulting order: v11, v12, v13
    v11.MoveToBack();
    EXPECT_EQ(&v11, v1.children().front());
    EXPECT_EQ(&v13, v1.children().back());

    OrderChangeObserver::Changes changes = observer.GetAndClearChanges();
    ASSERT_EQ(2U, changes.size());
    EXPECT_EQ(&v11, changes[0].view);
    EXPECT_EQ(&v12, changes[0].relative_view);
    EXPECT_EQ(ORDER_DIRECTION_BELOW, changes[0].direction);

    EXPECT_EQ(&v11, changes[1].view);
    EXPECT_EQ(&v12, changes[1].relative_view);
    EXPECT_EQ(ORDER_DIRECTION_BELOW, changes[1].direction);
  }

  {
    OrderChangeObserver observer(&v11);

    // Move v11 above v12.
    // Resulting order: v12. v11, v13
    v11.Reorder(&v12, ORDER_DIRECTION_ABOVE);
    EXPECT_EQ(&v12, v1.children().front());
    EXPECT_EQ(&v13, v1.children().back());

    OrderChangeObserver::Changes changes = observer.GetAndClearChanges();
    ASSERT_EQ(2U, changes.size());
    EXPECT_EQ(&v11, changes[0].view);
    EXPECT_EQ(&v12, changes[0].relative_view);
    EXPECT_EQ(ORDER_DIRECTION_ABOVE, changes[0].direction);

    EXPECT_EQ(&v11, changes[1].view);
    EXPECT_EQ(&v12, changes[1].relative_view);
    EXPECT_EQ(ORDER_DIRECTION_ABOVE, changes[1].direction);
  }

  {
    OrderChangeObserver observer(&v11);

    // Move v11 below v12.
    // Resulting order: v11, v12, v13
    v11.Reorder(&v12, ORDER_DIRECTION_BELOW);
    EXPECT_EQ(&v11, v1.children().front());
    EXPECT_EQ(&v13, v1.children().back());

    OrderChangeObserver::Changes changes = observer.GetAndClearChanges();
    ASSERT_EQ(2U, changes.size());
    EXPECT_EQ(&v11, changes[0].view);
    EXPECT_EQ(&v12, changes[0].relative_view);
    EXPECT_EQ(ORDER_DIRECTION_BELOW, changes[0].direction);

    EXPECT_EQ(&v11, changes[1].view);
    EXPECT_EQ(&v12, changes[1].relative_view);
    EXPECT_EQ(ORDER_DIRECTION_BELOW, changes[1].direction);
  }
}

namespace {

typedef std::vector<std::string> Changes;

std::string ViewIdToString(Id id) {
  return (id == 0) ? "null" :
      base::StringPrintf("%d,%d", HiWord(id), LoWord(id));
}

std::string RectToString(const Rect& rect) {
  return base::StringPrintf("%d,%d %dx%d",
                            rect.x, rect.y, rect.width, rect.height);
}

class BoundsChangeObserver : public ViewObserver {
 public:
  explicit BoundsChangeObserver(View* view) : view_(view) {
    view_->AddObserver(this);
  }
  ~BoundsChangeObserver() override { view_->RemoveObserver(this); }

  Changes GetAndClearChanges() {
    Changes changes;
    changes.swap(changes_);
    return changes;
  }

 private:
  // Overridden from ViewObserver:
  void OnViewBoundsChanging(View* view,
                            const Rect& old_bounds,
                            const Rect& new_bounds) override {
    changes_.push_back(
        base::StringPrintf(
            "view=%s old_bounds=%s new_bounds=%s phase=changing",
            ViewIdToString(view->id()).c_str(),
            RectToString(old_bounds).c_str(),
            RectToString(new_bounds).c_str()));
  }
  void OnViewBoundsChanged(View* view,
                           const Rect& old_bounds,
                           const Rect& new_bounds) override {
    changes_.push_back(
        base::StringPrintf(
            "view=%s old_bounds=%s new_bounds=%s phase=changed",
            ViewIdToString(view->id()).c_str(),
            RectToString(old_bounds).c_str(),
            RectToString(new_bounds).c_str()));
  }

  View* view_;
  Changes changes_;

  DISALLOW_COPY_AND_ASSIGN(BoundsChangeObserver);
};

}  // namespace

TEST_F(ViewObserverTest, SetBounds) {
  TestView v1;
  {
    BoundsChangeObserver observer(&v1);
    Rect rect;
    rect.width = rect.height = 100;
    v1.SetBounds(rect);

    Changes changes = observer.GetAndClearChanges();
    ASSERT_EQ(2U, changes.size());
    EXPECT_EQ(
        "view=0,1 old_bounds=0,0 0x0 new_bounds=0,0 100x100 phase=changing",
        changes[0]);
    EXPECT_EQ(
        "view=0,1 old_bounds=0,0 0x0 new_bounds=0,0 100x100 phase=changed",
        changes[1]);
  }
}

namespace {

class VisibilityChangeObserver : public ViewObserver {
 public:
  explicit VisibilityChangeObserver(View* view) : view_(view) {
    view_->AddObserver(this);
  }
  ~VisibilityChangeObserver() override { view_->RemoveObserver(this); }

  Changes GetAndClearChanges() {
    Changes changes;
    changes.swap(changes_);
    return changes;
  }

 private:
  // Overridden from ViewObserver:
  void OnViewVisibilityChanging(View* view) override {
    changes_.push_back(
        base::StringPrintf("view=%s phase=changing visibility=%s",
                           ViewIdToString(view->id()).c_str(),
                           view->visible() ? "true" : "false"));
  }
  void OnViewVisibilityChanged(View* view) override {
    changes_.push_back(base::StringPrintf("view=%s phase=changed visibility=%s",
                                          ViewIdToString(view->id()).c_str(),
                                          view->visible() ? "true" : "false"));
  }

  View* view_;
  Changes changes_;

  DISALLOW_COPY_AND_ASSIGN(VisibilityChangeObserver);
};

}  // namespace

TEST_F(ViewObserverTest, SetVisible) {
  TestView v1;
  EXPECT_TRUE(v1.visible());
  {
    // Change visibility from true to false and make sure we get notifications.
    VisibilityChangeObserver observer(&v1);
    v1.SetVisible(false);

    Changes changes = observer.GetAndClearChanges();
    ASSERT_EQ(2U, changes.size());
    EXPECT_EQ("view=0,1 phase=changing visibility=true", changes[0]);
    EXPECT_EQ("view=0,1 phase=changed visibility=false", changes[1]);
  }
  {
    // Set visible to existing value and verify no notifications.
    VisibilityChangeObserver observer(&v1);
    v1.SetVisible(false);
    EXPECT_TRUE(observer.GetAndClearChanges().empty());
  }
}

namespace {

class PropertyChangeObserver : public ViewObserver {
 public:
  explicit PropertyChangeObserver(View* view) : view_(view) {
    view_->AddObserver(this);
  }
  virtual ~PropertyChangeObserver() { view_->RemoveObserver(this); }

  Changes GetAndClearChanges() {
    Changes changes;
    changes.swap(changes_);
    return changes;
  }

 private:
  // Overridden from ViewObserver:
  void OnViewPropertyChanged(View* view,
                             const std::string& name,
                             const std::vector<uint8_t>* old_data,
                             const std::vector<uint8_t>* new_data) override {
    changes_.push_back(base::StringPrintf(
        "view=%s property changed key=%s old_value=%s new_value=%s",
        ViewIdToString(view->id()).c_str(),
        name.c_str(),
        VectorToString(old_data).c_str(),
        VectorToString(new_data).c_str()));
  }

  std::string VectorToString(const std::vector<uint8_t>* data) {
    if (!data)
      return "NULL";
    std::string s;
    for (char c : *data)
      s += c;
    return s;
  }

  View* view_;
  Changes changes_;

  DISALLOW_COPY_AND_ASSIGN(PropertyChangeObserver);
};

}  // namespace

TEST_F(ViewObserverTest, SetProperty) {
  TestView v1;
  std::vector<uint8_t> one(1, '1');

  {
    // Change visibility from true to false and make sure we get notifications.
    PropertyChangeObserver observer(&v1);
    v1.SetProperty("one", &one);
    Changes changes = observer.GetAndClearChanges();
    ASSERT_EQ(1U, changes.size());
    EXPECT_EQ("view=0,1 property changed key=one old_value=NULL new_value=1",
              changes[0]);
    EXPECT_EQ(1U, v1.properties().size());
  }
  {
    // Set visible to existing value and verify no notifications.
    PropertyChangeObserver observer(&v1);
    v1.SetProperty("one", &one);
    EXPECT_TRUE(observer.GetAndClearChanges().empty());
    EXPECT_EQ(1U, v1.properties().size());
  }
  {
    // Set the value to NULL to delete it.
    // Change visibility from true to false and make sure we get notifications.
    PropertyChangeObserver observer(&v1);
    v1.SetProperty("one", NULL);
    Changes changes = observer.GetAndClearChanges();
    ASSERT_EQ(1U, changes.size());
    EXPECT_EQ("view=0,1 property changed key=one old_value=1 new_value=NULL",
              changes[0]);
    EXPECT_EQ(0U, v1.properties().size());
  }
  {
    // Setting a null property to null shouldn't update us.
    PropertyChangeObserver observer(&v1);
    v1.SetProperty("one", NULL);
    EXPECT_TRUE(observer.GetAndClearChanges().empty());
    EXPECT_EQ(0U, v1.properties().size());
  }
}

}  // namespace mojo
