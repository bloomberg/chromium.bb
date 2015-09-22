// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/gesture_manager.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/mus/gesture_manager_delegate.h"
#include "components/mus/public/cpp/keys.h"
#include "components/mus/server_view.h"
#include "components/mus/test_server_view_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/mojo/events/input_events.mojom.h"

namespace mus {
namespace {

const uint32_t kInvalidGestureId = GestureManager::kInvalidGestureId;

void MarkAsRespondsToTouch(ServerView* view) {
  std::vector<uint8_t> empty_vector;
  view->SetProperty(kViewManagerKeyWantsTouchEvents, &empty_vector);
}

std::set<uint32_t> SetWith(uint32_t v1) {
  std::set<uint32_t> result;
  result.insert(v1);
  return result;
}

std::set<uint32_t> SetWith(uint32_t v1, uint32_t v2) {
  std::set<uint32_t> result;
  result.insert(v1);
  result.insert(v2);
  return result;
}

std::set<uint32_t> SetWith(uint32_t v1, uint32_t v2, uint32_t v3) {
  std::set<uint32_t> result;
  result.insert(v1);
  result.insert(v2);
  result.insert(v3);
  return result;
}

std::string EventTypeToString(mojo::EventType event_type) {
  switch (event_type) {
    case mojo::EVENT_TYPE_POINTER_CANCEL:
      return "cancel";
    case mojo::EVENT_TYPE_POINTER_DOWN:
      return "down";
    case mojo::EVENT_TYPE_POINTER_MOVE:
      return "move";
    case mojo::EVENT_TYPE_POINTER_UP:
      return "up";
    default:
      break;
  }
  return std::string("unexpected event");
}

mojo::EventPtr CreateEvent(mojo::EventType type,
                           int32_t pointer_id,
                           int x,
                           int y) {
  mojo::EventPtr event(mojo::Event::New());
  event->action = type;
  event->pointer_data = mojo::PointerData::New();
  event->pointer_data->pointer_id = pointer_id;
  event->pointer_data->location = mojo::LocationData::New();
  event->pointer_data->location->x = x;
  event->pointer_data->location->y = y;
  return event.Pass();
}

struct CompareViewByConnectionId {
  bool operator()(const ServerView* a, const ServerView* b) {
    return a->id().connection_id < b->id().connection_id;
  }
};

std::string IDsToString(const std::set<uint32_t>& ids) {
  std::string result;
  for (uint32_t id : ids) {
    if (!result.empty())
      result += ",";
    result += base::UintToString(id);
  }
  return result;
}

std::string GestureStateChangeToString(const ServerView* view,
                                       const GestureStateChange& change) {
  std::string result =
      "connection=" + base::IntToString(view->id().connection_id);
  if (change.chosen_gesture != GestureManager::kInvalidGestureId)
    result += " chosen=" + base::UintToString(change.chosen_gesture);
  if (!change.canceled_gestures.empty())
    result += " canceled=" + IDsToString(change.canceled_gestures);
  return result;
}

}  // namespace

class TestGestureManagerDelegate : public GestureManagerDelegate {
 public:
  TestGestureManagerDelegate() {}
  ~TestGestureManagerDelegate() override {}

  std::string GetAndClearDescriptions() {
    const std::string result(base::JoinString(descriptions_, "\n"));
    descriptions_.clear();
    return result;
  }

  std::vector<std::string>& descriptions() { return descriptions_; }

  void AppendDescriptionsFromResults(const ChangeMap& change_map) {
    std::set<const ServerView*, CompareViewByConnectionId> views_by_id;
    for (const auto& pair : change_map)
      views_by_id.insert(pair.first);

    for (auto* view : views_by_id) {
      descriptions_.push_back(
          GestureStateChangeToString(view, change_map.find(view)->second));
    }
  }

  // GestureManagerDelegate:
  void ProcessEvent(const ServerView* view,
                    mojo::EventPtr event,
                    bool has_chosen_gesture) override {
    descriptions_.push_back(
        EventTypeToString(event->action) + " pointer=" +
        base::IntToString(event->pointer_data->pointer_id) + " connection=" +
        base::UintToString(view->id().connection_id) + " chosen=" +
        (has_chosen_gesture ? "true" : "false"));
  }

