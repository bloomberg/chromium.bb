// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/overlay_presenter.h"

#include "ios/chrome/browser/overlays/overlay_request_queue_impl.h"
#include "ios/chrome/browser/overlays/public/overlay_modality.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"
#include "ios/chrome/browser/overlays/test/fake_overlay_ui_delegate.h"
#include "ios/chrome/browser/overlays/test/fake_overlay_user_data.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for OverlayPresenter.
class OverlayPresenterTest : public PlatformTest {
 public:
  OverlayPresenterTest()
      : web_state_list_(&web_state_list_delegate_),
        presenter_(OverlayModality::kWebContentArea, &web_state_list_) {}
  ~OverlayPresenterTest() override { presenter_.Disconnect(); }

  WebStateList& web_state_list() { return web_state_list_; }
  web::WebState* active_web_state() {
    return web_state_list_.GetActiveWebState();
  }
  OverlayPresenter& presenter() { return presenter_; }
  FakeOverlayUIDelegate& ui_delegate() { return ui_delegate_; }

  OverlayRequestQueueImpl* GetQueueForWebState(web::WebState* web_state) {
    if (!web_state)
      return nullptr;
    OverlayRequestQueueImpl::Container::CreateForWebState(web_state);
    return OverlayRequestQueueImpl::Container::FromWebState(web_state)
        ->QueueForModality(OverlayModality::kWebContentArea);
  }

 private:
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  FakeOverlayUIDelegate ui_delegate_;
  OverlayPresenter presenter_;
};

// Tests that setting the UI delegate will present overlays requested before the
// delegate is provided.
TEST_F(OverlayPresenterTest, PresentAfterSettingUIDelegate) {
  // Add a WebState to the list and add a request to that WebState's queue.
  web_state_list().InsertWebState(0, std::make_unique<web::TestWebState>(),
                                  WebStateList::InsertionFlags::INSERT_ACTIVATE,
                                  WebStateOpener());
  web::WebState* web_state = active_web_state();
  GetQueueForWebState(web_state)->AddRequest(
      OverlayRequest::CreateWithConfig<FakeOverlayUserData>(nullptr));
  ASSERT_EQ(FakeOverlayUIDelegate::PresentationState::kNotPresented,
            ui_delegate().GetPresentationState(web_state));

  // Set the UI delegate and verify that the request has been presented.
  presenter().SetUIDelegate(&ui_delegate());
  EXPECT_EQ(FakeOverlayUIDelegate::PresentationState::kPresented,
            ui_delegate().GetPresentationState(web_state));
}

// Tests that requested overlays are presented when added to the active queue
// after the UI delegate has been provided.
TEST_F(OverlayPresenterTest, PresentAfterRequestAddedToActiveQueue) {
  // Add a WebState to the list and add a request to that WebState's queue.
  presenter().SetUIDelegate(&ui_delegate());
  web_state_list().InsertWebState(0, std::make_unique<web::TestWebState>(),
                                  WebStateList::InsertionFlags::INSERT_ACTIVATE,
                                  WebStateOpener());
  web::WebState* web_state = active_web_state();
  GetQueueForWebState(web_state)->AddRequest(
      OverlayRequest::CreateWithConfig<FakeOverlayUserData>(nullptr));
  // Verify that the requested overlay has been presented.
  EXPECT_EQ(FakeOverlayUIDelegate::PresentationState::kPresented,
            ui_delegate().GetPresentationState(web_state));
}

// Tests resetting the UI delegate.  The UI should be cancelled in the previous
// UI delegate and presented in the new delegate.
TEST_F(OverlayPresenterTest, ResetUIDelegate) {
  // Add a WebState to the list and add a request to that WebState's queue.
  presenter().SetUIDelegate(&ui_delegate());
  web_state_list().InsertWebState(0, std::make_unique<web::TestWebState>(),
                                  WebStateList::InsertionFlags::INSERT_ACTIVATE,
                                  WebStateOpener());
  web::WebState* web_state = active_web_state();
  OverlayRequestQueueImpl* queue = GetQueueForWebState(web_state);
  queue->AddRequest(
      OverlayRequest::CreateWithConfig<FakeOverlayUserData>(nullptr));
  OverlayRequest* request = queue->front_request();

  ASSERT_EQ(FakeOverlayUIDelegate::PresentationState::kPresented,
            ui_delegate().GetPresentationState(web_state));

  // Reset the UI delegate and verify that the overlay UI is cancelled in the
  // previous delegate's context and presented in the new delegate's context.
  FakeOverlayUIDelegate new_ui_delegate;
  presenter().SetUIDelegate(&new_ui_delegate);
  EXPECT_EQ(FakeOverlayUIDelegate::PresentationState::kCancelled,
            ui_delegate().GetPresentationState(web_state));
  EXPECT_EQ(FakeOverlayUIDelegate::PresentationState::kPresented,
            new_ui_delegate.GetPresentationState(web_state));
  EXPECT_EQ(request, queue->front_request());

  // Reset the UI delegate to nullptr and verify that the overlay UI is
  // cancelled in |new_ui_delegate|'s context.
  presenter().SetUIDelegate(nullptr);
  EXPECT_EQ(FakeOverlayUIDelegate::PresentationState::kCancelled,
            new_ui_delegate.GetPresentationState(web_state));
  EXPECT_EQ(request, queue->front_request());
}

