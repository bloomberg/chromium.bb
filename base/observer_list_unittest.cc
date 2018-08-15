// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/observer_list.h"

#include "base/strings/string_piece.h"
#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

class Foo {
 public:
  virtual void Observe(int x) = 0;
  virtual ~Foo() = default;
  virtual int GetValue() const { return 0; }
};

class Adder : public Foo {
 public:
  explicit Adder(int scaler) : total(0), scaler_(scaler) {}
  ~Adder() override = default;

  void Observe(int x) override { total += x * scaler_; }
  int GetValue() const override { return total; }

  int total;

 private:
  int scaler_;
};

class Disrupter : public Foo {
 public:
  Disrupter(ObserverList<Foo>* list, Foo* doomed, bool remove_self)
      : list_(list), doomed_(doomed), remove_self_(remove_self) {}
  Disrupter(ObserverList<Foo>* list, Foo* doomed)
      : Disrupter(list, doomed, false) {}
  Disrupter(ObserverList<Foo>* list, bool remove_self)
      : Disrupter(list, nullptr, remove_self) {}

  ~Disrupter() override = default;

  void Observe(int x) override {
    if (remove_self_)
      list_->RemoveObserver(this);
    if (doomed_)
      list_->RemoveObserver(doomed_);
  }

  void SetDoomed(Foo* doomed) { doomed_ = doomed; }

 private:
  ObserverList<Foo>* list_;
  Foo* doomed_;
  bool remove_self_;
};

class AddInObserve : public Foo {
 public:
  explicit AddInObserve(ObserverList<Foo>* observer_list)
      : observer_list(observer_list), to_add_() {}

  void SetToAdd(Foo* to_add) { to_add_ = to_add; }

  void Observe(int x) override {
    if (to_add_) {
      observer_list->AddObserver(to_add_);
      to_add_ = nullptr;
    }
  }

  ObserverList<Foo>* observer_list;
  Foo* to_add_;
};

}  // namespace

