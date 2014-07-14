// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_session_state_delegate.h"
#include "ash/test/test_shell_delegate.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_chromeos.h"
#include "chrome/browser/ui/ash/multi_user/user_switch_animator_chromeos.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_manager/user_info.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/ui_base_types.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace test {

// A test class for preparing the chrome::MultiUserWindowManager. It creates
// various windows and instantiates the chrome::MultiUserWindowManager.
class MultiUserWindowManagerChromeOSTest : public AshTestBase {
 public:
  MultiUserWindowManagerChromeOSTest()
      : multi_user_window_manager_(NULL),
        session_state_delegate_(NULL) {}

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 protected:
  // Set up the test environment for this many windows.
  void SetUpForThisManyWindows(int windows);

  // Switch the user and wait until the animation is finished.
  void SwitchUserAndWaitForAnimation(const std::string& user_id) {
    multi_user_window_manager_->ActiveUserChanged(user_id);
    base::TimeTicks now = base::TimeTicks::Now();
    while (multi_user_window_manager_->IsAnimationRunningForTest()) {
      // This should never take longer then a second.
      ASSERT_GE(1000, (base::TimeTicks::Now() - now).InMilliseconds());
      base::MessageLoop::current()->RunUntilIdle();
    }
  }

  // Return the window with the given index.
  aura::Window* window(size_t index) {
    DCHECK(index < window_.size());
    return window_[index];
  }

  // Delete the window at the given index, and set the referefence to NULL.
  void delete_window_at(size_t index) {
    delete window_[index];
    window_[index] = NULL;
  }

  // The accessor to the MultiWindowManager.
  chrome::MultiUserWindowManagerChromeOS* multi_user_window_manager() {
    return multi_user_window_manager_;
  }

  // Returns a list of all open windows in the following form:
  // "<H(idden)/S(hown)/D(eleted)>[<Owner>[,<shownForUser>]], .."
  // Like: "S[B], .." would mean that window#0 is shown and belongs to user B.
  // or "S[B,A], .." would mean that window#0 is shown, belongs to B but is
  // shown by A, and "D,..." would mean that window#0 is deleted.
  std::string GetStatus();

  // Returns a test-friendly string format of GetOwnersOfVisibleWindows().
  std::string GetOwnersOfVisibleWindowsAsString();

  TestSessionStateDelegate* session_state_delegate() {
    return session_state_delegate_;
  }

  // Make a window system modal.
  void MakeWindowSystemModal(aura::Window* window) {
    aura::Window* system_modal_container =
        window->GetRootWindow()->GetChildById(
            ash::kShellWindowId_SystemModalContainer);
    system_modal_container->AddChild(window);
  }

  void ShowWindowForUserNoUserTransition(aura::Window* window,
                                         const std::string& user_id) {
    multi_user_window_manager_->ShowWindowForUserIntern(window, user_id);
  }

  // The test session state observer does not automatically call the window
  // manager. This function gets the current user from it and also sets it to
  // the multi user window manager.
  std::string GetAndValidateCurrentUserFromSessionStateObserver() {
    const std::string& user =
        session_state_delegate()->GetActiveUserInfo()->GetUserID();
    if (user != multi_user_window_manager_->GetCurrentUserForTest())
      multi_user_window_manager()->ActiveUserChanged(user);
    return user;
  }

  // Initiate a user transition.
  void StartUserTransitionAnimation(const std::string& user_id) {
    multi_user_window_manager_->ActiveUserChanged(user_id);
  }

  // Call next animation step.
  void AdvanceUserTransitionAnimation() {
    multi_user_window_manager_->animation_->AdvanceUserTransitionAnimation();
  }

  // Return the user id of the wallpaper which is currently set.
  const std::string& GetWallaperUserIdForTest() {
    return multi_user_window_manager_->animation_->wallpaper_user_id_for_test();
  }

  // Returns true if the given window covers the screen.
  bool CoversScreen(aura::Window* window) {
    return chrome::UserSwichAnimatorChromeOS::CoversScreen(
        window);
  }

 private:
  // These get created for each session.
  std::vector<aura::Window*> window_;

  // The instance of the MultiUserWindowManager.
  chrome::MultiUserWindowManagerChromeOS* multi_user_window_manager_;

  // The session state delegate.
  ash::test::TestSessionStateDelegate* session_state_delegate_;

  DISALLOW_COPY_AND_ASSIGN(MultiUserWindowManagerChromeOSTest);
};

void MultiUserWindowManagerChromeOSTest::SetUp() {
  AshTestBase::SetUp();
  session_state_delegate_ =
      static_cast<TestSessionStateDelegate*> (
          ash::Shell::GetInstance()->session_state_delegate());
  session_state_delegate_->AddUser("a");
  session_state_delegate_->AddUser("b");
  session_state_delegate_->AddUser("c");
}

void MultiUserWindowManagerChromeOSTest::SetUpForThisManyWindows(int windows) {
  DCHECK(!window_.size());
  for (int i = 0; i < windows; i++) {
    window_.push_back(CreateTestWindowInShellWithId(i));
    window_[i]->Show();
  }
  multi_user_window_manager_ = new chrome::MultiUserWindowManagerChromeOS("A");
  multi_user_window_manager_->SetAnimationSpeedForTest(
      chrome::MultiUserWindowManagerChromeOS::ANIMATION_SPEED_DISABLED);
  chrome::MultiUserWindowManager::SetInstanceForTest(multi_user_window_manager_,
        chrome::MultiUserWindowManager::MULTI_PROFILE_MODE_SEPARATED);
  EXPECT_TRUE(multi_user_window_manager_);
}

void MultiUserWindowManagerChromeOSTest::TearDown() {
  // Since the AuraTestBase is needed to create our assets, we have to
  // also delete them before we tear it down.
  while (!window_.empty()) {
    delete *(window_.begin());
    window_.erase(window_.begin());
  }

  chrome::MultiUserWindowManager::DeleteInstance();
  AshTestBase::TearDown();
}

