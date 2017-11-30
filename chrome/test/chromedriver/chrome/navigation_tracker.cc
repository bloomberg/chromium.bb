// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/navigation_tracker.h"

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/javascript_dialog_manager.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/net/timeout.h"

namespace {

const char kDummyFrameName[] = "chromedriver dummy frame";
const char kDummyFrameUrl[] = "about:blank";

// The error page URL was renamed in
// https://chromium-review.googlesource.com/c/580169, but because ChromeDriver
// needs to be backward-compatible with older versions of Chrome, it is
// necessary to compare against both the old and new error URL.
const char kUnreachableWebDataURL[] = "chrome-error://chromewebdata/";
const char kDeprecatedUnreachableWebDataURL[] = "data:text/html,chromewebdata";

const char kAutomationExtensionBackgroundPage[] =
    "chrome-extension://aapnijgdinlhnhlmodcfapnahmbfebeb/"
    "_generated_background_page.html";

Status MakeNavigationCheckFailedStatus(Status command_status) {
  if (command_status.code() == kUnexpectedAlertOpen)
    return Status(kUnexpectedAlertOpen);
  else if (command_status.code() == kTimeout)
    return Status(kTimeout);
  else
    return Status(kUnknownError, "cannot determine loading status",
                  command_status);
}

}  // namespace

NavigationTracker::NavigationTracker(
    DevToolsClient* client,
    const BrowserInfo* browser_info,
    const JavaScriptDialogManager* dialog_manager)
    : client_(client),
      loading_state_(kUnknown),
      browser_info_(browser_info),
      dialog_manager_(dialog_manager),
      dummy_execution_context_id_(0),
      load_event_fired_(true),
      timed_out_(false) {
  client_->AddListener(this);
}

NavigationTracker::NavigationTracker(
    DevToolsClient* client,
    LoadingState known_state,
    const BrowserInfo* browser_info,
    const JavaScriptDialogManager* dialog_manager)
    : client_(client),
      loading_state_(known_state),
      browser_info_(browser_info),
      dialog_manager_(dialog_manager),
      dummy_execution_context_id_(0),
      load_event_fired_(true),
      timed_out_(false) {
  client_->AddListener(this);
}

NavigationTracker::~NavigationTracker() {}