// Tests changing the active WebState while no overlays are presented over the
// current active WebState.
TEST_F(OverlayPresenterTest, ChangeActiveWebStateWhileNotPresenting) {
  // Add a WebState to the list and activate it.
  presenter().SetUIDelegate(&ui_delegate());
  web_state_list().InsertWebState(0, std::make_unique<web::TestWebState>(),
                                  WebStateList::InsertionFlags::INSERT_ACTIVATE,
                                  WebStateOpener());
  web::WebState* first_web_state = active_web_state();
  EXPECT_EQ(FakeOverlayUIDelegate::PresentationState::kNotPresented,
            ui_delegate().GetPresentationState(first_web_state));

  // Create a new WebState with a queued request and add it as the new active
  // WebState.
  std::unique_ptr<web::WebState> passed_web_state =
      std::make_unique<web::TestWebState>();
  web::WebState* second_web_state = passed_web_state.get();
  GetQueueForWebState(second_web_state)
      ->AddRequest(
          OverlayRequest::CreateWithConfig<FakeOverlayUserData>(nullptr));
  web_state_list().InsertWebState(1, std::move(passed_web_state),
                                  WebStateList::InsertionFlags::INSERT_ACTIVATE,
                                  WebStateOpener());

  // Verify that the new active WebState's overlay is being presented.
  EXPECT_EQ(FakeOverlayUIDelegate::PresentationState::kNotPresented,
            ui_delegate().GetPresentationState(first_web_state));
  EXPECT_EQ(FakeOverlayUIDelegate::PresentationState::kPresented,
            ui_delegate().GetPresentationState(second_web_state));
}

// Tests changing the active WebState while is it presenting an overlay.
TEST_F(OverlayPresenterTest, ChangeActiveWebStateWhilePresenting) {
  // Add a WebState to the list and add a request to that WebState's queue.
  presenter().SetUIDelegate(&ui_delegate());
  web_state_list().InsertWebState(0, std::make_unique<web::TestWebState>(),
                                  WebStateList::InsertionFlags::INSERT_ACTIVATE,
                                  WebStateOpener());
  web::WebState* first_web_state = active_web_state();
  GetQueueForWebState(first_web_state)
      ->AddRequest(
          OverlayRequest::CreateWithConfig<FakeOverlayUserData>(nullptr));
  ASSERT_EQ(FakeOverlayUIDelegate::PresentationState::kPresented,
            ui_delegate().GetPresentationState(first_web_state));

  // Create a new WebState with a queued request and add it as the new active
  // WebState.
  std::unique_ptr<web::WebState> passed_web_state =
      std::make_unique<web::TestWebState>();
  web::WebState* second_web_state = passed_web_state.get();
  GetQueueForWebState(second_web_state)
      ->AddRequest(
          OverlayRequest::CreateWithConfig<FakeOverlayUserData>(nullptr));
  web_state_list().InsertWebState(1, std::move(passed_web_state),
                                  WebStateList::InsertionFlags::INSERT_ACTIVATE,
                                  WebStateOpener());

  // Verify that the previously shown overlay is hidden and that the overlay for
  // the new active WebState is presented.
  EXPECT_EQ(FakeOverlayUIDelegate::PresentationState::kHidden,
            ui_delegate().GetPresentationState(first_web_state));
  EXPECT_EQ(FakeOverlayUIDelegate::PresentationState::kPresented,
            ui_delegate().GetPresentationState(second_web_state));

  // Reactivate the first WebState and verify that its overlay is presented
  // while the second WebState's overlay is hidden.
  web_state_list().ActivateWebStateAt(0);
  EXPECT_EQ(FakeOverlayUIDelegate::PresentationState::kPresented,
            ui_delegate().GetPresentationState(first_web_state));
  EXPECT_EQ(FakeOverlayUIDelegate::PresentationState::kHidden,
            ui_delegate().GetPresentationState(second_web_state));
}