 private:
  std::vector<std::string> descriptions_;

  DISALLOW_COPY_AND_ASSIGN(TestGestureManagerDelegate);
};

class GestureManagerTest : public testing::Test {
 public:
  GestureManagerTest()
      : root_(&view_delegate_, ViewId(1, 1)),
        child_(&view_delegate_, ViewId(2, 2)),
        gesture_manager_(&gesture_delegate_, &root_) {
    view_delegate_.set_root_view(&root_);
    root_.SetVisible(true);
    MarkAsRespondsToTouch(&root_);
    root_.SetBounds(gfx::Rect(0, 0, 100, 100));
  }
  ~GestureManagerTest() override {}

  void SetGestures(const ServerView* view,
                   int32_t pointer_id,
                   uint32_t chosen_gesture_id,
                   const std::set<uint32_t>& possible_gesture_ids,
                   const std::set<uint32_t>& canceled_ids) {
    scoped_ptr<ChangeMap> result(
        gesture_manager_.SetGestures(view, pointer_id, chosen_gesture_id,
                                     possible_gesture_ids, canceled_ids));
    gesture_delegate_.AppendDescriptionsFromResults(*result);
  }

  void AddChildView() {
    MarkAsRespondsToTouch(&child_);
    child_.SetVisible(true);
    root_.Add(&child_);
    child_.SetBounds(gfx::Rect(0, 0, 100, 100));
  }

 protected:
  TestServerViewDelegate view_delegate_;
  ServerView root_;
  ServerView child_;
  TestGestureManagerDelegate gesture_delegate_;
  GestureManager gesture_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GestureManagerTest);
};

TEST_F(GestureManagerTest, SingleViewAndSingleGesture) {
  const int32_t pointer_id = 1;
  gesture_manager_.ProcessEvent(
      *CreateEvent(mojo::EVENT_TYPE_POINTER_DOWN, pointer_id, 5, 5));
  EXPECT_EQ("down pointer=1 connection=1 chosen=false",
            gesture_delegate_.GetAndClearDescriptions());

  // Choose this pointer.
  SetGestures(&root_, pointer_id, 10u, SetWith(10u), std::set<uint32_t>());
  EXPECT_EQ("connection=1 chosen=10",
            gesture_delegate_.GetAndClearDescriptions());
}

TEST_F(GestureManagerTest, SingleViewAndTwoGestures) {
  const int32_t pointer_id = 1;
  gesture_manager_.ProcessEvent(
      *CreateEvent(mojo::EVENT_TYPE_POINTER_DOWN, pointer_id, 5, 5));
  EXPECT_EQ("down pointer=1 connection=1 chosen=false",
            gesture_delegate_.GetAndClearDescriptions());

  SetGestures(&root_, pointer_id, kInvalidGestureId, SetWith(5u, 10u),
              std::set<uint32_t>());

  // Delegate should have got nothing.
  EXPECT_EQ(std::string(), gesture_delegate_.GetAndClearDescriptions());

  // Cancel 10, 5 should become active.
  SetGestures(&root_, pointer_id, kInvalidGestureId, SetWith(5u, 10u),
              SetWith(10u));

  EXPECT_EQ("connection=1 chosen=5 canceled=10",
            gesture_delegate_.GetAndClearDescriptions());
}