std::string MultiUserWindowManagerChromeOSTest::GetStatus() {
  std::string s;
  for (size_t i = 0; i < window_.size(); i++) {
    if (i)
      s += ", ";
    if (!window(i)) {
      s += "D";
      continue;
    }
    s += window(i)->IsVisible() ? "S[" : "H[";
    const std::string& owner =
        multi_user_window_manager_->GetWindowOwner(window(i));
    s += owner;
    const std::string& presenter =
        multi_user_window_manager_->GetUserPresentingWindow(window(i));
    if (!owner.empty() && owner != presenter) {
      s += ",";
      s += presenter;
    }
    s += "]";
  }
  return s;
}

std::string
MultiUserWindowManagerChromeOSTest::GetOwnersOfVisibleWindowsAsString() {
  std::set<std::string> owners;
  multi_user_window_manager_->GetOwnersOfVisibleWindows(&owners);

  std::vector<std::string> owner_list;
  owner_list.insert(owner_list.begin(), owners.begin(), owners.end());
  return JoinString(owner_list, ' ');
}

// Testing basic assumptions like default state and existence of manager.
TEST_F(MultiUserWindowManagerChromeOSTest, BasicTests) {
  SetUpForThisManyWindows(3);
  // Check the basic assumptions: All windows are visible and there is no owner.
  EXPECT_EQ("S[], S[], S[]", GetStatus());
  EXPECT_TRUE(multi_user_window_manager());
  EXPECT_EQ(multi_user_window_manager(),
            chrome::MultiUserWindowManager::GetInstance());
  EXPECT_FALSE(multi_user_window_manager()->AreWindowsSharedAmongUsers());

  // The owner of an unowned window should be empty and it should be shown on
  // all windows.
  EXPECT_EQ("", multi_user_window_manager()->GetWindowOwner(window(0)));
  EXPECT_EQ("",
      multi_user_window_manager()->GetUserPresentingWindow(window(0)));
  EXPECT_TRUE(
      multi_user_window_manager()->IsWindowOnDesktopOfUser(window(0), "A"));
  EXPECT_TRUE(
      multi_user_window_manager()->IsWindowOnDesktopOfUser(window(0), "B"));

  // Set the owner of one window should remember it as such. It should only be
  // drawn on the owners desktop - not on any other.
  multi_user_window_manager()->SetWindowOwner(window(0), "A");
  EXPECT_EQ("A", multi_user_window_manager()->GetWindowOwner(window(0)));
  EXPECT_EQ("A",
      multi_user_window_manager()->GetUserPresentingWindow(window(0)));
  EXPECT_TRUE(
      multi_user_window_manager()->IsWindowOnDesktopOfUser(window(0), "A"));
  EXPECT_FALSE(
      multi_user_window_manager()->IsWindowOnDesktopOfUser(window(0), "B"));

  // Overriding it with another state should show it on the other user's
  // desktop.
  ShowWindowForUserNoUserTransition(window(0), "B");
  EXPECT_EQ("A", multi_user_window_manager()->GetWindowOwner(window(0)));
  EXPECT_EQ("B",
      multi_user_window_manager()->GetUserPresentingWindow(window(0)));
  EXPECT_FALSE(
      multi_user_window_manager()->IsWindowOnDesktopOfUser(window(0), "A"));
  EXPECT_TRUE(
      multi_user_window_manager()->IsWindowOnDesktopOfUser(window(0), "B"));
}

// Testing simple owner changes.
TEST_F(MultiUserWindowManagerChromeOSTest, OwnerTests) {
  SetUpForThisManyWindows(5);
  // Set some windows to the active owner.
  multi_user_window_manager()->SetWindowOwner(window(0), "A");
  EXPECT_EQ("S[A], S[], S[], S[], S[]", GetStatus());
  multi_user_window_manager()->SetWindowOwner(window(2), "A");
  EXPECT_EQ("S[A], S[], S[A], S[], S[]", GetStatus());

  // Set some windows to an inactive owner. Note that the windows should hide.
  multi_user_window_manager()->SetWindowOwner(window(1), "B");
  EXPECT_EQ("S[A], H[B], S[A], S[], S[]", GetStatus());
  multi_user_window_manager()->SetWindowOwner(window(3), "B");
  EXPECT_EQ("S[A], H[B], S[A], H[B], S[]", GetStatus());

  // Assume that the user has now changed to C - which should show / hide
  // accordingly.
  multi_user_window_manager()->ActiveUserChanged("C");
  EXPECT_EQ("H[A], H[B], H[A], H[B], S[]", GetStatus());

  // If someone tries to show an inactive window it should only work if it can
  // be shown / hidden.
  multi_user_window_manager()->ActiveUserChanged("A");
  EXPECT_EQ("S[A], H[B], S[A], H[B], S[]", GetStatus());
  window(3)->Show();
  EXPECT_EQ("S[A], H[B], S[A], H[B], S[]", GetStatus());
  window(2)->Hide();
  EXPECT_EQ("S[A], H[B], H[A], H[B], S[]", GetStatus());
  window(2)->Show();
  EXPECT_EQ("S[A], H[B], S[A], H[B], S[]", GetStatus());
}

TEST_F(MultiUserWindowManagerChromeOSTest, CloseWindowTests) {
  SetUpForThisManyWindows(1);
  multi_user_window_manager()->SetWindowOwner(window(0), "B");
  EXPECT_EQ("H[B]", GetStatus());
  ShowWindowForUserNoUserTransition(window(0), "A");
  EXPECT_EQ("S[B,A]", GetStatus());
  EXPECT_TRUE(multi_user_window_manager()->AreWindowsSharedAmongUsers());
  EXPECT_EQ("B", GetOwnersOfVisibleWindowsAsString());

  aura::Window* to_be_deleted = window(0);

  EXPECT_EQ(std::string("A"),
            multi_user_window_manager()->GetUserPresentingWindow(
                to_be_deleted));
  EXPECT_EQ(std::string("B"),
            multi_user_window_manager()->GetWindowOwner(
                to_be_deleted));

  // Close the window.
  delete_window_at(0);

  EXPECT_EQ("D", GetStatus());
  EXPECT_EQ("", GetOwnersOfVisibleWindowsAsString());
  // There should be no owner anymore for that window and the shared windows
  // should be gone as well.
  EXPECT_EQ(std::string(),
            multi_user_window_manager()->GetUserPresentingWindow(
                to_be_deleted));
  EXPECT_EQ(std::string(),
            multi_user_window_manager()->GetWindowOwner(
                to_be_deleted));
}

