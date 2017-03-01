// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/shared/chrome/browser/tabs/web_state_list.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/supports_user_data.h"
#import "ios/shared/chrome/browser/tabs/web_state_list_observer.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {
const char kURL0[] = "https://chromium.org/0";
const char kURL1[] = "https://chromium.org/1";
const char kURL2[] = "https://chromium.org/2";
const char kSupportsUserDataDeathGuardKey = '\0';

// A base::SupportsUserData::Data that tracks whether a base::SupportsUserData
// has been deleted (the fact is recorded in a provided pointer as part of the
// object destruction).
class SupportsUserDataDeathGuard : public base::SupportsUserData::Data {
 public:
  explicit SupportsUserDataDeathGuard(bool* object_was_destroyed)
      : object_was_destroyed_(object_was_destroyed) {
    *object_was_destroyed_ = false;
  }

  ~SupportsUserDataDeathGuard() override { *object_was_destroyed_ = true; }

 private:
  bool* object_was_destroyed_;

  DISALLOW_COPY_AND_ASSIGN(SupportsUserDataDeathGuard);
};

// WebStateList observer that records which events have been called by the
// WebStateList.
class WebStateListTestObserver : public WebStateListObserver {
 public:
  WebStateListTestObserver() = default;

  // Reset statistics whether events have been called.
  void ResetStatistics() {
    web_state_inserted_called_ = false;
    web_state_moved_called_ = false;
    web_state_replaced_called_ = false;
    web_state_detached_called_ = false;
  }

  // Returns whether WebStateInsertedAt was invoked.
  bool web_state_inserted_called() const { return web_state_inserted_called_; }

  // Returns whether WebStateMoved was invoked.
  bool web_state_moved_called() const { return web_state_moved_called_; }

  // Returns whether WebStateReplacedAt was invoked.
  bool web_state_replaced_called() const { return web_state_replaced_called_; }

  // Returns whether WebStateDetachedAt was invoked.
  bool web_state_detached_called() const { return web_state_detached_called_; }

  // WebStateListObserver implementation.
  void WebStateInsertedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index) override {
    web_state_inserted_called_ = true;
  }

  void WebStateMoved(WebStateList* web_state_list,
                     web::WebState* web_state,
                     int from_index,
                     int to_index) override {
    web_state_moved_called_ = true;
  }

  void WebStateReplacedAt(WebStateList* web_state_list,
                          web::WebState* old_web_state,
                          web::WebState* new_web_state,
                          int index) override {
    web_state_replaced_called_ = true;
  }

  void WebStateDetachedAt(WebStateList* web_state_list,
                          web::WebState* web_state,
                          int index) override {
    web_state_detached_called_ = true;
  }

 private:
  bool web_state_inserted_called_ = false;
  bool web_state_moved_called_ = false;
  bool web_state_replaced_called_ = false;
  bool web_state_detached_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(WebStateListTestObserver);
};

// A fake NavigationManager used to test opener-opened relationship in the
// WebStateList.
class FakeNavigationManer : public web::TestNavigationManager {
 public:
  FakeNavigationManer() = default;

  // web::NavigationManager implementation.
  int GetCurrentItemIndex() const override { return current_item_index_; }

  int GetLastCommittedItemIndex() const override { return current_item_index_; }

  bool CanGoBack() const override { return current_item_index_ > 0; }

  bool CanGoForward() const override { return current_item_index_ < INT_MAX; }

  void GoBack() override {
    DCHECK(CanGoBack());
    --current_item_index_;
  }

  void GoForward() override {
    DCHECK(CanGoForward());
    ++current_item_index_;
  }

  void GoToIndex(int index) override { current_item_index_ = index; }

  int current_item_index_ = 0;

  DISALLOW_COPY_AND_ASSIGN(FakeNavigationManer);
};

}  // namespace

class WebStateListTest : public PlatformTest {
 public:
  WebStateListTest() : web_state_list_(WebStateList::WebStateOwned) {
    web_state_list_.AddObserver(&observer_);
  }

  ~WebStateListTest() override { web_state_list_.RemoveObserver(&observer_); }

 protected:
  WebStateList web_state_list_;
  WebStateListTestObserver observer_;

  // This method should return std::unique_ptr<> however, due to the complex
  // ownership of Tab, WebStateList currently uses raw pointers. Change this
  // once Tab ownership is sane, see http://crbug.com/546222 for progress.
  web::WebState* CreateWebState(const char* url) {
    auto test_web_state = base::MakeUnique<web::TestWebState>();
    test_web_state->SetCurrentURL(GURL(url));
    test_web_state->SetNavigationManager(
        base::MakeUnique<FakeNavigationManer>());
    return test_web_state.release();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebStateListTest);
};

