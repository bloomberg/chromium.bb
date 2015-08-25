// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/navigation_tracker.h"

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"

namespace {

const std::string kDummyFrameName = "chromedriver dummy frame";
const std::string kDummyFrameUrl = "about:blank";
const std::string kUnreachableWebDataURL = "data:text/html,chromewebdata";

}  // namespace

NavigationTracker::NavigationTracker(DevToolsClient* client,
                                     const BrowserInfo* browser_info)
    : client_(client),
      loading_state_(kUnknown),
      browser_info_(browser_info),
      dummy_execution_context_id_(0),
      load_event_fired_(true),
      timed_out_(false) {
  client_->AddListener(this);
}

NavigationTracker::NavigationTracker(DevToolsClient* client,
                                     LoadingState known_state,
                                     const BrowserInfo* browser_info)
    : client_(client),
      loading_state_(known_state),
      browser_info_(browser_info),
      dummy_execution_context_id_(0),
      load_event_fired_(true),
      timed_out_(false) {
  client_->AddListener(this);
}

NavigationTracker::~NavigationTracker() {}

Status NavigationTracker::IsPendingNavigation(const std::string& frame_id,
                                              bool* is_pending) {
  if (!IsExpectingFrameLoadingEvents()) {
    // Some DevTools commands (e.g. Input.dispatchMouseEvent) are handled in the
    // browser process, and may cause the renderer process to start a new
    // navigation. We need to call Runtime.evaluate to force a roundtrip to the
    // renderer process, and make sure that we notice any pending navigations
    // (see crbug.com/524079).
    base::DictionaryValue params;
    params.SetString("expression", "1");
    scoped_ptr<base::DictionaryValue> result;
    Status status = client_->SendCommandAndGetResult(
        "Runtime.evaluate", params, &result);
    int value = 0;
    if (status.code() == kDisconnected) {
      // If we receive a kDisconnected status code from Runtime.evaluate, don't
      // wait for pending navigations to complete, since we won't see any more
      // events from it until we reconnect.
      *is_pending = false;
      return Status(kOk);
    } else if (status.IsError() ||
               !result->GetInteger("result.value", &value) ||
               value != 1) {
      return Status(kUnknownError, "cannot determine loading status", status);
    }
  }

  if (loading_state_ == kUnknown) {
    // In the case that a http request is sent to server to fetch the page
    // content and the server hasn't responded at all, a dummy page is created
    // for the new window. In such case, the baseURL will be empty.
    base::DictionaryValue empty_params;
    scoped_ptr<base::DictionaryValue> result;
    Status status = client_->SendCommandAndGetResult(
        "DOM.getDocument", empty_params, &result);
    std::string base_url;
    if (status.IsError() || !result->GetString("root.baseURL", &base_url))
      return Status(kUnknownError, "cannot determine loading status", status);
    if (base_url.empty()) {
      *is_pending = true;
      loading_state_ = kLoading;
      return Status(kOk);
    }

    // If the loading state is unknown (which happens after first connecting),
    // force loading to start and set the state to loading. This will cause a
    // frame start event to be received, and the frame stop event will not be
    // received until all frames are loaded.  Loading is forced to start by
    // attaching a temporary iframe.  Forcing loading to start is not
    // necessary if the main frame is not yet loaded.
    const std::string kStartLoadingIfMainFrameNotLoading =
       "var isLoaded = document.readyState == 'complete' ||"
       "    document.readyState == 'interactive';"
       "if (isLoaded) {"
       "  var frame = document.createElement('iframe');"
       "  frame.name = '" + kDummyFrameName + "';"
       "  frame.src = '" + kDummyFrameUrl + "';"
       "  document.body.appendChild(frame);"
       "  window.setTimeout(function() {"
       "    document.body.removeChild(frame);"
       "  }, 0);"
       "}";
    base::DictionaryValue params;
    params.SetString("expression", kStartLoadingIfMainFrameNotLoading);
    status = client_->SendCommandAndGetResult(
        "Runtime.evaluate", params, &result);
    if (status.IsError())
      return Status(kUnknownError, "cannot determine loading status", status);

    // Between the time the JavaScript is evaluated and
    // SendCommandAndGetResult returns, OnEvent may have received info about
    // the loading state.  This is only possible during a nested command. Only
    // set the loading state if the loading state is still unknown.
    if (loading_state_ == kUnknown)
      loading_state_ = kLoading;
  }
  *is_pending = loading_state_ == kLoading;

  if (frame_id.empty()) {
    *is_pending |= scheduled_frame_set_.size() > 0;
    *is_pending |= pending_frame_set_.size() > 0;
  } else {
    *is_pending |= scheduled_frame_set_.count(frame_id) > 0;
    *is_pending |= pending_frame_set_.count(frame_id) > 0;
  }
  return Status(kOk);
}

