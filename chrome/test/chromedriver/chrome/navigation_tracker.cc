// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/navigation_tracker.h"

#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"

NavigationTracker::NavigationTracker(DevToolsClient* client)
    : client_(client),
      loading_state_(kUnknown) {
  client_->AddListener(this);
}

NavigationTracker::NavigationTracker(DevToolsClient* client,
                                     LoadingState known_state)
    : client_(client),
      loading_state_(known_state) {
  client_->AddListener(this);
}

NavigationTracker::~NavigationTracker() {}

Status NavigationTracker::IsPendingNavigation(const std::string& frame_id,
                                              bool* is_pending) {
  if (loading_state_ == kUnknown) {
    // If the loading state is unknown (which happens after first connecting),
    // force loading to start and set the state to loading. This will
    // cause a frame start event to be received, and the frame stop event
    // will not be received until all frames are loaded.
    // Loading is forced to start by attaching a temporary iframe.
    // Forcing loading to start is not necessary if the main frame is not yet
    // loaded.
    const char kStartLoadingIfMainFrameNotLoading[] =
       "var isLoaded = document.readyState == 'complete' ||"
       "    document.readyState == 'interactive';"
       "if (isLoaded) {"
       "  var frame = document.createElement('iframe');"
       "  frame.src = 'about:blank';"
       "  document.body.appendChild(frame);"
       "  window.setTimeout(function() {"
       "    document.body.removeChild(frame);"
       "  }, 0);"
       "}";
    base::DictionaryValue params;
    params.SetString("expression", kStartLoadingIfMainFrameNotLoading);
    scoped_ptr<base::DictionaryValue> result;
    Status status = client_->SendCommandAndGetResult(
        "Runtime.evaluate", params, &result);
    if (status.IsError())
      return Status(kUnknownError, "cannot determine loading status", status);

    // Between the time the JavaScript is evaluated and SendCommandAndGetResult
    // returns, OnEvent may have received info about the loading state.
    // This is only possible during a nested command. Only set the loading state
    // if the loading state is still unknown.
    if (loading_state_ == kUnknown)
      loading_state_ = kLoading;
  }
  *is_pending = (loading_state_ == kLoading) ||
                scheduled_frame_set_.count(frame_id) > 0;
  return Status(kOk);
}

Status NavigationTracker::OnConnected() {
  loading_state_ = kUnknown;
  scheduled_frame_set_.clear();

  // Enable page domain notifications to allow tracking navigation state.
  base::DictionaryValue empty_params;
  return client_->SendCommand("Page.enable", empty_params);
}

void NavigationTracker::OnEvent(const std::string& method,
                                const base::DictionaryValue& params) {
  // Chrome does not send Page.frameStoppedLoading until all frames have
  // run their onLoad handlers (including frames created during the handlers).
  // When it does, it only sends one stopped event for all frames.
  if (method == "Page.frameStartedLoading") {
    loading_state_ = kLoading;
  } else if (method == "Page.frameStoppedLoading") {
    loading_state_ = kNotLoading;
  } else if (method == "Page.frameScheduledNavigation") {
    double delay;
    if (!params.GetDouble("delay", &delay)) {
      LOG(ERROR) << "missing or invalid 'delay'";
      return;
    }
    std::string frame_id;
    if (!params.GetString("frameId", &frame_id)) {
      LOG(ERROR) << "missing or invalid 'frameId'";
      return;
    }
    // WebDriver spec says to ignore redirects over 1s.
    if (delay > 1)
      return;
    scheduled_frame_set_.insert(frame_id);
  } else if (method == "Page.frameClearedScheduledNavigation") {
    std::string frame_id;
    if (!params.GetString("frameId", &frame_id)) {
      LOG(ERROR) << "missing or invalid 'frameId'";
      return;
    }
    scheduled_frame_set_.erase(frame_id);
  } else if (method == "Page.frameNavigated") {
    // Note: in some cases Page.frameNavigated may be received for subframes
    // without a frameStoppedLoading (for example cnn.com).

    // If the main frame just navigated, discard any pending scheduled
    // navigations. For some reasons at times the cleared event is not
    // received when navigating.
    // See crbug.com/180742.
    const base::Value* unused_value;
    if (!params.Get("frame.parentId", &unused_value))
      scheduled_frame_set_.clear();
  }
}