TEST_F(WebStateListTest, IsEmpty) {
  EXPECT_EQ(0, web_state_list_.count());
  EXPECT_TRUE(web_state_list_.empty());

  web_state_list_.InsertWebState(0, CreateWebState(kURL0), nullptr);

  EXPECT_TRUE(observer_.web_state_inserted_called());
  EXPECT_EQ(1, web_state_list_.count());
  EXPECT_FALSE(web_state_list_.empty());
}

TEST_F(WebStateListTest, InsertUrlSingle) {
  web_state_list_.InsertWebState(0, CreateWebState(kURL0), nullptr);

  EXPECT_TRUE(observer_.web_state_inserted_called());
  EXPECT_EQ(1, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
}

TEST_F(WebStateListTest, InsertUrlMultiple) {
  web_state_list_.InsertWebState(0, CreateWebState(kURL0), nullptr);
  web_state_list_.InsertWebState(0, CreateWebState(kURL1), nullptr);
  web_state_list_.InsertWebState(1, CreateWebState(kURL2), nullptr);

  EXPECT_TRUE(observer_.web_state_inserted_called());
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
}

TEST_F(WebStateListTest, MoveWebStateAtRightByOne) {
  web_state_list_.InsertWebState(0, CreateWebState(kURL0), nullptr);
  web_state_list_.InsertWebState(1, CreateWebState(kURL1), nullptr);
  web_state_list_.InsertWebState(2, CreateWebState(kURL2), nullptr);

  // Sanity check before closing WebState.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());

  observer_.ResetStatistics();
  web_state_list_.MoveWebStateAt(0, 1);

  EXPECT_TRUE(observer_.web_state_moved_called());
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
}

TEST_F(WebStateListTest, MoveWebStateAtRightByMoreThanOne) {
  web_state_list_.InsertWebState(0, CreateWebState(kURL0), nullptr);
  web_state_list_.InsertWebState(1, CreateWebState(kURL1), nullptr);
  web_state_list_.InsertWebState(2, CreateWebState(kURL2), nullptr);

  // Sanity check before closing WebState.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());

  observer_.ResetStatistics();
  web_state_list_.MoveWebStateAt(0, 2);

  EXPECT_TRUE(observer_.web_state_moved_called());
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
}

TEST_F(WebStateListTest, MoveWebStateAtLeftByOne) {
  web_state_list_.InsertWebState(0, CreateWebState(kURL0), nullptr);
  web_state_list_.InsertWebState(1, CreateWebState(kURL1), nullptr);
  web_state_list_.InsertWebState(2, CreateWebState(kURL2), nullptr);

  // Sanity check before closing WebState.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());

  observer_.ResetStatistics();
  web_state_list_.MoveWebStateAt(2, 1);

  EXPECT_TRUE(observer_.web_state_moved_called());
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
}

TEST_F(WebStateListTest, MoveWebStateAtLeftByMoreThanOne) {
  web_state_list_.InsertWebState(0, CreateWebState(kURL0), nullptr);
  web_state_list_.InsertWebState(1, CreateWebState(kURL1), nullptr);
  web_state_list_.InsertWebState(2, CreateWebState(kURL2), nullptr);

  // Sanity check before closing WebState.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());

  observer_.ResetStatistics();
  web_state_list_.MoveWebStateAt(2, 0);

  EXPECT_TRUE(observer_.web_state_moved_called());
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
}

TEST_F(WebStateListTest, MoveWebStateAtSameIndex) {
  web_state_list_.InsertWebState(0, CreateWebState(kURL0), nullptr);
  web_state_list_.InsertWebState(1, CreateWebState(kURL1), nullptr);
  web_state_list_.InsertWebState(2, CreateWebState(kURL2), nullptr);

  // Sanity check before closing WebState.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());

  observer_.ResetStatistics();
  web_state_list_.MoveWebStateAt(2, 2);

  EXPECT_FALSE(observer_.web_state_moved_called());
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());
}

TEST_F(WebStateListTest, ReplaceWebStateAt) {
  web_state_list_.InsertWebState(0, CreateWebState(kURL0), nullptr);
  web_state_list_.InsertWebState(1, CreateWebState(kURL1), nullptr);

  // Sanity check before replacing WebState.
  EXPECT_EQ(2, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());

  observer_.ResetStatistics();
  std::unique_ptr<web::WebState> old_web_state(
      web_state_list_.ReplaceWebStateAt(1, CreateWebState(kURL2), nullptr));

  EXPECT_TRUE(observer_.web_state_replaced_called());
  EXPECT_EQ(2, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, old_web_state->GetVisibleURL().spec());
}