TEST_F(GestureManagerTest, TwoViewsSingleGesture) {
  AddChildView();

  const int32_t pointer_id = 1;
  gesture_manager_.ProcessEvent(
      *CreateEvent(mojo::EVENT_TYPE_POINTER_DOWN, pointer_id, 5, 5));
  // Deepest child should be queried first.
  EXPECT_EQ("down pointer=1 connection=2 chosen=false",
            gesture_delegate_.GetAndClearDescriptions());

  // Respond from the first view, which triggers the second to be queried.
  SetGestures(&child_, pointer_id, kInvalidGestureId, SetWith(5u, 10u),
              std::set<uint32_t>());
  EXPECT_EQ("down pointer=1 connection=1 chosen=false",
            gesture_delegate_.GetAndClearDescriptions());

  // Respond with 5,10 for the second view. Should get nothing.
  SetGestures(&root_, pointer_id, kInvalidGestureId, SetWith(5u, 10u),
              std::set<uint32_t>());
  EXPECT_EQ(std::string(), gesture_delegate_.GetAndClearDescriptions());

  // Cancel 10 in the child.
  SetGestures(&child_, pointer_id, kInvalidGestureId, SetWith(5u, 10u),
              SetWith(10u));
  EXPECT_EQ("connection=2 canceled=10",
            gesture_delegate_.GetAndClearDescriptions());

  // Choose 5 in the root. This should choose 5 in the root and cancel 5 in
  // the child.
  SetGestures(&root_, pointer_id, 5u, SetWith(5u, 10u), SetWith(10u));
  ASSERT_EQ(2u, gesture_delegate_.descriptions().size());
  EXPECT_EQ("connection=1 chosen=5 canceled=10",
            gesture_delegate_.descriptions()[0]);
  EXPECT_EQ("connection=2 canceled=5", gesture_delegate_.descriptions()[1]);
}

TEST_F(GestureManagerTest, TwoViewsWaitForMoveToChoose) {
  AddChildView();

  const int32_t pointer_id = 1;
  gesture_manager_.ProcessEvent(
      *CreateEvent(mojo::EVENT_TYPE_POINTER_DOWN, pointer_id, 5, 5));
  // Deepest child should be queried first.
  EXPECT_EQ("down pointer=1 connection=2 chosen=false",
            gesture_delegate_.GetAndClearDescriptions());

  // Send a move. The move should not be processed as GestureManager is
  // still waiting for responses.
  gesture_manager_.ProcessEvent(
      *CreateEvent(mojo::EVENT_TYPE_POINTER_MOVE, pointer_id, 6, 6));
  EXPECT_EQ(std::string(), gesture_delegate_.GetAndClearDescriptions());

  // Respond from the first view, which triggers the second to be queried.
  SetGestures(&child_, pointer_id, kInvalidGestureId, SetWith(5u, 10u),
              std::set<uint32_t>());
  EXPECT_EQ("down pointer=1 connection=1 chosen=false",
            gesture_delegate_.GetAndClearDescriptions());

  // Respond with 1,2 for the second view.
  SetGestures(&root_, pointer_id, kInvalidGestureId, SetWith(1u, 2u),
              std::set<uint32_t>());
  // Now that we've responded to the down requests we should get a move for the
  // child.
  EXPECT_EQ("move pointer=1 connection=2 chosen=false",
            gesture_delegate_.GetAndClearDescriptions());

  // Respond for the child, root should now get move.
  SetGestures(&child_, pointer_id, kInvalidGestureId, SetWith(5u, 10u),
              std::set<uint32_t>());
  EXPECT_EQ("move pointer=1 connection=1 chosen=false",
            gesture_delegate_.GetAndClearDescriptions());

  // Respond with nothing chosen for the root. Nothing should come in as no
  // pending moves.
  SetGestures(&root_, pointer_id, kInvalidGestureId, SetWith(1u, 2u),
              std::set<uint32_t>());
  EXPECT_EQ(std::string(), gesture_delegate_.GetAndClearDescriptions());

  // Send another move event and respond with a chosen id.
  gesture_manager_.ProcessEvent(
      *CreateEvent(mojo::EVENT_TYPE_POINTER_MOVE, pointer_id, 7, 7));
  EXPECT_EQ("move pointer=1 connection=2 chosen=false",
            gesture_delegate_.GetAndClearDescriptions());
  SetGestures(&child_, pointer_id, 5u, SetWith(5u, 10u), std::set<uint32_t>());
  ASSERT_EQ(3u, gesture_delegate_.descriptions().size());
  // Now that a gesture is chosen the move event is generated.
  EXPECT_EQ("move pointer=1 connection=1 chosen=true",
            gesture_delegate_.descriptions()[0]);
  EXPECT_EQ("connection=1 canceled=1,2", gesture_delegate_.descriptions()[1]);
  EXPECT_EQ("connection=2 chosen=5 canceled=10",
            gesture_delegate_.descriptions()[2]);
}