TEST_F(MultiUserWindowManagerChromeOSTest, SharedWindowTests) {
  SetUpForThisManyWindows(5);
  // Set some owners and make sure we got what we asked for.
  multi_user_window_manager()->SetWindowOwner(window(0), "A");
  multi_user_window_manager()->SetWindowOwner(window(1), "A");
  multi_user_window_manager()->SetWindowOwner(window(2), "B");
  multi_user_window_manager()->SetWindowOwner(window(3), "B");
  multi_user_window_manager()->SetWindowOwner(window(4), "C");
  EXPECT_EQ("S[A], S[A], H[B], H[B], H[C]", GetStatus());
  EXPECT_FALSE(multi_user_window_manager()->AreWindowsSharedAmongUsers());
  EXPECT_EQ("A", GetOwnersOfVisibleWindowsAsString());

  // For all following tests we override window 2 to be shown by user B.
  ShowWindowForUserNoUserTransition(window(1), "B");

  // Change window 3 between two users and see that it changes
  // accordingly (or not).
  ShowWindowForUserNoUserTransition(window(2), "A");
  EXPECT_EQ("S[A], H[A,B], S[B,A], H[B], H[C]", GetStatus());
  EXPECT_TRUE(multi_user_window_manager()->AreWindowsSharedAmongUsers());
  EXPECT_EQ("A B", GetOwnersOfVisibleWindowsAsString());
  ShowWindowForUserNoUserTransition(window(2), "C");
  EXPECT_EQ("S[A], H[A,B], H[B,C], H[B], H[C]", GetStatus());
  EXPECT_TRUE(multi_user_window_manager()->AreWindowsSharedAmongUsers());
  EXPECT_EQ("A", GetOwnersOfVisibleWindowsAsString());

  // Switch the users and see that the results are correct.
  multi_user_window_manager()->ActiveUserChanged("B");
  EXPECT_EQ("H[A], S[A,B], H[B,C], S[B], H[C]", GetStatus());
  EXPECT_EQ("A B", GetOwnersOfVisibleWindowsAsString());
  multi_user_window_manager()->ActiveUserChanged("C");
  EXPECT_EQ("H[A], H[A,B], S[B,C], H[B], S[C]", GetStatus());
  EXPECT_EQ("B C", GetOwnersOfVisibleWindowsAsString());

  // Showing on the desktop of the already owning user should have no impact.
  ShowWindowForUserNoUserTransition(window(4), "C");
  EXPECT_EQ("H[A], H[A,B], S[B,C], H[B], S[C]", GetStatus());
  EXPECT_EQ("B C", GetOwnersOfVisibleWindowsAsString());

  // Changing however a shown window back to the original owner should hide it.
  ShowWindowForUserNoUserTransition(window(2), "B");
  EXPECT_EQ("H[A], H[A,B], H[B], H[B], S[C]", GetStatus());
  EXPECT_TRUE(multi_user_window_manager()->AreWindowsSharedAmongUsers());
  EXPECT_EQ("C", GetOwnersOfVisibleWindowsAsString());

  // And the change should be "permanent" - switching somewhere else and coming
  // back.
  multi_user_window_manager()->ActiveUserChanged("B");
  EXPECT_EQ("H[A], S[A,B], S[B], S[B], H[C]", GetStatus());
  EXPECT_EQ("A B", GetOwnersOfVisibleWindowsAsString());
  multi_user_window_manager()->ActiveUserChanged("C");
  EXPECT_EQ("H[A], H[A,B], H[B], H[B], S[C]", GetStatus());
  EXPECT_EQ("C", GetOwnersOfVisibleWindowsAsString());

  // After switching window 2 back to its original desktop, all desktops should
  // be "clean" again.
  ShowWindowForUserNoUserTransition(window(1), "A");
  EXPECT_FALSE(multi_user_window_manager()->AreWindowsSharedAmongUsers());
}

// Make sure that adding a window to another desktop does not cause harm.
TEST_F(MultiUserWindowManagerChromeOSTest, DoubleSharedWindowTests) {
  SetUpForThisManyWindows(1);
  multi_user_window_manager()->SetWindowOwner(window(0), "B");

  // Add two references to the same window.
  ShowWindowForUserNoUserTransition(window(0), "A");
  ShowWindowForUserNoUserTransition(window(0), "A");
  EXPECT_TRUE(multi_user_window_manager()->AreWindowsSharedAmongUsers());

  // Close the window.
  delete_window_at(0);

  EXPECT_EQ("D", GetStatus());
  // There should be no shares anymore open.
  EXPECT_FALSE(multi_user_window_manager()->AreWindowsSharedAmongUsers());
}