TEST_F(WebStateListTest, DetachWebStateAtIndexBegining) {
  web_state_list_.InsertWebState(0, CreateWebState(kURL0), nullptr);
  web_state_list_.InsertWebState(1, CreateWebState(kURL1), nullptr);
  web_state_list_.InsertWebState(2, CreateWebState(kURL2), nullptr);

  // Sanity check before closing WebState.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());

  observer_.ResetStatistics();
  std::unique_ptr<web::WebState> old_web_state(
      web_state_list_.DetachWebStateAt(0));

  EXPECT_TRUE(observer_.web_state_detached_called());
  EXPECT_EQ(2, web_state_list_.count());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
}

TEST_F(WebStateListTest, DetachWebStateAtIndexMiddle) {
  web_state_list_.InsertWebState(0, CreateWebState(kURL0), nullptr);
  web_state_list_.InsertWebState(1, CreateWebState(kURL1), nullptr);
  web_state_list_.InsertWebState(2, CreateWebState(kURL2), nullptr);

  // Sanity check before closing WebState.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());

  observer_.ResetStatistics();
  std::unique_ptr<web::WebState> old_web_state(
      web_state_list_.DetachWebStateAt(1));

  EXPECT_TRUE(observer_.web_state_detached_called());
  EXPECT_EQ(2, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
}

TEST_F(WebStateListTest, DetachWebStateAtIndexLast) {
  web_state_list_.InsertWebState(0, CreateWebState(kURL0), nullptr);
  web_state_list_.InsertWebState(1, CreateWebState(kURL1), nullptr);
  web_state_list_.InsertWebState(2, CreateWebState(kURL2), nullptr);

  // Sanity check before closing WebState.
  EXPECT_EQ(3, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
  EXPECT_EQ(kURL2, web_state_list_.GetWebStateAt(2)->GetVisibleURL().spec());

  observer_.ResetStatistics();
  std::unique_ptr<web::WebState> old_web_state(
      web_state_list_.DetachWebStateAt(2));

  EXPECT_TRUE(observer_.web_state_detached_called());
  EXPECT_EQ(2, web_state_list_.count());
  EXPECT_EQ(kURL0, web_state_list_.GetWebStateAt(0)->GetVisibleURL().spec());
  EXPECT_EQ(kURL1, web_state_list_.GetWebStateAt(1)->GetVisibleURL().spec());
}

TEST_F(WebStateListTest, OwnershipBorrowed) {
  bool web_state_was_killed = false;
  auto test_web_state = base::MakeUnique<web::TestWebState>();
  test_web_state->SetUserData(
      &kSupportsUserDataDeathGuardKey,
      base::MakeUnique<SupportsUserDataDeathGuard>(&web_state_was_killed));

  auto web_state_list =
      base::MakeUnique<WebStateList>(WebStateList::WebStateBorrowed);
  web_state_list->InsertWebState(0, test_web_state.get(), nullptr);
  EXPECT_FALSE(web_state_was_killed);

  web_state_list.reset();
  EXPECT_FALSE(web_state_was_killed);
}

TEST_F(WebStateListTest, OwnershipOwned) {
  bool web_state_was_killed = false;
  auto test_web_state = base::MakeUnique<web::TestWebState>();
  test_web_state->SetUserData(
      &kSupportsUserDataDeathGuardKey,
      base::MakeUnique<SupportsUserDataDeathGuard>(&web_state_was_killed));

  auto web_state_list =
      base::MakeUnique<WebStateList>(WebStateList::WebStateOwned);
  web_state_list->InsertWebState(0, test_web_state.release(), nullptr);
  EXPECT_FALSE(web_state_was_killed);

  web_state_list.reset();
  EXPECT_TRUE(web_state_was_killed);
}

TEST_F(WebStateListTest, OpenersEmptyList) {
  EXPECT_TRUE(web_state_list_.empty());

  EXPECT_EQ(WebStateList::kInvalidIndex,
            web_state_list_.GetIndexOfNextWebStateOpenedBy(
                nullptr, WebStateList::kInvalidIndex, false));
  EXPECT_EQ(WebStateList::kInvalidIndex,
            web_state_list_.GetIndexOfLastWebStateOpenedBy(
                nullptr, WebStateList::kInvalidIndex, false));

  EXPECT_EQ(WebStateList::kInvalidIndex,
            web_state_list_.GetIndexOfNextWebStateOpenedBy(
                nullptr, WebStateList::kInvalidIndex, true));
  EXPECT_EQ(WebStateList::kInvalidIndex,
            web_state_list_.GetIndexOfLastWebStateOpenedBy(
                nullptr, WebStateList::kInvalidIndex, true));
}

TEST_F(WebStateListTest, OpenersNothingOpened) {
  web_state_list_.InsertWebState(0, CreateWebState(kURL0), nullptr);
  web_state_list_.InsertWebState(1, CreateWebState(kURL1), nullptr);
  web_state_list_.InsertWebState(2, CreateWebState(kURL2), nullptr);

  for (int index = 0; index < web_state_list_.count(); ++index) {
    web::WebState* opener = web_state_list_.GetWebStateAt(index);
    EXPECT_EQ(
        WebStateList::kInvalidIndex,
        web_state_list_.GetIndexOfNextWebStateOpenedBy(opener, index, false));
    EXPECT_EQ(
        WebStateList::kInvalidIndex,
        web_state_list_.GetIndexOfLastWebStateOpenedBy(opener, index, false));

    EXPECT_EQ(
        WebStateList::kInvalidIndex,
        web_state_list_.GetIndexOfNextWebStateOpenedBy(opener, index, true));
    EXPECT_EQ(
        WebStateList::kInvalidIndex,
        web_state_list_.GetIndexOfLastWebStateOpenedBy(opener, index, true));
  }
}

TEST_F(WebStateListTest, OpenersChildsAfterOpener) {
  web_state_list_.InsertWebState(0, CreateWebState(kURL0), nullptr);
  web::WebState* opener = web_state_list_.GetWebStateAt(0);

  web_state_list_.InsertWebState(1, CreateWebState(kURL1), opener);
  web_state_list_.InsertWebState(2, CreateWebState(kURL2), opener);

  const int start_index = web_state_list_.GetIndexOfWebState(opener);
  EXPECT_EQ(1,
            web_state_list_.GetIndexOfNextWebStateOpenedBy(opener, start_index,
                                                           false));
  EXPECT_EQ(2,
            web_state_list_.GetIndexOfLastWebStateOpenedBy(opener, start_index,
                                                           false));

  EXPECT_EQ(1,
            web_state_list_.GetIndexOfNextWebStateOpenedBy(opener, start_index,
                                                           true));
  EXPECT_EQ(2,
            web_state_list_.GetIndexOfLastWebStateOpenedBy(opener, start_index,
                                                           true));

  // Simulate a navigation on the opener, results should not change if not
  // using groups, but should now be kInvalidIndex otherwise.
  opener->GetNavigationManager()->GoForward();

  EXPECT_EQ(1,
            web_state_list_.GetIndexOfNextWebStateOpenedBy(opener, start_index,
                                                           false));
  EXPECT_EQ(2,
            web_state_list_.GetIndexOfLastWebStateOpenedBy(opener, start_index,
                                                           false));

  EXPECT_EQ(WebStateList::kInvalidIndex,
            web_state_list_.GetIndexOfNextWebStateOpenedBy(opener, start_index,
                                                           true));
  EXPECT_EQ(WebStateList::kInvalidIndex,
            web_state_list_.GetIndexOfLastWebStateOpenedBy(opener, start_index,
                                                           true));

  // Add a new WebState with the same opener. It should be considered the next
  // WebState if groups are considered and the last independently on whether
  // groups are used or not.
  web_state_list_.InsertWebState(3, CreateWebState(kURL2), opener);

  EXPECT_EQ(1,
            web_state_list_.GetIndexOfNextWebStateOpenedBy(opener, start_index,
                                                           false));
  EXPECT_EQ(3,
            web_state_list_.GetIndexOfLastWebStateOpenedBy(opener, start_index,
                                                           false));

  EXPECT_EQ(3,
            web_state_list_.GetIndexOfNextWebStateOpenedBy(opener, start_index,
                                                           true));
  EXPECT_EQ(3,
            web_state_list_.GetIndexOfLastWebStateOpenedBy(opener, start_index,
                                                           true));
}

TEST_F(WebStateListTest, OpenersChildsBeforeOpener) {
  web_state_list_.InsertWebState(0, CreateWebState(kURL0), nullptr);
  web::WebState* opener = web_state_list_.GetWebStateAt(0);

  web_state_list_.InsertWebState(0, CreateWebState(kURL1), opener);
  web_state_list_.InsertWebState(1, CreateWebState(kURL2), opener);

  const int start_index = web_state_list_.GetIndexOfWebState(opener);
  EXPECT_EQ(WebStateList::kInvalidIndex,
            web_state_list_.GetIndexOfNextWebStateOpenedBy(opener, start_index,
                                                           false));
  EXPECT_EQ(WebStateList::kInvalidIndex,
            web_state_list_.GetIndexOfLastWebStateOpenedBy(opener, start_index,
                                                           false));

  EXPECT_EQ(WebStateList::kInvalidIndex,
            web_state_list_.GetIndexOfNextWebStateOpenedBy(opener, start_index,
                                                           true));
  EXPECT_EQ(WebStateList::kInvalidIndex,
            web_state_list_.GetIndexOfLastWebStateOpenedBy(opener, start_index,
                                                           true));
}