TEST(ObserverListTest, BasicTest) {
  ObserverList<Foo> observer_list;
  const ObserverList<Foo>& const_observer_list = observer_list;

  {
    const ObserverList<Foo>::const_iterator it1 = const_observer_list.begin();
    EXPECT_EQ(it1, const_observer_list.end());
    // Iterator copy.
    const ObserverList<Foo>::const_iterator it2 = it1;
    EXPECT_EQ(it2, it1);
    // Iterator assignment.
    ObserverList<Foo>::const_iterator it3;
    it3 = it2;
    EXPECT_EQ(it3, it1);
    EXPECT_EQ(it3, it2);
    // Self assignment.
    it3 = *&it3;  // The *& defeats Clang's -Wself-assign warning.
    EXPECT_EQ(it3, it1);
    EXPECT_EQ(it3, it2);
  }

  {
    const ObserverList<Foo>::iterator it1 = observer_list.begin();
    EXPECT_EQ(it1, observer_list.end());
    // Iterator copy.
    const ObserverList<Foo>::iterator it2 = it1;
    EXPECT_EQ(it2, it1);
    // Iterator assignment.
    ObserverList<Foo>::iterator it3;
    it3 = it2;
    EXPECT_EQ(it3, it1);
    EXPECT_EQ(it3, it2);
    // Self assignment.
    it3 = *&it3;  // The *& defeats Clang's -Wself-assign warning.
    EXPECT_EQ(it3, it1);
    EXPECT_EQ(it3, it2);
  }

  Adder a(1), b(-1), c(1), d(-1), e(-1);
  Disrupter evil(&observer_list, &c);

  observer_list.AddObserver(&a);
  observer_list.AddObserver(&b);

  EXPECT_TRUE(const_observer_list.HasObserver(&a));
  EXPECT_FALSE(const_observer_list.HasObserver(&c));

  {
    const ObserverList<Foo>::const_iterator it1 = const_observer_list.begin();
    EXPECT_NE(it1, const_observer_list.end());
    // Iterator copy.
    const ObserverList<Foo>::const_iterator it2 = it1;
    EXPECT_EQ(it2, it1);
    EXPECT_NE(it2, const_observer_list.end());
    // Iterator assignment.
    ObserverList<Foo>::const_iterator it3;
    it3 = it2;
    EXPECT_EQ(it3, it1);
    EXPECT_EQ(it3, it2);
    // Self assignment.
    it3 = *&it3;  // The *& defeats Clang's -Wself-assign warning.
    EXPECT_EQ(it3, it1);
    EXPECT_EQ(it3, it2);
    // Iterator post increment.
    ObserverList<Foo>::const_iterator it4 = it3++;
    EXPECT_EQ(it4, it1);
    EXPECT_EQ(it4, it2);
    EXPECT_NE(it4, it3);
  }

  {
    const ObserverList<Foo>::iterator it1 = observer_list.begin();
    EXPECT_NE(it1, observer_list.end());
    // Iterator copy.
    const ObserverList<Foo>::iterator it2 = it1;
    EXPECT_EQ(it2, it1);
    EXPECT_NE(it2, observer_list.end());
    // Iterator assignment.
    ObserverList<Foo>::iterator it3;
    it3 = it2;
    EXPECT_EQ(it3, it1);
    EXPECT_EQ(it3, it2);
    // Self assignment.
    it3 = *&it3;  // The *& defeats Clang's -Wself-assign warning.
    EXPECT_EQ(it3, it1);
    EXPECT_EQ(it3, it2);
    // Iterator post increment.
    ObserverList<Foo>::iterator it4 = it3++;
    EXPECT_EQ(it4, it1);
    EXPECT_EQ(it4, it2);
    EXPECT_NE(it4, it3);
  }

  for (auto& observer : observer_list)
    observer.Observe(10);

  observer_list.AddObserver(&evil);
  observer_list.AddObserver(&c);
  observer_list.AddObserver(&d);

  // Removing an observer not in the list should do nothing.
  observer_list.RemoveObserver(&e);

  for (auto& observer : observer_list)
    observer.Observe(10);

  EXPECT_EQ(20, a.total);
  EXPECT_EQ(-20, b.total);
  EXPECT_EQ(0, c.total);
  EXPECT_EQ(-10, d.total);
  EXPECT_EQ(0, e.total);
}

TEST(ObserverListTest, CompactsWhenNoActiveIterator) {
  ObserverList<const Foo> ol;
  const ObserverList<const Foo>& col = ol;

  const Adder a(1);
  const Adder b(2);
  const Adder c(3);

  ol.AddObserver(&a);
  ol.AddObserver(&b);

  EXPECT_TRUE(col.HasObserver(&a));
  EXPECT_FALSE(col.HasObserver(&c));

  EXPECT_TRUE(col.might_have_observers());

  using It = ObserverList<const Foo>::const_iterator;

  {
    It it = col.begin();
    EXPECT_NE(it, col.end());
    It ita = it;
    EXPECT_EQ(ita, it);
    EXPECT_NE(++it, col.end());
    EXPECT_NE(ita, it);
    It itb = it;
    EXPECT_EQ(itb, it);
    EXPECT_EQ(++it, col.end());

    EXPECT_TRUE(col.might_have_observers());
    EXPECT_EQ(&*ita, &a);
    EXPECT_EQ(&*itb, &b);

    ol.RemoveObserver(&a);
    EXPECT_TRUE(col.might_have_observers());
    EXPECT_FALSE(col.HasObserver(&a));
    EXPECT_EQ(&*itb, &b);

    ol.RemoveObserver(&b);
    EXPECT_TRUE(col.might_have_observers());
    EXPECT_FALSE(col.HasObserver(&a));
    EXPECT_FALSE(col.HasObserver(&b));

    it = It();
    ita = It();
    EXPECT_TRUE(col.might_have_observers());
    ita = itb;
    itb = It();
    EXPECT_TRUE(col.might_have_observers());
    ita = It();
    EXPECT_FALSE(col.might_have_observers());
  }

  ol.AddObserver(&a);
  ol.AddObserver(&b);
  EXPECT_TRUE(col.might_have_observers());
  ol.Clear();
  EXPECT_FALSE(col.might_have_observers());

  ol.AddObserver(&a);
  ol.AddObserver(&b);
  EXPECT_TRUE(col.might_have_observers());
  {
    const It it = col.begin();
    ol.Clear();
    EXPECT_TRUE(col.might_have_observers());
  }
  EXPECT_FALSE(col.might_have_observers());
}

