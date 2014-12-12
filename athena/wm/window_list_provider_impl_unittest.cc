// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/wm/window_list_provider_impl.h"

#include <algorithm>

#include "athena/screen/public/screen_manager.h"
#include "athena/test/base/athena_test_base.h"
#include "athena/test/base/test_windows.h"
#include "athena/wm/public/window_list_provider_observer.h"
#include "athena/wm/public/window_manager.h"
#include "base/strings/string_number_conversions.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/wm/core/window_util.h"

namespace athena {

namespace {

bool AreWindowListsEqual(const aura::Window::Windows& one,
                         const aura::Window::Windows& two) {
  return one.size() == two.size() &&
         std::equal(one.begin(), one.end(), two.begin());
}

scoped_ptr<aura::Window> CreateWindow(aura::Window* parent,
                                      aura::WindowDelegate* delegate,
                                      ui::wm::WindowType window_type) {
  scoped_ptr<aura::Window> window(new aura::Window(delegate));
  window->SetType(window_type);
  window->Init(aura::WINDOW_LAYER_SOLID_COLOR);
  if (parent)
    parent->AddChild(window.get());
  return window.Pass();
}

// Return a string which defines the order of windows in |now| using the indices
// of |original|. The string will then have the lowest/oldest window on the left
// and the highest / newest on the right.
std::string GetWindowOrder(const aura::Window::Windows& original,
                           const aura::Window::Windows& now) {
  if (original.size() != now.size())
    return "size has changed.";
  std::string output;
  for (aura::Window::Windows::const_iterator it = now.begin();
       it != now.end(); ++it) {
    for (size_t i = 0; i < original.size(); i++) {
      if ((*it) == original[i]) {
        output += (output.size() ? " " : std::string()) +
                  base::IntToString(i + 1);
        break;
      }
    }
  }
  return output;
}

class WindowListObserver : public WindowListProviderObserver {
 public:
  explicit WindowListObserver(WindowListProvider* provider)
      : calls_(0),
        window_add_calls_(0),
        window_removal_calls_(0),
        provider_(provider) {
    provider_->AddObserver(this);
  }
  ~WindowListObserver() override { provider_->RemoveObserver(this); }

  int calls() const { return calls_; }
  int window_add_calls() const { return window_add_calls_; }
  int window_removal_calls() const { return window_removal_calls_; }

  // WindowListProviderObserver:
  void OnWindowStackingChangedInList() override { calls_++; }

  void OnWindowAddedToList(aura::Window* removed_window) override {
    window_add_calls_++;
  }

  void OnWindowRemovedFromList(aura::Window* removed_window,
                               int index) override {
    window_removal_calls_++;
  }

 private:
  // The number of calls to the observer.
  int calls_;
  int window_add_calls_;
  int window_removal_calls_;

  // The associated WindowListProvider which is observed.
  WindowListProvider* provider_;