// Tests that the user's desktop visibility changes get respected. These tests
// are required to make sure that our usage of the same feature for showing and
// hiding does not interfere with the "normal operation".
TEST_F(MultiUserWindowManagerChromeOSTest, PreserveWindowVisibilityTests) {
  SetUpForThisManyWindows(5);
  // Set some owners and make sure we got what we asked for.
  // Note that we try to cover all combinations in one go.
  multi_user_window_manager()->SetWindowOwner(window(0), "A");
  multi_user_window_manager()->SetWindowOwner(window(1), "A");
  multi_user_window_manager()->SetWindowOwner(window(2), "B");
  multi_user_window_manager()->SetWindowOwner(window(3), "B");
  ShowWindowForUserNoUserTransition(window(2), "A");
  ShowWindowForUserNoUserTransition(window(3), "A");
  EXPECT_EQ("S[A], S[A], S[B,A], S[B,A], S[]", GetStatus());

  // Hiding a window should be respected - no matter if it is owned by that user
  // owned by someone else but shown on that desktop - or not owned.
  window(0)->Hide();
  window(2)->Hide();
  window(4)->Hide();
  EXPECT_EQ("H[A], S[A], H[B,A], S[B,A], H[]", GetStatus());

  // Flipping to another user and back should preserve all show / hide states.
  multi_user_window_manager()->ActiveUserChanged("B");
  EXPECT_EQ("H[A], H[A], H[B,A], H[B,A], H[]", GetStatus());

  multi_user_window_manager()->ActiveUserChanged("A");
  EXPECT_EQ("H[A], S[A], H[B,A], S[B,A], H[]", GetStatus());

  // After making them visible and switching fore and back everything should be
  // visible.
  window(0)->Show();
  window(2)->Show();
  window(4)->Show();
  EXPECT_EQ("S[A], S[A], S[B,A], S[B,A], S[]", GetStatus());

  multi_user_window_manager()->ActiveUserChanged("B");
  EXPECT_EQ("H[A], H[A], H[B,A], H[B,A], S[]", GetStatus());

  multi_user_window_manager()->ActiveUserChanged("A");
  EXPECT_EQ("S[A], S[A], S[B,A], S[B,A], S[]", GetStatus());

  // Now test that making windows visible through "normal operation" while the
  // user's desktop is hidden leads to the correct result.
  multi_user_window_manager()->ActiveUserChanged("B");
  EXPECT_EQ("H[A], H[A], H[B,A], H[B,A], S[]", GetStatus());
  window(0)->Show();
  window(2)->Show();
  window(4)->Show();
  EXPECT_EQ("H[A], H[A], H[B,A], H[B,A], S[]", GetStatus());
  multi_user_window_manager()->ActiveUserChanged("A");
  EXPECT_EQ("S[A], S[A], S[B,A], S[B,A], S[]", GetStatus());
}

// Check that minimizing a window which is owned by another user will move it
// back and gets restored upon switching back to the original user.
TEST_F(MultiUserWindowManagerChromeOSTest, MinimizeChangesOwnershipBack) {
  SetUpForThisManyWindows(4);
  multi_user_window_manager()->SetWindowOwner(window(0), "A");
  multi_user_window_manager()->SetWindowOwner(window(1), "B");
  multi_user_window_manager()->SetWindowOwner(window(2), "B");
  ShowWindowForUserNoUserTransition(window(1), "A");
  EXPECT_EQ("S[A], S[B,A], H[B], S[]", GetStatus());
  EXPECT_TRUE(multi_user_window_manager()->IsWindowOnDesktopOfUser(window(1),
                                                                   "A"));
  wm::GetWindowState(window(1))->Minimize();
  // At this time the window is still on the desktop of that user, but the user
  // does not have a way to get to it.
  EXPECT_EQ("S[A], H[B,A], H[B], S[]", GetStatus());
  EXPECT_TRUE(multi_user_window_manager()->IsWindowOnDesktopOfUser(window(1),
                                                                   "A"));
  EXPECT_TRUE(wm::GetWindowState(window(1))->IsMinimized());
  // Change to user B and make sure that minimizing does not change anything.
  multi_user_window_manager()->ActiveUserChanged("B");
  EXPECT_EQ("H[A], S[B], S[B], S[]", GetStatus());
  EXPECT_FALSE(wm::GetWindowState(window(1))->IsMinimized());
}

// Check that we cannot transfer the ownership of a minimized window.
TEST_F(MultiUserWindowManagerChromeOSTest, MinimizeSuppressesViewTransfer) {
  SetUpForThisManyWindows(1);
  multi_user_window_manager()->SetWindowOwner(window(0), "A");
  wm::GetWindowState(window(0))->Minimize();
  EXPECT_EQ("H[A]", GetStatus());

  // Try to transfer the window to user B - which should get ignored.
  ShowWindowForUserNoUserTransition(window(0), "B");
  EXPECT_EQ("H[A]", GetStatus());
}

// Testing that the activation state changes to the active window.
TEST_F(MultiUserWindowManagerChromeOSTest, ActiveWindowTests) {
  SetUpForThisManyWindows(4);

  aura::client::ActivationClient* activation_client =
      aura::client::GetActivationClient(window(0)->GetRootWindow());

  // Set some windows to the active owner.
  multi_user_window_manager()->SetWindowOwner(window(0), "A");
  multi_user_window_manager()->SetWindowOwner(window(1), "A");
  multi_user_window_manager()->SetWindowOwner(window(2), "B");
  multi_user_window_manager()->SetWindowOwner(window(3), "B");
  EXPECT_EQ("S[A], S[A], H[B], H[B]", GetStatus());

  // Set the active window for user A to be #1
  activation_client->ActivateWindow(window(1));

  // Change to user B and make sure that one of its windows is active.
  multi_user_window_manager()->ActiveUserChanged("B");
  EXPECT_EQ("H[A], H[A], S[B], S[B]", GetStatus());
  EXPECT_TRUE(window(3) == activation_client->GetActiveWindow() ||
              window(2) == activation_client->GetActiveWindow());
  // Set the active window for user B now to be #2
  activation_client->ActivateWindow(window(2));

  multi_user_window_manager()->ActiveUserChanged("A");
  EXPECT_EQ(window(1), activation_client->GetActiveWindow());

  multi_user_window_manager()->ActiveUserChanged("B");
  EXPECT_EQ(window(2), activation_client->GetActiveWindow());

  multi_user_window_manager()->ActiveUserChanged("C");
  EXPECT_EQ(NULL, activation_client->GetActiveWindow());

  // Now test that a minimized window stays minimized upon switch and back.
  multi_user_window_manager()->ActiveUserChanged("A");
  wm::GetWindowState(window(0))->Minimize();

  multi_user_window_manager()->ActiveUserChanged("B");
  multi_user_window_manager()->ActiveUserChanged("A");
  EXPECT_TRUE(wm::GetWindowState(window(0))->IsMinimized());
  EXPECT_EQ(window(1), activation_client->GetActiveWindow());
}