TEST(ObserverListTest, DisruptSelf) {
  ObserverList<Foo> observer_list;
  Adder a(1), b(-1), c(1), d(-1);
  Disrupter evil(&observer_list, true);

  observer_list.AddObserver(&a);
  observer_list.AddObserver(&b);

  for (auto& observer : observer_list)
    observer.Observe(10);

  observer_list.AddObserver(&evil);
  observer_list.AddObserver(&c);
  observer_list.AddObserver(&d);

  for (auto& observer : observer_list)
    observer.Observe(10);

  EXPECT_EQ(20, a.total);
  EXPECT_EQ(-20, b.total);
  EXPECT_EQ(10, c.total);
  EXPECT_EQ(-10, d.total);
}

TEST(ObserverListTest, DisruptBefore) {
  ObserverList<Foo> observer_list;
  Adder a(1), b(-1), c(1), d(-1);
  Disrupter evil(&observer_list, &b);

  observer_list.AddObserver(&a);
  observer_list.AddObserver(&b);
  observer_list.AddObserver(&evil);
  observer_list.AddObserver(&c);
  observer_list.AddObserver(&d);

  for (auto& observer : observer_list)
    observer.Observe(10);
  for (auto& observer : observer_list)
    observer.Observe(10);

  EXPECT_EQ(20, a.total);
  EXPECT_EQ(-10, b.total);
  EXPECT_EQ(20, c.total);
  EXPECT_EQ(-20, d.total);
}

TEST(ObserverListTest, Existing) {
  ObserverList<Foo> observer_list(ObserverListPolicy::EXISTING_ONLY);
  Adder a(1);
  AddInObserve b(&observer_list);
  Adder c(1);
  b.SetToAdd(&c);

  observer_list.AddObserver(&a);
  observer_list.AddObserver(&b);

  for (auto& observer : observer_list)
    observer.Observe(1);

  EXPECT_FALSE(b.to_add_);
  // B's adder should not have been notified because it was added during
  // notification.
  EXPECT_EQ(0, c.total);

  // Notify again to make sure b's adder is notified.
  for (auto& observer : observer_list)
    observer.Observe(1);
  EXPECT_EQ(1, c.total);
}

class AddInClearObserve : public Foo {
 public:
  explicit AddInClearObserve(ObserverList<Foo>* list)
      : list_(list), added_(false), adder_(1) {}

  void Observe(int /* x */) override {
    list_->Clear();
    list_->AddObserver(&adder_);
    added_ = true;
  }

  bool added() const { return added_; }
  const Adder& adder() const { return adder_; }

 private:
  ObserverList<Foo>* const list_;

  bool added_;
  Adder adder_;
};

TEST(ObserverListTest, ClearNotifyAll) {
  ObserverList<Foo> observer_list;
  AddInClearObserve a(&observer_list);

  observer_list.AddObserver(&a);

  for (auto& observer : observer_list)
    observer.Observe(1);
  EXPECT_TRUE(a.added());
  EXPECT_EQ(1, a.adder().total)
      << "Adder should observe once and have sum of 1.";
}

TEST(ObserverListTest, ClearNotifyExistingOnly) {
  ObserverList<Foo> observer_list(ObserverListPolicy::EXISTING_ONLY);
  AddInClearObserve a(&observer_list);

  observer_list.AddObserver(&a);

  for (auto& observer : observer_list)
    observer.Observe(1);
  EXPECT_TRUE(a.added());
  EXPECT_EQ(0, a.adder().total)
      << "Adder should not observe, so sum should still be 0.";
}