TEST_F(GestureManagerTest, SingleViewNewPointerAfterChoose) {
  const int32_t pointer_id = 1;
  gesture_manager_.ProcessEvent(
      *CreateEvent(mojo::EVENT_TYPE_POINTER_DOWN, pointer_id, 5, 5));
  EXPECT_EQ("down pointer=1 connection=1 chosen=false",
            gesture_delegate_.GetAndClearDescriptions());

  // Choose 5.
  SetGestures(&root_, pointer_id, 5u, SetWith(5u, 10u), std::set<uint32_t>());
  EXPECT_EQ("connection=1 chosen=5 canceled=10",
            gesture_delegate_.GetAndClearDescriptions());

  // Start another down event with a different pointer.
  const int32_t pointer_id2 = 2;
  gesture_manager_.ProcessEvent(
      *CreateEvent(mojo::EVENT_TYPE_POINTER_DOWN, pointer_id2, 5, 5));
  EXPECT_EQ("down pointer=2 connection=1 chosen=false",
            gesture_delegate_.GetAndClearDescriptions());

  // For the new pointer supply the id of a gesture that has been chosen.
  // Even though we didn't explicitly supply 5 as chosen, 5 is chosen because
  // it's already in the chosen state for pointer 1.
  SetGestures(&root_, pointer_id2, kInvalidGestureId, SetWith(5u, 11u),
              std::set<uint32_t>());
  EXPECT_EQ("connection=1 chosen=5 canceled=11",
            gesture_delegate_.GetAndClearDescriptions());
}

TEST_F(GestureManagerTest, SingleViewChoosingConflictingGestures) {
  // For pointer1 choose 1 with 1,2,3 as possibilities.
  const int32_t pointer1 = 1;
  gesture_manager_.ProcessEvent(
      *CreateEvent(mojo::EVENT_TYPE_POINTER_DOWN, pointer1, 5, 5));
  gesture_delegate_.GetAndClearDescriptions();
  SetGestures(&root_, pointer1, 1u, SetWith(1u, 2u, 3u), std::set<uint32_t>());
  EXPECT_EQ("connection=1 chosen=1 canceled=2,3",
            gesture_delegate_.GetAndClearDescriptions());

  // For pointer2 choose 11 with 11,12 as possibilities.
  const int32_t pointer2 = 2;
  gesture_manager_.ProcessEvent(
      *CreateEvent(mojo::EVENT_TYPE_POINTER_DOWN, pointer2, 5, 5));
  gesture_delegate_.GetAndClearDescriptions();
  SetGestures(&root_, pointer2, 11u, SetWith(11u, 12u), std::set<uint32_t>());
  EXPECT_EQ("connection=1 chosen=11 canceled=12",
            gesture_delegate_.GetAndClearDescriptions());

  // For pointer3 choose 21 with 1,11,21 as possibilties.
  const int32_t pointer3 = 3;
  gesture_manager_.ProcessEvent(
      *CreateEvent(mojo::EVENT_TYPE_POINTER_DOWN, pointer3, 5, 5));
  gesture_delegate_.GetAndClearDescriptions();
  SetGestures(&root_, pointer3, 21u, SetWith(1u, 11u, 21u),
              std::set<uint32_t>());
  EXPECT_EQ("connection=1 chosen=21 canceled=1,11",
            gesture_delegate_.GetAndClearDescriptions());
}

