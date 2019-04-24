// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/test_navigation_listener.h"

#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/run_loop.h"
#include "fuchsia/base/mem_buffer_util.h"

namespace cr_fuchsia {
namespace {

void QuitRunLoopAndRunCallback(
    base::RunLoop* run_loop,
    TestNavigationListener::BeforeAckCallback before_ack_callback,
    const fuchsia::web::NavigationState& change,
    fuchsia::web::NavigationEventListener::OnNavigationStateChangedCallback
        ack_callback) {
  run_loop->Quit();
  before_ack_callback.Run(change, std::move(ack_callback));
}

}  // namespace

TestNavigationListener::TestNavigationListener() {
  before_ack_ = base::BindRepeating(
      [](const fuchsia::web::NavigationState&,
         OnNavigationStateChangedCallback callback) { callback(); });
}

TestNavigationListener::~TestNavigationListener() = default;

void TestNavigationListener::RunUntilNavigationStateMatches(
    const fuchsia::web::NavigationState& expected_state) {
  DCHECK(before_ack_);

  // Spin the runloop until the expected conditions are met.
  while (!AllFieldsMatch(expected_state)) {
    base::RunLoop run_loop;
    base::AutoReset<BeforeAckCallback> callback_setter(
        &before_ack_,
        base::BindRepeating(&QuitRunLoopAndRunCallback,
                            base::Unretained(&run_loop), before_ack_));
    run_loop.Run();
  }
}

void TestNavigationListener::RunUntilUrlEquals(const GURL& expected_url) {
  fuchsia::web::NavigationState state;
  state.set_url(expected_url.spec());
  RunUntilNavigationStateMatches(state);
}

void TestNavigationListener::RunUntilUrlAndTitleEquals(
    const GURL& expected_url,
    const std::string& expected_title) {
  fuchsia::web::NavigationState state;
  state.set_url(expected_url.spec());
  state.set_title(expected_title);
  RunUntilNavigationStateMatches(state);
}

void TestNavigationListener::OnNavigationStateChanged(
    fuchsia::web::NavigationState change,
    OnNavigationStateChangedCallback callback) {
  DCHECK(before_ack_);

  // Update our local cache of the Frame's current state.
  if (change.has_url())
    current_state_.set_url(change.url());
  if (change.has_title())
    current_state_.set_title(change.title());
  if (change.has_can_go_forward())
    current_state_.set_can_go_forward(change.can_go_forward());
  if (change.has_can_go_back())
    current_state_.set_can_go_back(change.can_go_back());

  // Signal readiness for the next navigation event.
  before_ack_.Run(change, std::move(callback));
}

void TestNavigationListener::SetBeforeAckHook(BeforeAckCallback send_ack_cb) {
  DCHECK(send_ack_cb);
  before_ack_ = send_ack_cb;
}

bool TestNavigationListener::AllFieldsMatch(
    const fuchsia::web::NavigationState& expected) {
  bool all_equal = true;

  if (expected.has_url()) {
    if (!current_state_.has_url() || expected.url() != current_state_.url()) {
      all_equal = false;
    }
  }
  if (expected.has_title()) {
    if (!current_state_.has_title() ||
        expected.title() != current_state_.title()) {
      all_equal = false;
    }
  }
  if (expected.has_can_go_forward()) {
    if (!current_state_.has_can_go_forward() ||
        expected.can_go_forward() != current_state_.can_go_forward()) {
      all_equal = false;
    }
  }
  if (expected.has_can_go_back()) {
    if (!current_state_.has_can_go_back() ||
        expected.can_go_back() != current_state_.can_go_back()) {
      all_equal = false;
    }
  }

  return all_equal;
}

}  // namespace cr_fuchsia