class ListDestructor : public Foo {
 public:
  explicit ListDestructor(ObserverList<Foo>* list) : list_(list) {}
  ~ListDestructor() override = default;

  void Observe(int x) override { delete list_; }

 private:
  ObserverList<Foo>* list_;
};


TEST(ObserverListTest, IteratorOutlivesList) {
  ObserverList<Foo>* observer_list = new ObserverList<Foo>;
  ListDestructor a(observer_list);
  observer_list->AddObserver(&a);

  for (auto& observer : *observer_list)
    observer.Observe(0);

  // There are no EXPECT* statements for this test, if we catch
  // use-after-free errors for observer_list (eg with ASan) then
  // this test has failed.  See http://crbug.com/85296.
}

TEST(ObserverListTest, BasicStdIterator) {
  using FooList = ObserverList<Foo>;
  FooList observer_list;

  // An optimization: begin() and end() do not involve weak pointers on
  // empty list.
  EXPECT_FALSE(observer_list.begin().list_);
  EXPECT_FALSE(observer_list.end().list_);

  // Iterate over empty list: no effect, no crash.
  for (auto& i : observer_list)
    i.Observe(10);

  Adder a(1), b(-1), c(1), d(-1);

  observer_list.AddObserver(&a);
  observer_list.AddObserver(&b);
  observer_list.AddObserver(&c);
  observer_list.AddObserver(&d);

  for (FooList::iterator i = observer_list.begin(), e = observer_list.end();
       i != e; ++i)
    i->Observe(1);

  EXPECT_EQ(1, a.total);
  EXPECT_EQ(-1, b.total);
  EXPECT_EQ(1, c.total);
  EXPECT_EQ(-1, d.total);

  // Check an iteration over a 'const view' for a given container.
  const FooList& const_list = observer_list;
  for (FooList::const_iterator i = const_list.begin(), e = const_list.end();
       i != e; ++i) {
    EXPECT_EQ(1, std::abs(i->GetValue()));
  }

  for (const auto& o : const_list)
    EXPECT_EQ(1, std::abs(o.GetValue()));
}

TEST(ObserverListTest, StdIteratorRemoveItself) {
  ObserverList<Foo> observer_list;
  Adder a(1), b(-1), c(1), d(-1);
  Disrupter disrupter(&observer_list, true);

  observer_list.AddObserver(&a);
  observer_list.AddObserver(&b);
  observer_list.AddObserver(&disrupter);
  observer_list.AddObserver(&c);
  observer_list.AddObserver(&d);

  for (auto& o : observer_list)
    o.Observe(1);

  for (auto& o : observer_list)
    o.Observe(10);

  EXPECT_EQ(11, a.total);
  EXPECT_EQ(-11, b.total);
  EXPECT_EQ(11, c.total);
  EXPECT_EQ(-11, d.total);
}

TEST(ObserverListTest, StdIteratorRemoveBefore) {
  ObserverList<Foo> observer_list;
  Adder a(1), b(-1), c(1), d(-1);
  Disrupter disrupter(&observer_list, &b);

  observer_list.AddObserver(&a);
  observer_list.AddObserver(&b);
  observer_list.AddObserver(&disrupter);
  observer_list.AddObserver(&c);
  observer_list.AddObserver(&d);

  for (auto& o : observer_list)
    o.Observe(1);

  for (auto& o : observer_list)
    o.Observe(10);

  EXPECT_EQ(11, a.total);
  EXPECT_EQ(-1, b.total);
  EXPECT_EQ(11, c.total);
  EXPECT_EQ(-11, d.total);
}

TEST(ObserverListTest, StdIteratorRemoveAfter) {
  ObserverList<Foo> observer_list;
  Adder a(1), b(-1), c(1), d(-1);
  Disrupter disrupter(&observer_list, &c);

  observer_list.AddObserver(&a);
  observer_list.AddObserver(&b);
  observer_list.AddObserver(&disrupter);
  observer_list.AddObserver(&c);
  observer_list.AddObserver(&d);

  for (auto& o : observer_list)
    o.Observe(1);

  for (auto& o : observer_list)
    o.Observe(10);

  EXPECT_EQ(11, a.total);
  EXPECT_EQ(-11, b.total);
  EXPECT_EQ(0, c.total);
  EXPECT_EQ(-11, d.total);
}