Status NavigationTracker::IsPendingNavigation(const std::string& frame_id,
                                              const Timeout* timeout,
                                              bool* is_pending) {
  if (!IsExpectingFrameLoadingEvents()) {
    if (IsEventLoopPausedByDialogs() && dialog_manager_->IsDialogOpen()) {
      // The render process is paused while modal dialogs are open, so
      // Runtime.evaluate will block and time out if we attempt to call it. In
      // this case we can consider the page to have loaded, so that we return
      // control back to the test and let it dismiss the dialog.
      *is_pending = false;
      return Status(kOk);
    }

    // Some DevTools commands (e.g. Input.dispatchMouseEvent) are handled in the
    // browser process, and may cause the renderer process to start a new
    // navigation. We need to call Runtime.evaluate to force a roundtrip to the
    // renderer process, and make sure that we notice any pending navigations
    // (see crbug.com/524079).
    base::DictionaryValue params;
    params.SetString("expression", "1");
    std::unique_ptr<base::DictionaryValue> result;
    Status status = client_->SendCommandAndGetResultWithTimeout(
        "Runtime.evaluate", params, timeout, &result);
    int value = 0;
    if (status.code() == kDisconnected) {
      // If we receive a kDisconnected status code from Runtime.evaluate, don't
      // wait for pending navigations to complete, since we won't see any more
      // events from it until we reconnect.
      *is_pending = false;
      return Status(kOk);
    } else if (status.code() == kUnexpectedAlertOpen &&
               IsEventLoopPausedByDialogs()) {
      // The JS event loop is paused while modal dialogs are open, so return
      // control to the test so that it can dismiss the dialog.
      *is_pending = false;
      return Status(kOk);
    } else if (status.IsError() && dialog_manager_->IsDialogOpen()) {
      // TODO(samuong): Remove when we stop supporting M51.
      // When a dialog is open, DevTools returns "Internal error: result is not
      // an Object" for this request. If this happens, we assume that any
      // cross-process navigation has started, and determine whether a
      // navigation is pending based on the number of scheduled and pending
      // frames.
      LOG(WARNING) << "Failed to evaluate expression while dialog was open";
    } else if (status.IsError() ||
               !result->GetInteger("result.value", &value) ||
               value != 1) {
      return MakeNavigationCheckFailedStatus(status);
    }
  }

  if (loading_state_ == kUnknown) {
    // In the case that a http request is sent to server to fetch the page
    // content and the server hasn't responded at all, a dummy page is created
    // for the new window. In such case, the baseURL will be empty for <=M59 ;
    // whereas the baseURL will be 'about:blank' for >=M60. See crbug/711562.
    // TODO(gmanikpure):Remove condition for <3076 when we stop supporting M59.
    base::DictionaryValue empty_params;
    std::unique_ptr<base::DictionaryValue> result;
    Status status = client_->SendCommandAndGetResultWithTimeout(
        "DOM.getDocument", empty_params, timeout, &result);
    std::string base_url;
    std::string doc_url;
    if (status.IsError() || !result->GetString("root.baseURL", &base_url) ||
        !result->GetString("root.documentURL", &doc_url))
      return MakeNavigationCheckFailedStatus(status);

    bool condition;
    if (browser_info_->build_no >= 3076)
      condition = doc_url != "about:blank" && base_url == "about:blank";
    else
      condition = base_url.empty();

    if (condition) {
      *is_pending = true;
      loading_state_ = kLoading;
      return Status(kOk);
    }

    // If we're loading the ChromeDriver automation extension background page,
    // look for a known function to determine the loading status.
    if (base_url == kAutomationExtensionBackgroundPage) {
      bool function_exists = false;
      status = CheckFunctionExists(timeout, &function_exists);
      if (status.IsError())
        return MakeNavigationCheckFailedStatus(status);
      loading_state_ = function_exists ? kNotLoading : kLoading;
    }

    // If the loading state is unknown (which happens after first connecting),
    // force loading to start and set the state to loading. This will cause a
    // frame start event to be received, and the frame stop event will not be
    // received until all frames are loaded.  Loading is forced to start by
    // attaching a temporary iframe.  Forcing loading to start is not
    // necessary if the main frame is not yet loaded.
    const std::string kStartLoadingIfMainFrameNotLoading = base::StringPrintf(
       "var isLoaded = document.readyState == 'complete' ||"
       "    document.readyState == 'interactive';"
       "if (isLoaded) {"
       "  var frame = document.createElement('iframe');"
       "  frame.name = '%s';"
       "  frame.src = '%s';"
       "  document.body.appendChild(frame);"
       "  window.setTimeout(function() {"
       "    document.body.removeChild(frame);"
       "  }, 0);"
       "}", kDummyFrameName, kDummyFrameUrl);
    base::DictionaryValue params;
    params.SetString("expression", kStartLoadingIfMainFrameNotLoading);
    status = client_->SendCommandAndGetResultWithTimeout(
        "Runtime.evaluate", params, timeout, &result);
    if (status.IsError())
      return MakeNavigationCheckFailedStatus(status);

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

Status NavigationTracker::CheckFunctionExists(const Timeout* timeout,
                                              bool* exists) {
  base::DictionaryValue params;
  params.SetString("expression", "typeof(getWindowInfo)");
  std::unique_ptr<base::DictionaryValue> result;
  Status status = client_->SendCommandAndGetResultWithTimeout(
      "Runtime.evaluate", params, timeout, &result);
  std::string type;
  if (status.IsError() || !result->GetString("result.value", &type))
    return MakeNavigationCheckFailedStatus(status);
  *exists = type == "function";
  return Status(kOk);
}

void NavigationTracker::set_timed_out(bool timed_out) {
  timed_out_ = timed_out;
}

bool NavigationTracker::IsNonBlocking() const {
  return false;
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

    if (browser_info_->major_version >= 63) {
      // Check if the document is really loading.
      base::DictionaryValue params;
      params.SetString("expression", "document.readyState");
      std::unique_ptr<base::DictionaryValue> result;
      Status status =
          client_->SendCommandAndGetResult("Runtime.evaluate", params, &result);
      std::string value;
      if (status.IsError() || !result->GetString("result.value", &value)) {
        LOG(ERROR) << "Unable to retrieve document state " << status.message();
        return status;
      }
      if (value == "complete") {
        pending_frame_set_.erase(frame_id);
        loading_state_ = kNotLoading;
      }
    }
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

    scheduled_frame_set_.erase(frame_id);
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

    // A normal Page.loadEventFired event isn't expected after a scheduled
    // navigation, so set load_event_fired_ flag.
    load_event_fired_ = true;
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
        // Discard pending and scheduled frames, except for the root frame,
        // which just navigated (and which we should consider pending until we
        // receive a Page.frameStoppedLoading event for it).
        std::string frame_id;
        if (!params.GetString("frame.id", &frame_id))
          return Status(kUnknownError, "missing or invalid 'frame.id'");
        bool frame_was_pending = pending_frame_set_.count(frame_id) > 0;
        pending_frame_set_.clear();
        scheduled_frame_set_.clear();
        if (frame_was_pending)
          pending_frame_set_.insert(frame_id);
        // If the URL indicates that the web page is unreachable (the sad tab
        // page) then discard all pending navigations.
        std::string frame_url;
        if (!params.GetString("frame.url", &frame_url))
          return Status(kUnknownError, "missing or invalid 'frame.url'");
        if (frame_url == kUnreachableWebDataURL ||
            frame_url == kDeprecatedUnreachableWebDataURL)
          pending_frame_set_.clear();
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
      load_event_fired_ = false;
      if (browser_info_->build_no >= 2685 && execution_context_set_.empty()) {
        // As of crrev.com/382211, DevTools sends an executionContextsCleared
        // event right before the first execution context is created, but after
        // Page.loadEventFired. Set the loading state to loading, but do not
        // clear the pending and scheduled frame sets, since they may contain
        // frames that we're still waiting for.
        loading_state_ = kLoading;
      } else {
        // In older browser versions, this event signifies the first event after
        // a cross-process navigation. Set the loading state, and clear any
        // pending or scheduled frames, since we're about to navigate away from
        // them.
        ResetLoadingState(kLoading);
      }
    }
  } else if (method == "Runtime.executionContextCreated") {
    if (!IsExpectingFrameLoadingEvents()) {
      int execution_context_id;
      if (!params.GetInteger("context.id", &execution_context_id))
        return Status(kUnknownError, "missing or invalid 'context.id'");
      std::string frame_id;
      if (!params.GetString("context.auxData.frameId", &frame_id)) {
        // TODO(samuong): remove this when we stop supporting Chrome 53.
        if (!params.GetString("context.frameId", &frame_id))
          return Status(kUnknownError, "missing or invalid 'context.frameId'");
      }
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
    const base::DictionaryValue& result,
    const Timeout& command_timeout) {
  // Check for start of navigation. In some case response to navigate is delayed
  // until after the command has already timed out, in which case it has already
  // been cancelled or will be cancelled soon, and should be ignored.
  if ((method == "Page.navigate" || method == "Page.navigateToHistoryEntry") &&
      loading_state_ != kLoading && !command_timeout.IsExpired()) {
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
    std::unique_ptr<base::DictionaryValue> result;
    Status status = client_->SendCommandAndGetResultWithTimeout(
        "Runtime.evaluate", params, &command_timeout, &result);
    std::string url;
    if (status.IsError() || !result->GetString("result.value", &url))
      return MakeNavigationCheckFailedStatus(status);
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
  if (browser_info_->browser_name == "webview")
    return browser_info_->major_version < 44;
  else
    return browser_info_->build_no < 2358;
}

bool NavigationTracker::IsEventLoopPausedByDialogs() {
  // As of crrev.com/394883 (Chrome 52+) the JavaScript event loop is paused by
  // modal dialogs. This pauses the render process so we need to be careful not
  // to issue Runtime.evaluate commands while an alert is up, otherwise the call
  // will block and timeout. For details refer to
  // https://bugs.chromium.org/p/chromedriver/issues/detail?id=1381.
  // TODO(samuong): Remove this once we stop supporting M51.
  if (browser_info_->browser_name == "webview")
    return browser_info_->major_version >= 52;
  else
    return browser_info_->build_no >= 2743;
}