// Test that Transient windows are handled properly.
TEST_F(MultiUserWindowManagerChromeOSTest, TransientWindows) {
  SetUpForThisManyWindows(10);

  // We create a hierarchy like this:
  //    0 (A)  4 (B)   7 (-)   - The top level owned/not owned windows
  //    |      |       |
  //    1      5 - 6   8       - Transient child of the owned windows.
  //    |              |
  //    2              9       - A transtient child of a transient child.
  //    |
  //    3                      - ..
  multi_user_window_manager()->SetWindowOwner(window(0), "A");
  multi_user_window_manager()->SetWindowOwner(window(4), "B");
  ::wm::AddTransientChild(window(0), window(1));
  // We first attach 2->3 and then 1->2 to see that the ownership gets
  // properly propagated through the sub tree upon assigning.
  ::wm::AddTransientChild(window(2), window(3));
  ::wm::AddTransientChild(window(1), window(2));
  ::wm::AddTransientChild(window(4), window(5));
  ::wm::AddTransientChild(window(4), window(6));
  ::wm::AddTransientChild(window(7), window(8));
  ::wm::AddTransientChild(window(7), window(9));

  // By now the hierarchy should have updated itself to show all windows of A
  // and hide all windows of B. Unowned windows should remain in what ever state
  // they are in.
  EXPECT_EQ("S[A], S[], S[], S[], H[B], H[], H[], S[], S[], S[]", GetStatus());

  // Trying to show a hidden transient window shouldn't change anything for now.
  window(5)->Show();
  window(6)->Show();
  EXPECT_EQ("S[A], S[], S[], S[], H[B], H[], H[], S[], S[], S[]", GetStatus());

  // Hiding on the other hand a shown window should work and hide also its
  // children. Note that hide will have an immediate impact on itself and all
  // transient children. It furthermore should remember this state when the
  // transient children are removed from its owner later on.
  window(2)->Hide();
  window(9)->Hide();
  EXPECT_EQ("S[A], S[], H[], H[], H[B], H[], H[], S[], S[], H[]", GetStatus());

  // Switching users and switch back should return to the previous state.
  multi_user_window_manager()->ActiveUserChanged("B");
  EXPECT_EQ("H[A], H[], H[], H[], S[B], S[], S[], S[], S[], H[]", GetStatus());
  multi_user_window_manager()->ActiveUserChanged("A");
  EXPECT_EQ("S[A], S[], H[], H[], H[B], H[], H[], S[], S[], H[]", GetStatus());

  // Removing a window from its transient parent should return to the previously
  // set visibility state.
  // Note: Window2 was explicitly hidden above and that state should remain.
  // Note furthermore that Window3 should also be hidden since it was hidden
  // implicitly by hiding Window2.
  // set hidden above).
  //    0 (A)  4 (B)   7 (-)   2(-)   3 (-)    6(-)
  //    |      |       |
  //    1      5       8
  //                   |
  //                   9
  ::wm::RemoveTransientChild(window(2), window(3));
  ::wm::RemoveTransientChild(window(4), window(6));
  EXPECT_EQ("S[A], S[], H[], H[], H[B], H[], S[], S[], S[], H[]", GetStatus());
  // Before we leave we need to reverse all transient window ownerships.
  ::wm::RemoveTransientChild(window(0), window(1));
  ::wm::RemoveTransientChild(window(1), window(2));
  ::wm::RemoveTransientChild(window(4), window(5));
  ::wm::RemoveTransientChild(window(7), window(8));
  ::wm::RemoveTransientChild(window(7), window(9));
}

// Test that the initial visibility state gets remembered.
TEST_F(MultiUserWindowManagerChromeOSTest, PreserveInitialVisibility) {
  SetUpForThisManyWindows(4);

  // Set our initial show state before we assign an owner.
  window(0)->Show();
  window(1)->Hide();
  window(2)->Show();
  window(3)->Hide();
  EXPECT_EQ("S[], H[], S[], H[]", GetStatus());

  // First test: The show state gets preserved upon user switch.
  multi_user_window_manager()->SetWindowOwner(window(0), "A");
  multi_user_window_manager()->SetWindowOwner(window(1), "A");
  multi_user_window_manager()->SetWindowOwner(window(2), "B");
  multi_user_window_manager()->SetWindowOwner(window(3), "B");
  EXPECT_EQ("S[A], H[A], H[B], H[B]", GetStatus());
  multi_user_window_manager()->ActiveUserChanged("B");
  EXPECT_EQ("H[A], H[A], S[B], H[B]", GetStatus());
  multi_user_window_manager()->ActiveUserChanged("A");
  EXPECT_EQ("S[A], H[A], H[B], H[B]", GetStatus());

  // Second test: Transferring the window to another desktop preserves the
  // show state.
  ShowWindowForUserNoUserTransition(window(0), "B");
  ShowWindowForUserNoUserTransition(window(1), "B");
  ShowWindowForUserNoUserTransition(window(2), "A");
  ShowWindowForUserNoUserTransition(window(3), "A");
  EXPECT_EQ("H[A,B], H[A,B], S[B,A], H[B,A]", GetStatus());
  multi_user_window_manager()->ActiveUserChanged("B");
  EXPECT_EQ("S[A,B], H[A,B], H[B,A], H[B,A]", GetStatus());
  multi_user_window_manager()->ActiveUserChanged("A");
  EXPECT_EQ("H[A,B], H[A,B], S[B,A], H[B,A]", GetStatus());
}

// Test that a system modal dialog will switch to the desktop of the owning
// user.
TEST_F(MultiUserWindowManagerChromeOSTest, SwitchUsersUponModalityChange) {
  SetUpForThisManyWindows(1);
  session_state_delegate()->SwitchActiveUser("a");

  // Making the window system modal should not change anything.
  MakeWindowSystemModal(window(0));
  EXPECT_EQ("a", session_state_delegate()->GetActiveUserInfo()->GetUserID());

  // Making the window owned by user B should switch users.
  multi_user_window_manager()->SetWindowOwner(window(0), "b");
  EXPECT_EQ("b", session_state_delegate()->GetActiveUserInfo()->GetUserID());
}

// Test that a system modal dialog will not switch desktop if active user has
// shows window.
TEST_F(MultiUserWindowManagerChromeOSTest, DontSwitchUsersUponModalityChange) {
  SetUpForThisManyWindows(1);
  session_state_delegate()->SwitchActiveUser("a");

  // Making the window system modal should not change anything.
  MakeWindowSystemModal(window(0));
  EXPECT_EQ("a", session_state_delegate()->GetActiveUserInfo()->GetUserID());

  // Making the window owned by user a should not switch users.
  multi_user_window_manager()->SetWindowOwner(window(0), "a");
  EXPECT_EQ("a", session_state_delegate()->GetActiveUserInfo()->GetUserID());
}