TEST(ObserverListTest, StdIteratorRemoveAfterFront) {
  ObserverList<Foo> observer_list;
  Adder a(1), b(-1), c(1), d(-1);
  Disrupter disrupter(&observer_list, &a);

  observer_list.AddObserver(&a);
  observer_list.AddObserver(&disrupter);
  observer_list.AddObserver(&b);
  observer_list.AddObserver(&c);
  observer_list.AddObserver(&d);

  for (auto& o : observer_list)
    o.Observe(1);

  for (auto& o : observer_list)
    o.Observe(10);

  EXPECT_EQ(1, a.total);
  EXPECT_EQ(-11, b.total);
  EXPECT_EQ(11, c.total);
  EXPECT_EQ(-11, d.total);
}

TEST(ObserverListTest, StdIteratorRemoveBeforeBack) {
  ObserverList<Foo> observer_list;
  Adder a(1), b(-1), c(1), d(-1);
  Disrupter disrupter(&observer_list, &d);

  observer_list.AddObserver(&a);
  observer_list.AddObserver(&b);
  observer_list.AddObserver(&c);
  observer_list.AddObserver(&disrupter);
  observer_list.AddObserver(&d);

  for (auto& o : observer_list)
    o.Observe(1);

  for (auto& o : observer_list)
    o.Observe(10);

  EXPECT_EQ(11, a.total);
  EXPECT_EQ(-11, b.total);
  EXPECT_EQ(11, c.total);
  EXPECT_EQ(0, d.total);
}

TEST(ObserverListTest, StdIteratorRemoveFront) {
  using FooList = ObserverList<Foo>;
  FooList observer_list;
  Adder a(1), b(-1), c(1), d(-1);
  Disrupter disrupter(&observer_list, true);

  observer_list.AddObserver(&disrupter);
  observer_list.AddObserver(&a);
  observer_list.AddObserver(&b);
  observer_list.AddObserver(&c);
  observer_list.AddObserver(&d);

  bool test_disruptor = true;
  for (FooList::iterator i = observer_list.begin(), e = observer_list.end();
       i != e; ++i) {
    i->Observe(1);
    // Check that second call to i->Observe() would crash here.
    if (test_disruptor) {
      EXPECT_FALSE(i.GetCurrent());
      test_disruptor = false;
    }
  }

  for (auto& o : observer_list)
    o.Observe(10);

  EXPECT_EQ(11, a.total);
  EXPECT_EQ(-11, b.total);
  EXPECT_EQ(11, c.total);
  EXPECT_EQ(-11, d.total);
}

TEST(ObserverListTest, StdIteratorRemoveBack) {
  ObserverList<Foo> observer_list;
  Adder a(1), b(-1), c(1), d(-1);
  Disrupter disrupter(&observer_list, true);

  observer_list.AddObserver(&a);
  observer_list.AddObserver(&b);
  observer_list.AddObserver(&c);
  observer_list.AddObserver(&d);
  observer_list.AddObserver(&disrupter);

  for (auto& o : observer_list)
    o.Observe(1);

  for (auto& o : observer_list)
    o.Observe(10);

  EXPECT_EQ(11, a.total);
  EXPECT_EQ(-11, b.total);
  EXPECT_EQ(11, c.total);
  EXPECT_EQ(-11, d.total);
}

TEST(ObserverListTest, NestedLoop) {
  ObserverList<Foo> observer_list;
  Adder a(1), b(-1), c(1), d(-1);
  Disrupter disrupter(&observer_list, true);

  observer_list.AddObserver(&disrupter);
  observer_list.AddObserver(&a);
  observer_list.AddObserver(&b);
  observer_list.AddObserver(&c);
  observer_list.AddObserver(&d);

  for (auto& o : observer_list) {
    o.Observe(10);

    for (auto& o : observer_list)
      o.Observe(1);
  }

  EXPECT_EQ(15, a.total);
  EXPECT_EQ(-15, b.total);
  EXPECT_EQ(15, c.total);
  EXPECT_EQ(-15, d.total);
}

