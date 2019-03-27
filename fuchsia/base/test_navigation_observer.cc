// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/test_navigation_observer.h"

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
    TestNavigationObserver::BeforeAckCallback before_ack_callback,
    const chromium::web::NavigationEvent& change,
    chromium::web::NavigationEventObserver::OnNavigationStateChangedCallback
        ack_callback) {
  run_loop->Quit();
  before_ack_callback.Run(change, std::move(ack_callback));
}

}  // namespace

TestNavigationObserver::TestNavigationObserver() {
  before_ack_ = base::BindRepeating(
      [](const chromium::web::NavigationEvent&,
         OnNavigationStateChangedCallback callback) { callback(); });
}

TestNavigationObserver::~TestNavigationObserver() = default;

void TestNavigationObserver::RunUntilNavigationEquals(
    const GURL& expected_url,
    const base::Optional<std::string>& expected_title) {
  DCHECK(expected_url.is_valid());
  DCHECK(before_ack_);

  // Spin the runloop until the expected condition is met.
  while (expected_url != current_url_ ||
         (expected_title && *expected_title != current_title_)) {
    base::RunLoop run_loop;
    base::AutoReset<BeforeAckCallback> callback_setter(
        &before_ack_,
        base::BindRepeating(&QuitRunLoopAndRunCallback,
                            base::Unretained(&run_loop), before_ack_));
    run_loop.Run();
  }
}

void TestNavigationObserver::OnNavigationStateChanged(
    chromium::web::NavigationEvent change,
    OnNavigationStateChangedCallback callback) {
  DCHECK(before_ack_);

  // Update our local cache of the Frame's current state.
  if (!change.url.is_null())
    current_url_ = GURL(*change.url);
  if (!change.title.is_null())
    current_title_ = *change.title;

  // Signal readiness for the next navigation event.
  before_ack_.Run(change, std::move(callback));
}

void TestNavigationObserver::SetBeforeAckHook(BeforeAckCallback send_ack_cb) {
  DCHECK(send_ack_cb);
  before_ack_ = send_ack_cb;
}

}  // namespace cr_fuchsia