// Test that a system modal dialog will not switch if shown on correct desktop
// but owned by another user.
TEST_F(MultiUserWindowManagerChromeOSTest,
       DontSwitchUsersUponModalityChangeWhenShownButNotOwned) {
  SetUpForThisManyWindows(1);
  session_state_delegate()->SwitchActiveUser("a");

  window(0)->Hide();
  multi_user_window_manager()->SetWindowOwner(window(0), "b");
  ShowWindowForUserNoUserTransition(window(0), "a");
  MakeWindowSystemModal(window(0));
  // Showing the window should trigger no user switch.
  window(0)->Show();
  EXPECT_EQ("a", session_state_delegate()->GetActiveUserInfo()->GetUserID());
}

// Test that a system modal dialog will switch if shown on incorrect desktop but
// even if owned by current user.
TEST_F(MultiUserWindowManagerChromeOSTest,
       SwitchUsersUponModalityChangeWhenShownButNotOwned) {
  SetUpForThisManyWindows(1);
  session_state_delegate()->SwitchActiveUser("a");

  window(0)->Hide();
  multi_user_window_manager()->SetWindowOwner(window(0), "a");
  ShowWindowForUserNoUserTransition(window(0), "b");
  MakeWindowSystemModal(window(0));
  // Showing the window should trigger a user switch.
  window(0)->Show();
  EXPECT_EQ("b", session_state_delegate()->GetActiveUserInfo()->GetUserID());
}

// Test that using the full user switch animations are working as expected.
TEST_F(MultiUserWindowManagerChromeOSTest, FullUserSwitchAnimationTests) {
  SetUpForThisManyWindows(3);
  // Turn the use of delays and animation on.
  multi_user_window_manager()->SetAnimationSpeedForTest(
      chrome::MultiUserWindowManagerChromeOS::ANIMATION_SPEED_FAST);
  // Set some owners and make sure we got what we asked for.
  multi_user_window_manager()->SetWindowOwner(window(0), "A");
  multi_user_window_manager()->SetWindowOwner(window(1), "B");
  multi_user_window_manager()->SetWindowOwner(window(2), "C");
  EXPECT_EQ("S[A], H[B], H[C]", GetStatus());
  EXPECT_EQ("A", GetOwnersOfVisibleWindowsAsString());

  // Switch the user fore and back and see that the results are correct.
  SwitchUserAndWaitForAnimation("B");

  EXPECT_EQ("H[A], S[B], H[C]", GetStatus());
  EXPECT_EQ("B", GetOwnersOfVisibleWindowsAsString());

  SwitchUserAndWaitForAnimation("A");

  EXPECT_EQ("S[A], H[B], H[C]", GetStatus());

  // Switch the user quickly to another user and before the animation is done
  // switch back and see that this works.
  multi_user_window_manager()->ActiveUserChanged("B");
  // With the start of the animation B should become visible.
  EXPECT_EQ("H[A], S[B], H[C]", GetStatus());
  // Check that after switching to C, C is fully visible.
  SwitchUserAndWaitForAnimation("C");
  EXPECT_EQ("H[A], H[B], S[C]", GetStatus());
}

// Make sure that we do not crash upon shutdown when an animation is pending and
// a shutdown happens.
TEST_F(MultiUserWindowManagerChromeOSTest, SystemShutdownWithActiveAnimation) {
  SetUpForThisManyWindows(2);
  // Turn the use of delays and animation on.
  multi_user_window_manager()->SetAnimationSpeedForTest(
      chrome::MultiUserWindowManagerChromeOS::ANIMATION_SPEED_FAST);
  // Set some owners and make sure we got what we asked for.
  multi_user_window_manager()->SetWindowOwner(window(0), "A");
  multi_user_window_manager()->SetWindowOwner(window(1), "B");
  StartUserTransitionAnimation("B");
  // We don't do anything more here - the animations are pending and with the
  // shutdown of the framework the animations should get cancelled. If not a
  // crash would happen.
}

// Test that using the full user switch, the animations are transitioning as
// we expect them to in all animation steps.
TEST_F(MultiUserWindowManagerChromeOSTest, AnimationSteps) {
  SetUpForThisManyWindows(3);
  // Turn the use of delays and animation on.
  multi_user_window_manager()->SetAnimationSpeedForTest(
      chrome::MultiUserWindowManagerChromeOS::ANIMATION_SPEED_FAST);
  // Set some owners and make sure we got what we asked for.
  multi_user_window_manager()->SetWindowOwner(window(0), "A");
  multi_user_window_manager()->SetWindowOwner(window(1), "B");
  multi_user_window_manager()->SetWindowOwner(window(2), "C");
  EXPECT_FALSE(CoversScreen(window(0)));
  EXPECT_FALSE(CoversScreen(window(1)));
  EXPECT_EQ("S[A], H[B], H[C]", GetStatus());
  EXPECT_EQ("A", GetOwnersOfVisibleWindowsAsString());
  EXPECT_NE(ash::SHELF_AUTO_HIDE_ALWAYS_HIDDEN,
            ash::Shell::GetInstance()->GetShelfAutoHideBehavior(
                window(0)->GetRootWindow()));
  EXPECT_EQ(1.0f, window(0)->layer()->GetTargetOpacity());

  // Start the animation and see that the old window is becoming invisible, the
  // new one visible, the background starts transitionining and the shelf hides.
  StartUserTransitionAnimation("B");
  EXPECT_EQ("->B", GetWallaperUserIdForTest());
  EXPECT_EQ("H[A], S[B], H[C]", GetStatus());
  EXPECT_EQ(0.0f, window(0)->layer()->GetTargetOpacity());
  EXPECT_EQ(1.0f, window(1)->layer()->GetTargetOpacity());
  EXPECT_EQ(ash::SHELF_AUTO_HIDE_ALWAYS_HIDDEN,
            ash::Shell::GetInstance()->GetShelfAutoHideBehavior(
                window(0)->GetRootWindow()));

  // Staring the next step should show the shelf again, but there are many
  // subsystems missing (preferences system, ChromeLauncherController, ...)
  // which should set the shelf to its users state. Since that isn't there we
  // can only make sure that it stays where it is.
  AdvanceUserTransitionAnimation();
  EXPECT_EQ("->B", GetWallaperUserIdForTest());
  EXPECT_EQ("H[A], S[B], H[C]", GetStatus());
  EXPECT_EQ(ash::SHELF_AUTO_HIDE_ALWAYS_HIDDEN,
            ash::Shell::GetInstance()->GetShelfAutoHideBehavior(
                window(0)->GetRootWindow()));

  // After the finalize the animation of the wallpaper should be finished.
  AdvanceUserTransitionAnimation();
  EXPECT_EQ("B", GetWallaperUserIdForTest());
}