TEST_F(GestureManagerTest,
       TwoViewsRespondingWithChosenGestureSendsRemainingEvents) {
  AddChildView();

  // Start two pointer downs, don't respond to either.
  const int32_t pointer1 = 1;
  gesture_manager_.ProcessEvent(
      *CreateEvent(mojo::EVENT_TYPE_POINTER_DOWN, pointer1, 5, 5));
  EXPECT_EQ("down pointer=1 connection=2 chosen=false",
            gesture_delegate_.GetAndClearDescriptions());

  const int32_t pointer2 = 2;
  gesture_manager_.ProcessEvent(
      *CreateEvent(mojo::EVENT_TYPE_POINTER_DOWN, pointer2, 5, 5));
  EXPECT_EQ("down pointer=2 connection=2 chosen=false",
            gesture_delegate_.GetAndClearDescriptions());

  // Queue up a move event for pointer1. The event should not be forwarded
  // as we're still waiting.
  gesture_manager_.ProcessEvent(
      *CreateEvent(mojo::EVENT_TYPE_POINTER_MOVE, pointer1, 5, 5));
  EXPECT_EQ(std::string(), gesture_delegate_.GetAndClearDescriptions());

  // Respond with 1,2 for pointer1 (nothing chosen yet).
  SetGestures(&child_, pointer1, kInvalidGestureId, SetWith(1u, 2u),
              std::set<uint32_t>());
  EXPECT_EQ("down pointer=1 connection=1 chosen=false",
            gesture_delegate_.GetAndClearDescriptions());

  // Respond with 1,2 and choose 1 for pointer2. This results in the following:
  // down for pointer 1 (because we chose a gesture in common with pointer1),
  // move for pointer 1 in both connections (because a gesture was chosen queued
  // up events are sent), down for pointer2 for the root and finally
  // notification of what was chosen.
  SetGestures(&child_, pointer2, 1u, SetWith(1u, 2u), std::set<uint32_t>());
  ASSERT_EQ(5u, gesture_delegate_.descriptions().size());
  EXPECT_EQ("down pointer=1 connection=1 chosen=true",
            gesture_delegate_.descriptions()[0]);
  EXPECT_EQ("move pointer=1 connection=2 chosen=true",
            gesture_delegate_.descriptions()[1]);
  EXPECT_EQ("move pointer=1 connection=1 chosen=true",
            gesture_delegate_.descriptions()[2]);
  EXPECT_EQ("down pointer=2 connection=1 chosen=true",
            gesture_delegate_.descriptions()[3]);
  EXPECT_EQ("connection=2 chosen=1 canceled=2",
            gesture_delegate_.descriptions()[4]);
}

TEST_F(GestureManagerTest, TwoViewsSingleGestureUp) {
  AddChildView();

  const int32_t pointer_id = 1;
  gesture_manager_.ProcessEvent(
      *CreateEvent(mojo::EVENT_TYPE_POINTER_DOWN, pointer_id, 5, 5));
  // Deepest child should be queried first.
  EXPECT_EQ("down pointer=1 connection=2 chosen=false",
            gesture_delegate_.GetAndClearDescriptions());

  // Send an up, shouldn't result in anything.
  gesture_manager_.ProcessEvent(
      *CreateEvent(mojo::EVENT_TYPE_POINTER_UP, pointer_id, 5, 5));
  EXPECT_EQ(std::string(), gesture_delegate_.GetAndClearDescriptions());

  // Respond from the first view, with a chosen gesture.
  SetGestures(&child_, pointer_id, 5u, SetWith(5u, 10u), std::set<uint32_t>());
  ASSERT_EQ(4u, gesture_delegate_.descriptions().size());
  EXPECT_EQ("down pointer=1 connection=1 chosen=true",
            gesture_delegate_.descriptions()[0]);
  EXPECT_EQ("up pointer=1 connection=2 chosen=true",
            gesture_delegate_.descriptions()[1]);
  EXPECT_EQ("up pointer=1 connection=1 chosen=true",
            gesture_delegate_.descriptions()[2]);
  EXPECT_EQ("connection=2 chosen=5 canceled=10",
            gesture_delegate_.descriptions()[3]);
}

TEST_F(GestureManagerTest, SingleViewSingleGestureCancel) {
  const int32_t pointer_id = 1;
  gesture_manager_.ProcessEvent(
      *CreateEvent(mojo::EVENT_TYPE_POINTER_DOWN, pointer_id, 5, 5));
  EXPECT_EQ("down pointer=1 connection=1 chosen=false",
            gesture_delegate_.GetAndClearDescriptions());

  // Send a cancel, shouldn't result in anything.
  gesture_manager_.ProcessEvent(
      *CreateEvent(mojo::EVENT_TYPE_POINTER_CANCEL, pointer_id, 5, 5));
  EXPECT_EQ(std::string(), gesture_delegate_.GetAndClearDescriptions());

  // Respond from the first view, with no gesture, should unblock cancel.
  SetGestures(&root_, pointer_id, kInvalidGestureId, SetWith(5u, 10u),
              std::set<uint32_t>());
  EXPECT_EQ("cancel pointer=1 connection=1 chosen=false",
            gesture_delegate_.GetAndClearDescriptions());
}

}  // namespace mus
