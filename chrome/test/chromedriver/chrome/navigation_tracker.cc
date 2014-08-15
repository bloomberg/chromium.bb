// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/navigation_tracker.h"

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/version.h"

NavigationTracker::NavigationTracker(DevToolsClient* client,
                                     const BrowserInfo* browser_info)
    : client_(client),
      loading_state_(kUnknown),
      browser_info_(browser_info) {
  client_->AddListener(this);
}

NavigationTracker::NavigationTracker(DevToolsClient* client,
                                     LoadingState known_state,
                                     const BrowserInfo* browser_info)
    : client_(client),
      loading_state_(known_state),
      browser_info_(browser_info) {
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
  *is_pending = loading_state_ == kLoading;
  if (frame_id.empty())
    *is_pending |= scheduled_frame_set_.size() > 0;
  else
    *is_pending |= scheduled_frame_set_.count(frame_id) > 0;
  return Status(kOk);
}

Status NavigationTracker::OnConnected(DevToolsClient* client) {
  ResetLoadingState(kUnknown);

  // Enable page domain notifications to allow tracking navigation state.
  base::DictionaryValue empty_params;
  return client_->SendCommand("Page.enable", empty_params);
}

Status NavigationTracker::OnEvent(DevToolsClient* client,
                                  const std::string& method,
                                  const base::DictionaryValue& params) {
  if (method == "Page.frameStartedLoading") {
    std::string frame_id;
    if (!params.GetString("frameId", &frame_id))
      return Status(kUnknownError, "missing or invalid 'frameId'");
    pending_frame_set_.insert(frame_id);
    loading_state_ = kLoading;
  } else if (method == "Page.frameStoppedLoading") {
    // Versions of Blink before revision 170248 sent a single
    // Page.frameStoppedLoading event per page, but 170248 and newer revisions
    // only send one event for each frame on the page.
    //
    // This change was rolled into the Chromium tree in revision 260203.
    // Versions of Chrome with build number 1916 and earlier do not contain this
    // change.
    bool expecting_single_stop_event = false;

    if (browser_info_->browser_name == "chrome") {
      // If we're talking to a version of Chrome with an old build number, we
      // are using a branched version of Blink which does not contain 170248
      // (even if blink_revision > 170248).
      expecting_single_stop_event = browser_info_->build_no <= 1916;
    } else {
      // If we're talking to a non-Chrome embedder (e.g. Content Shell, Android
      // WebView), assume that the browser does not use a branched version of
      // Blink.
      expecting_single_stop_event = browser_info_->blink_revision < 170248;
    }

    std::string frame_id;
    if (!params.GetString("frameId", &frame_id))
      return Status(kUnknownError, "missing or invalid 'frameId'");

    pending_frame_set_.erase(frame_id);

    if (pending_frame_set_.empty() || expecting_single_stop_event) {
      pending_frame_set_.clear();
      loading_state_ = kNotLoading;
    }
  } else if (method == "Page.frameScheduledNavigation") {
    double delay;
    if (!params.GetDouble("delay", &delay))
      return Status(kUnknownError, "missing or invalid 'delay'");

    std::string frame_id;
    if (!params.GetString("frameId", &frame_id))
      return Status(kUnknownError, "missing or invalid 'frameId'");

    // WebDriver spec says to ignore redirects over 1s.
    if (delay > 1)
      return Status(kOk);
    scheduled_frame_set_.insert(frame_id);
  } else if (method == "Page.frameClearedScheduledNavigation") {
    std::string frame_id;
    if (!params.GetString("frameId", &frame_id))
      return Status(kUnknownError, "missing or invalid 'frameId'");

    scheduled_frame_set_.erase(frame_id);
  } else if (method == "Page.frameNavigated") {
    // Note: in some cases Page.frameNavigated may be received for subframes
    // without a frameStoppedLoading (for example cnn.com).

    // If the main frame just navigated, discard any pending scheduled
    // navigations. For some reasons at times the cleared event is not
    // received when navigating.
    // See crbug.com/180742.
    const base::Value* unused_value;
    if (!params.Get("frame.parentId", &unused_value)) {
      pending_frame_set_.clear();
      scheduled_frame_set_.clear();
    }
  } else if (method == "Inspector.targetCrashed") {
    ResetLoadingState(kNotLoading);
  }
  return Status(kOk);
}

Status NavigationTracker::OnCommandSuccess(DevToolsClient* client,
                                           const std::string& method) {
  if (method == "Page.navigate" && loading_state_ != kLoading) {
    // At this point the browser has initiated the navigation, but besides that,
    // it is unknown what will happen.
    //
    // There are a few cases (perhaps more):
    // 1 The RenderFrameHost has already queued FrameMsg_Navigate and loading
    //   will start shortly.
    // 2 The RenderFrameHost has already queued FrameMsg_Navigate and loading
    //   will never start because it is just an in-page fragment navigation.
    // 3 The RenderFrameHost is suspended and hasn't queued FrameMsg_Navigate
    //   yet. This happens for cross-site navigations. The RenderFrameHost
    //   will not queue FrameMsg_Navigate until it is ready to unload the
    //   previous page (after running unload handlers and such).
    // TODO(nasko): Revisit case 3, since now unload handlers are run in the
    // background. http://crbug.com/323528.
    //
    // To determine whether a load is expected, do a round trip to the
    // renderer to ask what the URL is.
    // If case #1, by the time the command returns, the frame started to load
    // event will also have been received, since the DevTools command will
    // be queued behind FrameMsg_Navigate.
    // If case #2, by the time the command returns, the navigation will
    // have already happened, although no frame start/stop events will have
    // been received.
    // If case #3, the URL will be blank if the navigation hasn't been started
    // yet. In that case, expect a load to happen in the future.
    loading_state_ = kUnknown;
    base::DictionaryValue params;
    params.SetString("expression", "document.URL");
    scoped_ptr<base::DictionaryValue> result;
    Status status = client_->SendCommandAndGetResult(
        "Runtime.evaluate", params, &result);
    std::string url;
    if (status.IsError() || !result->GetString("result.value", &url))
      return Status(kUnknownError, "cannot determine loading status", status);
    if (loading_state_ == kUnknown && url.empty())
      loading_state_ = kLoading;
  }
  return Status(kOk);
}

void NavigationTracker::ResetLoadingState(LoadingState loading_state) {
  loading_state_ = loading_state;
  pending_frame_set_.clear();
  scheduled_frame_set_.clear();
}