TEST(ObserverListTest, NonCompactList) {
  ObserverList<Foo> observer_list;
  Adder a(1), b(-1);

  Disrupter disrupter1(&observer_list, true);
  Disrupter disrupter2(&observer_list, true);

  // Disrupt itself and another one.
  disrupter1.SetDoomed(&disrupter2);

  observer_list.AddObserver(&disrupter1);
  observer_list.AddObserver(&disrupter2);
  observer_list.AddObserver(&a);
  observer_list.AddObserver(&b);

  for (auto& o : observer_list) {
    // Get the { nullptr, nullptr, &a, &b } non-compact list
    // on the first inner pass.
    o.Observe(10);

    for (auto& o : observer_list)
      o.Observe(1);
  }

  EXPECT_EQ(13, a.total);
  EXPECT_EQ(-13, b.total);
}

TEST(ObserverListTest, BecomesEmptyThanNonEmpty) {
  ObserverList<Foo> observer_list;
  Adder a(1), b(-1);

  Disrupter disrupter1(&observer_list, true);
  Disrupter disrupter2(&observer_list, true);

  // Disrupt itself and another one.
  disrupter1.SetDoomed(&disrupter2);

  observer_list.AddObserver(&disrupter1);
  observer_list.AddObserver(&disrupter2);

  bool add_observers = true;
  for (auto& o : observer_list) {
    // Get the { nullptr, nullptr } empty list on the first inner pass.
    o.Observe(10);

    for (auto& o : observer_list)
      o.Observe(1);

    if (add_observers) {
      observer_list.AddObserver(&a);
      observer_list.AddObserver(&b);
      add_observers = false;
    }
  }

  EXPECT_EQ(12, a.total);
  EXPECT_EQ(-12, b.total);
}

TEST(ObserverListTest, AddObserverInTheLastObserve) {
  ObserverList<Foo> observer_list;
  AddInObserve a(&observer_list);
  Adder b(-1);

  a.SetToAdd(&b);
  observer_list.AddObserver(&a);

  auto it = observer_list.begin();
  while (it != observer_list.end()) {
    auto& observer = *it;
    // Intentionally increment the iterator before calling Observe(). The
    // ObserverList starts with only one observer, and it == observer_list.end()
    // should be true after the next line.
    ++it;
    // However, the first Observe() call will add a second observer: at this
    // point, it != observer_list.end() should be true, and Observe() should be
    // called on the newly added observer on the next iteration of the loop.
    observer.Observe(10);
  }

  EXPECT_EQ(-10, b.total);
}

class MockLogAssertHandler {
 public:
  MOCK_METHOD4(
      HandleLogAssert,
      void(const char*, int, const base::StringPiece, const base::StringPiece));
};

#if DCHECK_IS_ON()
TEST(ObserverListTest, NonReentrantObserverList) {
  using ::testing::_;

  ObserverList<Foo, /*check_empty=*/false, /*allow_reentrancy=*/false>
      non_reentrant_observer_list;
  Adder a(1);
  non_reentrant_observer_list.AddObserver(&a);

  EXPECT_DCHECK_DEATH({
    for (const Foo& a : non_reentrant_observer_list) {
      for (const Foo& b : non_reentrant_observer_list) {
        std::ignore = a;
        std::ignore = b;
      }
    }
  });
}

TEST(ObserverListTest, ReentrantObserverList) {
  using ::testing::_;

  ReentrantObserverList<Foo> reentrant_observer_list;
  Adder a(1);
  reentrant_observer_list.AddObserver(&a);
  bool passed = false;
  for (const Foo& a : reentrant_observer_list) {
    for (const Foo& b : reentrant_observer_list) {
      std::ignore = a;
      std::ignore = b;
      passed = true;
    }
  }
  EXPECT_TRUE(passed);
}
#endif

}  // namespace base