  DISALLOW_COPY_AND_ASSIGN(WindowListObserver);
};


}  // namespace

typedef test::AthenaTestBase WindowListProviderImplTest;

// Tests that the order of windows match the stacking order of the windows in
// the container, even after the order is changed through the aura Window API.
TEST_F(WindowListProviderImplTest, StackingOrder) {
  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> container(new aura::Window(&delegate));
  scoped_ptr<WindowListProvider> list_provider(
      new WindowListProviderImpl(container.get()));

  scoped_ptr<aura::Window> first = test::CreateWindowWithType(
      &delegate, container.get(), ui::wm::WINDOW_TYPE_NORMAL);
  scoped_ptr<aura::Window> second =
      CreateWindow(container.get(), &delegate, ui::wm::WINDOW_TYPE_NORMAL);
  scoped_ptr<aura::Window> third =
      CreateWindow(container.get(), &delegate, ui::wm::WINDOW_TYPE_NORMAL);

  EXPECT_EQ(3u, container->children().size());
  EXPECT_EQ(container->children().size(),
            list_provider->GetWindowList().size());

  EXPECT_TRUE(AreWindowListsEqual(container->children(),
                                  list_provider->GetWindowList()));

  container->StackChildAtTop(first.get());
  EXPECT_TRUE(AreWindowListsEqual(container->children(),
                                  list_provider->GetWindowList()));
  EXPECT_EQ(3u, container->children().size());
  EXPECT_EQ(first.get(), container->children().back());
}

// Tests that only normal windows of the associated container will be listed.
TEST_F(WindowListProviderImplTest, ListContainsOnlyNormalWindows) {
  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> container(new aura::Window(&delegate));
  scoped_ptr<WindowListProvider> list_provider(
      new WindowListProviderImpl(container.get()));

  scoped_ptr<aura::Window> first =
      CreateWindow(container.get(), &delegate, ui::wm::WINDOW_TYPE_NORMAL);
  scoped_ptr<aura::Window> second =
      CreateWindow(container.get(), &delegate, ui::wm::WINDOW_TYPE_POPUP);
  scoped_ptr<aura::Window> third =
      CreateWindow(container.get(), &delegate, ui::wm::WINDOW_TYPE_NORMAL);
  scoped_ptr<aura::Window> fourth =
      CreateWindow(container.get(), &delegate, ui::wm::WINDOW_TYPE_MENU);

  const aura::Window::Windows& list = list_provider->GetWindowList();
  EXPECT_EQ(list.end(), std::find(list.begin(), list.end(), second.get()));
  EXPECT_EQ(list.end(), std::find(list.begin(), list.end(), fourth.get()));
  EXPECT_NE(list.end(), std::find(list.begin(), list.end(), first.get()));
  EXPECT_NE(list.end(), std::find(list.begin(), list.end(), third.get()));
}

// Testing that IsValidWidow, IsWindowInList and AddWindow work as expected.
TEST_F(WindowListProviderImplTest, SimpleChecks) {
  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> container(new aura::Window(&delegate));
  scoped_ptr<WindowListProviderImpl> list_provider(
      new WindowListProviderImpl(container.get()));

  scoped_ptr<aura::Window> normal_window =
      CreateWindow(nullptr, &delegate, ui::wm::WINDOW_TYPE_NORMAL);
  scoped_ptr<aura::Window> popup_window =
      CreateWindow(nullptr, &delegate, ui::wm::WINDOW_TYPE_POPUP);
  scoped_ptr<aura::Window> menu_window =
      CreateWindow(nullptr, &delegate, ui::wm::WINDOW_TYPE_MENU);

  // Check which windows are valid and which are not.
  EXPECT_TRUE(list_provider->IsValidWindow(normal_window.get()));
  EXPECT_FALSE(list_provider->IsValidWindow(popup_window.get()));
  EXPECT_FALSE(list_provider->IsValidWindow(menu_window.get()));

  // Check that no window is currently in the list.
  EXPECT_FALSE(list_provider->IsWindowInList(normal_window.get()));
  EXPECT_FALSE(list_provider->IsWindowInList(popup_window.get()));
  EXPECT_FALSE(list_provider->IsWindowInList(menu_window.get()));

  // Check that adding the window will add it to the list.
  container->AddChild(normal_window.get());
  EXPECT_TRUE(list_provider->IsWindowInList(normal_window.get()));
}

// Testing that window ordering functions work as expected.
TEST_F(WindowListProviderImplTest, TestWindowOrderingFunctions) {
  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> container(new aura::Window(&delegate));
  scoped_ptr<WindowListProvider> list_provider(
      new WindowListProviderImpl(container.get()));
  scoped_ptr<WindowListObserver> observer(
      new WindowListObserver(list_provider.get()));

  scoped_ptr<aura::Window> window1 =
      CreateWindow(nullptr, &delegate, ui::wm::WINDOW_TYPE_NORMAL);
  scoped_ptr<aura::Window> window2 =
      CreateWindow(nullptr, &delegate, ui::wm::WINDOW_TYPE_NORMAL);
  scoped_ptr<aura::Window> window3 =
      CreateWindow(nullptr, &delegate, ui::wm::WINDOW_TYPE_NORMAL);

  EXPECT_FALSE(list_provider->IsWindowInList(window1.get()));
  EXPECT_FALSE(list_provider->IsWindowInList(window2.get()));
  EXPECT_FALSE(list_provider->IsWindowInList(window3.get()));

  // Add the windows.
  container->AddChild(window1.get());
  container->AddChild(window2.get());
  container->AddChild(window3.get());
  // Make a copy of the window-list in the original order.
  aura::Window::Windows original_order = list_provider->GetWindowList();
  ASSERT_EQ(3U, original_order.size());
  EXPECT_EQ(original_order[0], window1.get());
  EXPECT_EQ(original_order[1], window2.get());
  EXPECT_EQ(original_order[2], window3.get());

  EXPECT_EQ(0, observer.get()->calls());

  // Move 1 (from the back) in front of 2.
  list_provider->StackWindowFrontOf(window1.get(), window2.get());
  EXPECT_EQ("2 1 3", GetWindowOrder(original_order,
                                    list_provider->GetWindowList()));
  EXPECT_EQ(1, observer->calls());

  // Move 3 (from the front) in front of 2.
  list_provider->StackWindowFrontOf(window3.get(), window2.get());
  EXPECT_EQ("2 3 1", GetWindowOrder(original_order,
                                    list_provider->GetWindowList()));
  EXPECT_EQ(2, observer->calls());

  // Move 1 (from the front) behind 2.
  list_provider->StackWindowBehindTo(window1.get(), window2.get());
  EXPECT_EQ("1 2 3", GetWindowOrder(original_order,
                                    list_provider->GetWindowList()));
  EXPECT_EQ(3, observer->calls());

  // Move 1 (from the back) in front of 3.
  list_provider->StackWindowFrontOf(window1.get(), window3.get());
  EXPECT_EQ("2 3 1", GetWindowOrder(original_order,
                                    list_provider->GetWindowList()));
  EXPECT_EQ(4, observer->calls());

  // Test that no change should also report no call.
  list_provider->StackWindowFrontOf(window1.get(), window3.get());
  EXPECT_EQ("2 3 1", GetWindowOrder(original_order,
                                    list_provider->GetWindowList()));
  EXPECT_EQ(4, observer->calls());
  list_provider->StackWindowBehindTo(window3.get(), window1.get());
  EXPECT_EQ("2 3 1", GetWindowOrder(original_order,
                                    list_provider->GetWindowList()));
  EXPECT_EQ(4, observer->calls());
}

TEST_F(WindowListProviderImplTest, TestWindowAddRemoveNotification) {
  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> container(new aura::Window(&delegate));
  scoped_ptr<WindowListProvider> list_provider(
      new WindowListProviderImpl(container.get()));
  scoped_ptr<WindowListObserver> observer(
      new WindowListObserver(list_provider.get()));

  scoped_ptr<aura::Window> window1 =
      CreateWindow(container.get(), &delegate, ui::wm::WINDOW_TYPE_NORMAL);
  scoped_ptr<aura::Window> window2 =
      CreateWindow(container.get(), &delegate, ui::wm::WINDOW_TYPE_NORMAL);
  scoped_ptr<aura::Window> window3 =
      CreateWindow(container.get(), &delegate, ui::wm::WINDOW_TYPE_NORMAL);
  scoped_ptr<aura::Window> window4 =
      CreateWindow(container.get(), &delegate, ui::wm::WINDOW_TYPE_POPUP);
  // The popup-window (window4) should not be added tot he list.
  EXPECT_EQ(3, observer->window_add_calls());

  // The popup-window should not be included in the window-list.
  ASSERT_EQ(3U, list_provider->GetWindowList().size());
  EXPECT_EQ(0, observer->window_removal_calls());
  EXPECT_EQ(3, observer->window_add_calls());
  EXPECT_FALSE(list_provider->IsWindowInList(window4.get()));

  // Destroying the popup window should not trigger the remove notification.
  window4.reset();
  ASSERT_EQ(3U, list_provider->GetWindowList().size());
  EXPECT_EQ(0, observer->window_removal_calls());

  window2.reset();
  ASSERT_EQ(2U, list_provider->GetWindowList().size());
  EXPECT_EQ(1, observer->window_removal_calls());

  window1.reset();
  ASSERT_EQ(1U, list_provider->GetWindowList().size());
  EXPECT_EQ(2, observer->window_removal_calls());

  window3.reset();
  ASSERT_EQ(0U, list_provider->GetWindowList().size());
  EXPECT_EQ(3, observer->window_removal_calls());
}

// Test that transient windows are handled property.
TEST_F(WindowListProviderImplTest, TransientWindows) {
  aura::test::TestWindowDelegate delegate;
  delegate.set_can_focus(true);

  WindowListProvider* list_provider =
      WindowManager::Get()->GetWindowListProvider();

  scoped_ptr<WindowListObserver> observer(
      new WindowListObserver(list_provider));
  scoped_ptr<aura::Window> w1 = test::CreateNormalWindow(&delegate, nullptr);
  w1->Show();
  scoped_ptr<aura::Window> w2 = test::CreateNormalWindow(&delegate, nullptr);
  w2->Show();
  scoped_ptr<aura::Window> t1 = test::CreateTransientWindow(
      &delegate, w1.get(), ui::MODAL_TYPE_NONE, false);
  t1->Show();

  EXPECT_EQ(2u, list_provider->GetWindowList().size());

  // Activation should honor transient relations.
  wm::ActivateWindow(w2.get());
  EXPECT_EQ(w1.get(), list_provider->GetWindowList()[0]);
  EXPECT_EQ(w2.get(), list_provider->GetWindowList()[1]);

  EXPECT_EQ(w1.get(), w1->parent()->children()[0]);
  EXPECT_EQ(t1.get(), w1->parent()->children()[1]);
  EXPECT_EQ(w2.get(), w1->parent()->children()[2]);

  wm::ActivateWindow(w1.get());
  EXPECT_EQ(w2.get(), w1->parent()->children()[0]);
  EXPECT_EQ(w1.get(), w1->parent()->children()[1]);
  EXPECT_EQ(t1.get(), w1->parent()->children()[2]);

  // Manual operations should honor transient relations too.
  // TODO(oshima): moving the active window back should activate the top window.
  list_provider->StackWindowBehindTo(w1.get(), w2.get());
  EXPECT_EQ(w1.get(), w1->parent()->children()[0]);
  EXPECT_EQ(t1.get(), w1->parent()->children()[1]);
  EXPECT_EQ(w2.get(), w1->parent()->children()[2]);

  list_provider->StackWindowFrontOf(w1.get(), w2.get());
  EXPECT_EQ(w2.get(), w1->parent()->children()[0]);
  EXPECT_EQ(w1.get(), w1->parent()->children()[1]);
  EXPECT_EQ(t1.get(), w1->parent()->children()[2]);

  // Transient windows should follow the transient parent's
  // visibility.
  EXPECT_TRUE(t1->IsVisible());
  w1->Hide();
  EXPECT_FALSE(t1->IsVisible());
  w1->Show();
  EXPECT_TRUE(t1->IsVisible());

  // Resetting transient window won't notify the observer.
  t1.reset();
  EXPECT_EQ(0, observer->window_removal_calls());
}

}  // namespace athena