// Test that the screen coverage is properly determined.
TEST_F(MultiUserWindowManagerChromeOSTest, AnimationStepsScreenCoverage) {
  SetUpForThisManyWindows(3);
  // Maximizing, fully covering the screen by bounds or fullscreen mode should
  // make CoversScreen return true.
  wm::GetWindowState(window(0))->Maximize();
  window(1)->SetBounds(gfx::Rect(0, 0, 3000, 3000));

  EXPECT_TRUE(CoversScreen(window(0)));
  EXPECT_TRUE(CoversScreen(window(1)));
  EXPECT_FALSE(CoversScreen(window(2)));

  ash::wm::WMEvent event(ash::wm::WM_EVENT_FULLSCREEN);
  wm::GetWindowState(window(2))->OnWMEvent(&event);
  EXPECT_TRUE(CoversScreen(window(2)));
}

// Test that switching from a desktop which has a maximized window to a desktop
// which has no maximized window will produce the proper animation.
TEST_F(MultiUserWindowManagerChromeOSTest, AnimationStepsMaximizeToNormal) {
  SetUpForThisManyWindows(3);
  // Turn the use of delays and animation on.
  multi_user_window_manager()->SetAnimationSpeedForTest(
      chrome::MultiUserWindowManagerChromeOS::ANIMATION_SPEED_FAST);
  // Set some owners and make sure we got what we asked for.
  multi_user_window_manager()->SetWindowOwner(window(0), "A");
  wm::GetWindowState(window(0))->Maximize();
  multi_user_window_manager()->SetWindowOwner(window(1), "B");
  multi_user_window_manager()->SetWindowOwner(window(2), "C");
  EXPECT_TRUE(CoversScreen(window(0)));
  EXPECT_FALSE(CoversScreen(window(1)));
  EXPECT_EQ("S[A], H[B], H[C]", GetStatus());
  EXPECT_EQ("A", GetOwnersOfVisibleWindowsAsString());
  EXPECT_EQ(1.0f, window(0)->layer()->GetTargetOpacity());

  // Start the animation and see that the new background is immediately set.
  StartUserTransitionAnimation("B");
  EXPECT_EQ("H[A], S[B], H[C]", GetStatus());
  EXPECT_EQ("B", GetWallaperUserIdForTest());
  EXPECT_EQ(0.0f, window(0)->layer()->GetTargetOpacity());
  EXPECT_EQ(1.0f, window(1)->layer()->GetTargetOpacity());

  // The next step will not change anything.
  AdvanceUserTransitionAnimation();
  EXPECT_EQ("B", GetWallaperUserIdForTest());
  EXPECT_EQ(0.0f, window(0)->layer()->GetTargetOpacity());
  EXPECT_EQ(1.0f, window(1)->layer()->GetTargetOpacity());

  // The final step will also not have any visible impact.
  AdvanceUserTransitionAnimation();
  EXPECT_EQ("H[A], S[B], H[C]", GetStatus());
  EXPECT_EQ("B", GetWallaperUserIdForTest());
  EXPECT_EQ(0.0f, window(0)->layer()->GetTargetOpacity());
  EXPECT_EQ(1.0f, window(1)->layer()->GetTargetOpacity());
}

// Test that switching from a desktop which has a normal window to a desktop
// which has a maximized window will produce the proper animation.
TEST_F(MultiUserWindowManagerChromeOSTest, AnimationStepsNormalToMaximized) {
  SetUpForThisManyWindows(3);
  // Turn the use of delays and animation on.
  multi_user_window_manager()->SetAnimationSpeedForTest(
      chrome::MultiUserWindowManagerChromeOS::ANIMATION_SPEED_FAST);
  // Set some owners and make sure we got what we asked for.
  multi_user_window_manager()->SetWindowOwner(window(0), "A");
  multi_user_window_manager()->SetWindowOwner(window(1), "B");
  wm::GetWindowState(window(1))->Maximize();
  multi_user_window_manager()->SetWindowOwner(window(2), "C");
  EXPECT_FALSE(CoversScreen(window(0)));
  EXPECT_TRUE(CoversScreen(window(1)));
  EXPECT_EQ("S[A], H[B], H[C]", GetStatus());
  EXPECT_EQ("A", GetOwnersOfVisibleWindowsAsString());
  EXPECT_EQ(1.0f, window(0)->layer()->GetTargetOpacity());

  // Start the animation and see that the old window is becoming invisible, the
  // new one visible and the background remains as is.
  StartUserTransitionAnimation("B");
  EXPECT_EQ("H[A], S[B], H[C]", GetStatus());
  EXPECT_EQ("", GetWallaperUserIdForTest());
  EXPECT_EQ(0.0f, window(0)->layer()->GetTargetOpacity());
  EXPECT_EQ(1.0f, window(1)->layer()->GetTargetOpacity());

  // The next step will not change anything.
  AdvanceUserTransitionAnimation();
  EXPECT_EQ("", GetWallaperUserIdForTest());
  EXPECT_EQ(0.0f, window(0)->layer()->GetTargetOpacity());
  EXPECT_EQ(1.0f, window(1)->layer()->GetTargetOpacity());

  // The final step however will switch the background.
  AdvanceUserTransitionAnimation();
  EXPECT_EQ("H[A], S[B], H[C]", GetStatus());
  EXPECT_EQ("B", GetWallaperUserIdForTest());
  EXPECT_EQ(0.0f, window(0)->layer()->GetTargetOpacity());
  EXPECT_EQ(1.0f, window(1)->layer()->GetTargetOpacity());
}