void NavigationTracker::set_timed_out(bool timed_out) {
  timed_out_ = timed_out;
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
    if (expecting_single_stop_event)
      pending_frame_set_.clear();
    if (pending_frame_set_.empty() &&
        (load_event_fired_ || timed_out_ || execution_context_set_.empty()))
      loading_state_ = kNotLoading;
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

    const base::Value* unused_value;
    if (!params.Get("frame.parentId", &unused_value)) {
      if (IsExpectingFrameLoadingEvents()) {
        // If the main frame just navigated, discard any pending scheduled
        // navigations. For some reasons at times the cleared event is not
        // received when navigating.
        // See crbug.com/180742.
        pending_frame_set_.clear();
        scheduled_frame_set_.clear();
      } else {
        // If the URL indicates that the web page is unreachable (the sad tab
        // page) then discard any pending or scheduled navigations.
        std::string frame_url;
        if (!params.GetString("frame.url", &frame_url))
          return Status(kUnknownError, "missing or invalid 'frame.url'");
        if (frame_url == kUnreachableWebDataURL) {
          pending_frame_set_.clear();
          scheduled_frame_set_.clear();
        }
      }
    } else {
      // If a child frame just navigated, check if it is the dummy frame that
      // was attached by IsPendingNavigation(). We don't want to track execution
      // contexts created and destroyed for this dummy frame.
      std::string name;
      if (!params.GetString("frame.name", &name))
        return Status(kUnknownError, "missing or invalid 'frame.name'");
      std::string url;
      if (!params.GetString("frame.url", &url))
        return Status(kUnknownError, "missing or invalid 'frame.url'");
      if (name == kDummyFrameName && url == kDummyFrameUrl)
        params.GetString("frame.id", &dummy_frame_id_);
    }
  } else if (method == "Runtime.executionContextsCleared") {
    if (!IsExpectingFrameLoadingEvents()) {
      execution_context_set_.clear();
      ResetLoadingState(kLoading);
      load_event_fired_ = false;
    }
  } else if (method == "Runtime.executionContextCreated") {
    if (!IsExpectingFrameLoadingEvents()) {
      int execution_context_id;
      if (!params.GetInteger("context.id", &execution_context_id))
        return Status(kUnknownError, "missing or invalid 'context.id'");
      std::string frame_id;
      if (!params.GetString("context.frameId", &frame_id))
        return Status(kUnknownError, "missing or invalid 'context.frameId'");
      if (frame_id == dummy_frame_id_)
        dummy_execution_context_id_ = execution_context_id;
      else
        execution_context_set_.insert(execution_context_id);
    }
  } else if (method == "Runtime.executionContextDestroyed") {
    if (!IsExpectingFrameLoadingEvents()) {
      int execution_context_id;
      if (!params.GetInteger("executionContextId", &execution_context_id))
        return Status(kUnknownError, "missing or invalid 'context.id'");
      execution_context_set_.erase(execution_context_id);
      if (execution_context_id != dummy_execution_context_id_) {
        if (execution_context_set_.empty()) {
          loading_state_ = kLoading;
          load_event_fired_ = false;
          dummy_frame_id_ = std::string();
          dummy_execution_context_id_ = 0;
        }
      }
    }
  } else if (method == "Page.loadEventFired") {
    if (!IsExpectingFrameLoadingEvents())
      load_event_fired_ = true;
  } else if (method == "Inspector.targetCrashed") {
    ResetLoadingState(kNotLoading);
  }
  return Status(kOk);
}

Status NavigationTracker::OnCommandSuccess(
    DevToolsClient* client,
    const std::string& method,
    const base::DictionaryValue& result) {
  if (!IsExpectingFrameLoadingEvents()) {
    if (method == "Page.navigate" && loading_state_ == kLoading) {
      // In all versions of Chrome that are supported by ChromeDriver, except
      // for old versions of Android WebView, Page.navigate will return a
      // frameId in the command response. We'll get a notification that the
      // frame has loaded when we get the Page.frameStoppedLoading event, so
      // keep track of the pending frame id here.
      std::string pending_frame_id;
      if (result.GetString("frameId", &pending_frame_id)) {
        pending_frame_set_.insert(pending_frame_id);
        return Status(kOk);
      }
    }
  }

  if ((method == "Page.navigate" || method == "Page.navigateToHistoryEntry") &&
      loading_state_ != kLoading) {
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

bool NavigationTracker::IsExpectingFrameLoadingEvents() {
  // As of crrev.com/323900 (Chrome 44+) we no longer receive
  // Page.frameStartedLoading and Page.frameStoppedLoading events immediately
  // after clicking on an element. This can cause tests to fail if the click
  // command results in a page navigation, such as in the case of
  // PageLoadingTest.testShouldTimeoutIfAPageTakesTooLongToLoadAfterClick from
  // the Selenium test suite.
  // TODO(samuong): Remove this once we stop supporting M43.
  if (browser_info_->browser_name == "chrome")
    return browser_info_->build_no < 2358;
  else
    return browser_info_->major_version < 44;
}