// Tests replacing the active WebState while it is presenting an overlay.
TEST_F(OverlayPresenterTest, ReplaceActiveWebState) {
  // Add a WebState to the list and add a request to that WebState's queue.
  presenter().SetUIDelegate(&ui_delegate());
  web_state_list().InsertWebState(0, std::make_unique<web::TestWebState>(),
                                  WebStateList::InsertionFlags::INSERT_ACTIVATE,
                                  WebStateOpener());
  web::WebState* first_web_state = active_web_state();
  GetQueueForWebState(first_web_state)
      ->AddRequest(
          OverlayRequest::CreateWithConfig<FakeOverlayUserData>(nullptr));
  ASSERT_EQ(FakeOverlayUIDelegate::PresentationState::kPresented,
            ui_delegate().GetPresentationState(first_web_state));

  // Replace |first_web_state| with a new active WebState with a queued request.
  std::unique_ptr<web::WebState> passed_web_state =
      std::make_unique<web::TestWebState>();
  web::WebState* replacement_web_state = passed_web_state.get();
  GetQueueForWebState(replacement_web_state)
      ->AddRequest(
          OverlayRequest::CreateWithConfig<FakeOverlayUserData>(nullptr));
  web_state_list().ReplaceWebStateAt(0, std::move(passed_web_state));

  // Verify that the previously shown overlay is canceled and that the overlay
  // for the replacement WebState is presented.
  EXPECT_EQ(FakeOverlayUIDelegate::PresentationState::kCancelled,
            ui_delegate().GetPresentationState(first_web_state));
  EXPECT_EQ(FakeOverlayUIDelegate::PresentationState::kPresented,
            ui_delegate().GetPresentationState(replacement_web_state));
}

// Tests removing the active WebState while it is presenting an overlay.
TEST_F(OverlayPresenterTest, RemoveActiveWebState) {
  // Add a WebState to the list and add a request to that WebState's queue.
  presenter().SetUIDelegate(&ui_delegate());
  web_state_list().InsertWebState(0, std::make_unique<web::TestWebState>(),
                                  WebStateList::InsertionFlags::INSERT_ACTIVATE,
                                  WebStateOpener());
  web::WebState* web_state = active_web_state();
  GetQueueForWebState(web_state)->AddRequest(
      OverlayRequest::CreateWithConfig<FakeOverlayUserData>(nullptr));
  EXPECT_EQ(FakeOverlayUIDelegate::PresentationState::kPresented,
            ui_delegate().GetPresentationState(web_state));

  // Remove the WebState and verify that its overlay was cancelled.
  web_state_list().CloseWebStateAt(0, 0);
  EXPECT_EQ(FakeOverlayUIDelegate::PresentationState::kCancelled,
            ui_delegate().GetPresentationState(web_state));
}

// Tests dismissing an overlay for user interaction.
TEST_F(OverlayPresenterTest, DismissForUserInteraction) {
  // Add a WebState to the list and add two request to that WebState's queue.
  presenter().SetUIDelegate(&ui_delegate());
  web_state_list().InsertWebState(0, std::make_unique<web::TestWebState>(),
                                  WebStateList::InsertionFlags::INSERT_ACTIVATE,
                                  WebStateOpener());
  web::WebState* web_state = active_web_state();
  OverlayRequestQueueImpl* queue = GetQueueForWebState(web_state);
  std::unique_ptr<OverlayRequest> passed_request =
      OverlayRequest::CreateWithConfig<FakeOverlayUserData>(nullptr);
  OverlayRequest* first_request = passed_request.get();
  queue->AddRequest(std::move(passed_request));
  passed_request =
      OverlayRequest::CreateWithConfig<FakeOverlayUserData>(nullptr);
  OverlayRequest* second_request = passed_request.get();
  queue->AddRequest(std::move(passed_request));

  EXPECT_EQ(FakeOverlayUIDelegate::PresentationState::kPresented,
            ui_delegate().GetPresentationState(web_state));
  EXPECT_EQ(first_request, queue->front_request());
  EXPECT_EQ(2U, queue->size());

  // Dismiss the overlay and check that the second request's overlay is
  // presented.
  ui_delegate().SimulateDismissalForWebState(
      web_state, OverlayDismissalReason::kUserInteraction);

  EXPECT_EQ(FakeOverlayUIDelegate::PresentationState::kPresented,
            ui_delegate().GetPresentationState(web_state));
  EXPECT_EQ(second_request, queue->front_request());
  EXPECT_EQ(1U, queue->size());
}