// Test that switching from a desktop which has a maximized window to a desktop
// which has a maximized window will produce the proper animation.
TEST_F(MultiUserWindowManagerChromeOSTest, AnimationStepsMaximizedToMaximized) {
  SetUpForThisManyWindows(3);
  // Turn the use of delays and animation on.
  multi_user_window_manager()->SetAnimationSpeedForTest(
      chrome::MultiUserWindowManagerChromeOS::ANIMATION_SPEED_FAST);
  // Set some owners and make sure we got what we asked for.
  multi_user_window_manager()->SetWindowOwner(window(0), "A");
  wm::GetWindowState(window(0))->Maximize();
  multi_user_window_manager()->SetWindowOwner(window(1), "B");
  wm::GetWindowState(window(1))->Maximize();
  multi_user_window_manager()->SetWindowOwner(window(2), "C");
  EXPECT_TRUE(CoversScreen(window(0)));
  EXPECT_TRUE(CoversScreen(window(1)));
  EXPECT_EQ("S[A], H[B], H[C]", GetStatus());
  EXPECT_EQ("A", GetOwnersOfVisibleWindowsAsString());
  EXPECT_EQ(1.0f, window(0)->layer()->GetTargetOpacity());

  // Start the animation and see that the old window is staying visible, the
  // new one slowly visible and the background changes immediately.
  StartUserTransitionAnimation("B");
  EXPECT_EQ("S[A], S[B], H[C]", GetStatus());
  EXPECT_EQ("B", GetWallaperUserIdForTest());
  EXPECT_EQ(1.0f, window(0)->layer()->GetTargetOpacity());
  EXPECT_EQ(1.0f, window(1)->layer()->GetTargetOpacity());

  // The next step will not change anything.
  AdvanceUserTransitionAnimation();
  EXPECT_EQ("B", GetWallaperUserIdForTest());
  EXPECT_EQ(1.0f, window(0)->layer()->GetTargetOpacity());
  EXPECT_EQ(1.0f, window(1)->layer()->GetTargetOpacity());

  // The final step however will hide the old window.
  AdvanceUserTransitionAnimation();
  EXPECT_EQ("H[A], S[B], H[C]", GetStatus());
  EXPECT_EQ("B", GetWallaperUserIdForTest());
  EXPECT_EQ(0.0f, window(0)->layer()->GetTargetOpacity());
  EXPECT_EQ(1.0f, window(1)->layer()->GetTargetOpacity());

  // Switching back will preserve the z-order by instantly showing the new
  // window, hiding the layer above it and switching instantly the wallpaper.
  StartUserTransitionAnimation("A");
  EXPECT_EQ("S[A], H[B], H[C]", GetStatus());
  EXPECT_EQ("A", GetWallaperUserIdForTest());
  EXPECT_EQ(1.0f, window(0)->layer()->GetTargetOpacity());
  EXPECT_EQ(0.0f, window(1)->layer()->GetTargetOpacity());

  // The next step will not change anything.
  AdvanceUserTransitionAnimation();
  EXPECT_EQ("A", GetWallaperUserIdForTest());
  EXPECT_EQ(1.0f, window(0)->layer()->GetTargetOpacity());
  EXPECT_EQ(0.0f, window(1)->layer()->GetTargetOpacity());

  // The final step is also not changing anything to the status.
  AdvanceUserTransitionAnimation();
  EXPECT_EQ("S[A], H[B], H[C]", GetStatus());
  EXPECT_EQ("A", GetWallaperUserIdForTest());
  EXPECT_EQ(1.0f, window(0)->layer()->GetTargetOpacity());
  EXPECT_EQ(0.0f, window(1)->layer()->GetTargetOpacity());
}

// Test that showing a window for another user also switches the desktop.
TEST_F(MultiUserWindowManagerChromeOSTest, ShowForUserSwitchesDesktop) {
  SetUpForThisManyWindows(3);
  multi_user_window_manager()->ActiveUserChanged("a");
  session_state_delegate()->SwitchActiveUser("a");

  // Set some owners and make sure we got what we asked for.
  multi_user_window_manager()->SetWindowOwner(window(0), "a");
  multi_user_window_manager()->SetWindowOwner(window(1), "b");
  multi_user_window_manager()->SetWindowOwner(window(2), "c");
  EXPECT_EQ("S[a], H[b], H[c]", GetStatus());

  // SetWindowOwner should not have changed the active user.
  EXPECT_EQ("a", GetAndValidateCurrentUserFromSessionStateObserver());

  // Check that teleporting the window of the currently active user will
  // teleport to the new desktop.
  multi_user_window_manager()->ShowWindowForUser(window(0), "b");
  EXPECT_EQ("b", GetAndValidateCurrentUserFromSessionStateObserver());
  EXPECT_EQ("S[a,b], S[b], H[c]", GetStatus());

  // Check that teleporting a window from a currently inactive user will not
  // trigger a switch.
  multi_user_window_manager()->ShowWindowForUser(window(2), "a");
  EXPECT_EQ("b", GetAndValidateCurrentUserFromSessionStateObserver());
  EXPECT_EQ("S[a,b], S[b], H[c,a]", GetStatus());
  multi_user_window_manager()->ShowWindowForUser(window(2), "b");
  EXPECT_EQ("b", GetAndValidateCurrentUserFromSessionStateObserver());
  EXPECT_EQ("S[a,b], S[b], S[c,b]", GetStatus());

  // Check that teleporting back will also change the desktop.
  multi_user_window_manager()->ShowWindowForUser(window(2), "c");
  EXPECT_EQ("c", GetAndValidateCurrentUserFromSessionStateObserver());
  EXPECT_EQ("H[a,b], H[b], S[c]", GetStatus());
}

}  // namespace test
}  // namespace ash
