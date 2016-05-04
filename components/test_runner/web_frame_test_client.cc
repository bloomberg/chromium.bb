// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test_runner/web_frame_test_client.h"

#include <memory>

#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/test_runner/accessibility_controller.h"
#include "components/test_runner/event_sender.h"
#include "components/test_runner/mock_color_chooser.h"
#include "components/test_runner/mock_screen_orientation_client.h"
#include "components/test_runner/mock_web_user_media_client.h"
#include "components/test_runner/test_common.h"
#include "components/test_runner/test_interfaces.h"
#include "components/test_runner/test_plugin.h"
#include "components/test_runner/test_runner.h"
#include "components/test_runner/web_frame_test_proxy.h"
#include "components/test_runner/web_test_delegate.h"
#include "components/test_runner/web_test_proxy.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNavigationPolicy.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace test_runner {

namespace {

void PrintFrameDescription(WebTestDelegate* delegate, blink::WebFrame* frame) {
  std::string name8 = frame->uniqueName().utf8();
  if (frame == frame->view()->mainFrame()) {
    if (!name8.length()) {
      delegate->PrintMessage("main frame");
      return;
    }
    delegate->PrintMessage(std::string("main frame \"") + name8 + "\"");
    return;
  }
  if (!name8.length()) {
    delegate->PrintMessage("frame (anonymous)");
    return;
  }
  delegate->PrintMessage(std::string("frame \"") + name8 + "\"");
}

void PrintFrameuserGestureStatus(WebTestDelegate* delegate,
                                 blink::WebFrame* frame,
                                 const char* msg) {
  bool is_user_gesture =
      blink::WebUserGestureIndicator::isProcessingUserGesture();
  delegate->PrintMessage(std::string("Frame with user gesture \"") +
                         (is_user_gesture ? "true" : "false") + "\"" + msg);
}

// Used to write a platform neutral file:/// URL by taking the
// filename and its directory. (e.g., converts
// "file:///tmp/foo/bar.txt" to just "bar.txt").
std::string DescriptionSuitableForTestResult(const std::string& url) {
  if (url.empty() || std::string::npos == url.find("file://"))
    return url;

  size_t pos = url.rfind('/');
  if (pos == std::string::npos || !pos)
    return "ERROR:" + url;
  pos = url.rfind('/', pos - 1);
  if (pos == std::string::npos)
    return "ERROR:" + url;

  return url.substr(pos + 1);
}

void PrintResponseDescription(WebTestDelegate* delegate,
                              const blink::WebURLResponse& response) {
  if (response.isNull()) {
    delegate->PrintMessage("(null)");
    return;
  }
  delegate->PrintMessage(base::StringPrintf(
      "<NSURLResponse %s, http status code %d>",
      DescriptionSuitableForTestResult(response.url().string().utf8()).c_str(),
      response.httpStatusCode()));
}

std::string PriorityDescription(
    const blink::WebURLRequest::Priority& priority) {
  switch (priority) {
    case blink::WebURLRequest::PriorityVeryLow:
      return "VeryLow";
    case blink::WebURLRequest::PriorityLow:
      return "Low";
    case blink::WebURLRequest::PriorityMedium:
      return "Medium";
    case blink::WebURLRequest::PriorityHigh:
      return "High";
    case blink::WebURLRequest::PriorityVeryHigh:
      return "VeryHigh";
    case blink::WebURLRequest::PriorityUnresolved:
    default:
      return "Unresolved";
  }
}

void BlockRequest(blink::WebURLRequest& request) {
  request.setURL(GURL("255.255.255.255"));
}

bool IsLocalHost(const std::string& host) {
  return host == "127.0.0.1" || host == "localhost" || host == "[::1]";
}

bool IsTestHost(const std::string& host) {
  return base::EndsWith(host, ".test", base::CompareCase::INSENSITIVE_ASCII);
}

bool HostIsUsedBySomeTestsToGenerateError(const std::string& host) {
  return host == "255.255.255.255";
}

// Used to write a platform neutral file:/// URL by only taking the filename
// (e.g., converts "file:///tmp/foo.txt" to just "foo.txt").
std::string URLSuitableForTestResult(const std::string& url) {
  if (url.empty() || std::string::npos == url.find("file://"))
    return url;

  size_t pos = url.rfind('/');
  if (pos == std::string::npos) {
#ifdef WIN32
    pos = url.rfind('\\');
    if (pos == std::string::npos)
      pos = 0;
#else
    pos = 0;
#endif
  }
  std::string filename = url.substr(pos + 1);
  if (filename.empty())
    return "file:";  // A WebKit test has this in its expected output.
  return filename;
}

// WebNavigationType debugging strings taken from PolicyDelegate.mm.
const char* kLinkClickedString = "link clicked";
const char* kFormSubmittedString = "form submitted";
const char* kBackForwardString = "back/forward";
const char* kReloadString = "reload";
const char* kFormResubmittedString = "form resubmitted";
const char* kOtherString = "other";
const char* kIllegalString = "illegal value";

// Get a debugging string from a WebNavigationType.
const char* WebNavigationTypeToString(blink::WebNavigationType type) {
  switch (type) {
    case blink::WebNavigationTypeLinkClicked:
      return kLinkClickedString;
    case blink::WebNavigationTypeFormSubmitted:
      return kFormSubmittedString;
    case blink::WebNavigationTypeBackForward:
      return kBackForwardString;
    case blink::WebNavigationTypeReload:
      return kReloadString;
    case blink::WebNavigationTypeFormResubmitted:
      return kFormResubmittedString;
    case blink::WebNavigationTypeOther:
      return kOtherString;
  }
  return kIllegalString;
}

}  // namespace

WebFrameTestClient::WebFrameTestClient(
    TestRunner* test_runner,
    WebTestDelegate* delegate,
    WebTestProxyBase* web_test_proxy_base,
    WebFrameTestProxyBase* web_frame_test_proxy_base)
    : test_runner_(test_runner),
      delegate_(delegate),
      web_test_proxy_base_(web_test_proxy_base),
      web_frame_test_proxy_base_(web_frame_test_proxy_base) {
  DCHECK(test_runner);
  DCHECK(delegate_);
  DCHECK(web_test_proxy_base_);
}

WebFrameTestClient::~WebFrameTestClient() {}

blink::WebColorChooser* WebFrameTestClient::createColorChooser(
    blink::WebColorChooserClient* client,
    const blink::WebColor& color,
    const blink::WebVector<blink::WebColorSuggestion>& suggestions) {
  // This instance is deleted by WebCore::ColorInputType
  return new MockColorChooser(client, delegate_, test_runner_);
}

void WebFrameTestClient::runModalAlertDialog(const blink::WebString& message) {
  delegate_->PrintMessage(std::string("ALERT: ") + message.utf8().data() +
                          "\n");
}

bool WebFrameTestClient::runModalConfirmDialog(
    const blink::WebString& message) {
  delegate_->PrintMessage(std::string("CONFIRM: ") + message.utf8().data() +
                          "\n");
  return true;
}

bool WebFrameTestClient::runModalPromptDialog(
    const blink::WebString& message,
    const blink::WebString& default_value,
    blink::WebString* actual_value) {
  delegate_->PrintMessage(std::string("PROMPT: ") + message.utf8().data() +
                          ", default text: " + default_value.utf8().data() +
                          "\n");
  return true;
}

bool WebFrameTestClient::runModalBeforeUnloadDialog(bool is_reload) {
  delegate_->PrintMessage(std::string("CONFIRM NAVIGATION\n"));
  return !test_runner_->shouldStayOnPageAfterHandlingBeforeUnload();
}

blink::WebScreenOrientationClient*
WebFrameTestClient::webScreenOrientationClient() {
  return test_runner_->getMockScreenOrientationClient();
}

void WebFrameTestClient::postAccessibilityEvent(const blink::WebAXObject& obj,
                                                blink::WebAXEvent event) {
  // Only hook the accessibility events occured during the test run.
  // This check prevents false positives in WebLeakDetector.
  // The pending tasks in browser/renderer message queue may trigger
  // accessibility events,
  // and AccessibilityController will hold on to their target nodes if we don't
  // ignore them here.
  if (!test_runner_->TestIsRunning())
    return;

  const char* event_name = NULL;
  switch (event) {
    case blink::WebAXEventActiveDescendantChanged:
      event_name = "ActiveDescendantChanged";
      break;
    case blink::WebAXEventAlert:
      event_name = "Alert";
      break;
    case blink::WebAXEventAriaAttributeChanged:
      event_name = "AriaAttributeChanged";
      break;
    case blink::WebAXEventAutocorrectionOccured:
      event_name = "AutocorrectionOccured";
      break;
    case blink::WebAXEventBlur:
      event_name = "Blur";
      break;
    case blink::WebAXEventCheckedStateChanged:
      event_name = "CheckedStateChanged";
      break;
    case blink::WebAXEventChildrenChanged:
      event_name = "ChildrenChanged";
      break;
    case blink::WebAXEventClicked:
      event_name = "Clicked";
      break;
    case blink::WebAXEventDocumentSelectionChanged:
      event_name = "DocumentSelectionChanged";
      break;
    case blink::WebAXEventFocus:
      event_name = "Focus";
      break;
    case blink::WebAXEventHide:
      event_name = "Hide";
      break;
    case blink::WebAXEventHover:
      event_name = "Hover";
      break;
    case blink::WebAXEventInvalidStatusChanged:
      event_name = "InvalidStatusChanged";
      break;
    case blink::WebAXEventLayoutComplete:
      event_name = "LayoutComplete";
      break;
    case blink::WebAXEventLiveRegionChanged:
      event_name = "LiveRegionChanged";
      break;
    case blink::WebAXEventLoadComplete:
      event_name = "LoadComplete";
      break;
    case blink::WebAXEventLocationChanged:
      event_name = "LocationChanged";
      break;
    case blink::WebAXEventMenuListItemSelected:
      event_name = "MenuListItemSelected";
      break;
    case blink::WebAXEventMenuListItemUnselected:
      event_name = "MenuListItemUnselected";
      break;
    case blink::WebAXEventMenuListValueChanged:
      event_name = "MenuListValueChanged";
      break;
    case blink::WebAXEventRowCollapsed:
      event_name = "RowCollapsed";
      break;
    case blink::WebAXEventRowCountChanged:
      event_name = "RowCountChanged";
      break;
    case blink::WebAXEventRowExpanded:
      event_name = "RowExpanded";
      break;
    case blink::WebAXEventScrollPositionChanged:
      event_name = "ScrollPositionChanged";
      break;
    case blink::WebAXEventScrolledToAnchor:
      event_name = "ScrolledToAnchor";
      break;
    case blink::WebAXEventSelectedChildrenChanged:
      event_name = "SelectedChildrenChanged";
      break;
    case blink::WebAXEventSelectedTextChanged:
      event_name = "SelectedTextChanged";
      break;
    case blink::WebAXEventShow:
      event_name = "Show";
      break;
    case blink::WebAXEventTextChanged:
      event_name = "TextChanged";
      break;
    case blink::WebAXEventTextInserted:
      event_name = "TextInserted";
      break;
    case blink::WebAXEventTextRemoved:
      event_name = "TextRemoved";
      break;
    case blink::WebAXEventValueChanged:
      event_name = "ValueChanged";
      break;
    default:
      event_name = "Unknown";
      break;
  }

  AccessibilityController* accessibility_controller =
      web_test_proxy_base_->accessibility_controller();
  accessibility_controller->NotificationReceived(obj, event_name);
  if (accessibility_controller->ShouldLogAccessibilityEvents()) {
    std::string message("AccessibilityNotification - ");
    message += event_name;

    blink::WebNode node = obj.node();
    if (!node.isNull() && node.isElementNode()) {
      blink::WebElement element = node.to<blink::WebElement>();
      if (element.hasAttribute("id")) {
        message += " - id:";
        message += element.getAttribute("id").utf8().data();
      }
    }

    delegate_->PrintMessage(message + "\n");
  }
}

void WebFrameTestClient::didChangeSelection(bool is_empty_callback) {
  if (test_runner_->shouldDumpEditingCallbacks())
    delegate_->PrintMessage(
        "EDITING DELEGATE: "
        "webViewDidChangeSelection:WebViewDidChangeSelectionNotification\n");
}

blink::WebPlugin* WebFrameTestClient::createPlugin(
    blink::WebLocalFrame* frame,
    const blink::WebPluginParams& params) {
  if (TestPlugin::IsSupportedMimeType(params.mimeType))
    return TestPlugin::create(frame, params, delegate_);
  return delegate_->CreatePluginPlaceholder(frame, params);
}

void WebFrameTestClient::showContextMenu(
    const blink::WebContextMenuData& context_menu_data) {
  web_test_proxy_base_->event_sender()->SetContextMenuData(context_menu_data);
}

blink::WebUserMediaClient* WebFrameTestClient::userMediaClient() {
  return test_runner_->getMockWebUserMediaClient();
}

void WebFrameTestClient::loadURLExternally(
    const blink::WebURLRequest& request,
    blink::WebNavigationPolicy policy,
    const blink::WebString& suggested_name,
    bool replaces_current_history_item) {
  if (test_runner_->shouldWaitUntilExternalURLLoad()) {
    if (policy == blink::WebNavigationPolicyDownload) {
      delegate_->PrintMessage(
          std::string("Downloading URL with suggested filename \"") +
          suggested_name.utf8() + "\"\n");
    } else {
      delegate_->PrintMessage(std::string("Loading URL externally - \"") +
                              URLDescription(request.url()) + "\"\n");
    }
    delegate_->TestFinished();
  }
}

void WebFrameTestClient::didStartProvisionalLoad(blink::WebLocalFrame* frame,
                                                 double trigering_event_time) {
  test_runner_->tryToSetTopLoadingFrame(frame);

  if (test_runner_->shouldDumpFrameLoadCallbacks()) {
    PrintFrameDescription(delegate_, frame);
    delegate_->PrintMessage(" - didStartProvisionalLoadForFrame\n");
  }

  if (test_runner_->shouldDumpUserGestureInFrameLoadCallbacks()) {
    PrintFrameuserGestureStatus(delegate_, frame,
                                " - in didStartProvisionalLoadForFrame\n");
  }
}

void WebFrameTestClient::didReceiveServerRedirectForProvisionalLoad(
    blink::WebLocalFrame* frame) {
  if (test_runner_->shouldDumpFrameLoadCallbacks()) {
    PrintFrameDescription(delegate_, frame);
    delegate_->PrintMessage(
        " - didReceiveServerRedirectForProvisionalLoadForFrame\n");
  }
}

void WebFrameTestClient::didFailProvisionalLoad(
    blink::WebLocalFrame* frame,
    const blink::WebURLError& error,
    blink::WebHistoryCommitType commit_type) {
  if (test_runner_->shouldDumpFrameLoadCallbacks()) {
    PrintFrameDescription(delegate_, frame);
    delegate_->PrintMessage(" - didFailProvisionalLoadWithError\n");
  }
}

void WebFrameTestClient::didCommitProvisionalLoad(
    blink::WebLocalFrame* frame,
    const blink::WebHistoryItem& history_item,
    blink::WebHistoryCommitType history_type) {
  if (test_runner_->shouldDumpFrameLoadCallbacks()) {
    PrintFrameDescription(delegate_, frame);
    delegate_->PrintMessage(" - didCommitLoadForFrame\n");
  }
}

void WebFrameTestClient::didReceiveTitle(blink::WebLocalFrame* frame,
                                         const blink::WebString& title,
                                         blink::WebTextDirection direction) {
  if (test_runner_->shouldDumpFrameLoadCallbacks()) {
    PrintFrameDescription(delegate_, frame);
    delegate_->PrintMessage(std::string(" - didReceiveTitle: ") + title.utf8() +
                            "\n");
  }

  if (test_runner_->shouldDumpTitleChanges())
    delegate_->PrintMessage(std::string("TITLE CHANGED: '") + title.utf8() +
                            "'\n");
}

void WebFrameTestClient::didChangeIcon(blink::WebLocalFrame* frame,
                                       blink::WebIconURL::Type icon_type) {
  if (test_runner_->shouldDumpIconChanges()) {
    PrintFrameDescription(delegate_, frame);
    delegate_->PrintMessage(std::string(" - didChangeIcons\n"));
  }
}

void WebFrameTestClient::didFinishDocumentLoad(blink::WebLocalFrame* frame) {
  if (test_runner_->shouldDumpFrameLoadCallbacks()) {
    PrintFrameDescription(delegate_, frame);
    delegate_->PrintMessage(" - didFinishDocumentLoadForFrame\n");
  }
}

void WebFrameTestClient::didHandleOnloadEvents(blink::WebLocalFrame* frame) {
  if (test_runner_->shouldDumpFrameLoadCallbacks()) {
    PrintFrameDescription(delegate_, frame);
    delegate_->PrintMessage(" - didHandleOnloadEventsForFrame\n");
  }
}

void WebFrameTestClient::didFailLoad(blink::WebLocalFrame* frame,
                                     const blink::WebURLError& error,
                                     blink::WebHistoryCommitType commit_type) {
  if (test_runner_->shouldDumpFrameLoadCallbacks()) {
    PrintFrameDescription(delegate_, frame);
    delegate_->PrintMessage(" - didFailLoadWithError\n");
  }
}

void WebFrameTestClient::didFinishLoad(blink::WebLocalFrame* frame) {
  if (test_runner_->shouldDumpFrameLoadCallbacks()) {
    PrintFrameDescription(delegate_, frame);
    delegate_->PrintMessage(" - didFinishLoadForFrame\n");
  }
}

void WebFrameTestClient::didStopLoading() {
  test_runner_->tryToClearTopLoadingFrame(
      web_frame_test_proxy_base_->web_frame());
}

void WebFrameTestClient::didDetectXSS(const blink::WebURL& insecure_url,
                                      bool did_block_entire_page) {
  if (test_runner_->shouldDumpFrameLoadCallbacks())
    delegate_->PrintMessage("didDetectXSS\n");
}

void WebFrameTestClient::didDispatchPingLoader(const blink::WebURL& url) {
  if (test_runner_->shouldDumpPingLoaderCallbacks())
    delegate_->PrintMessage(std::string("PingLoader dispatched to '") +
                            URLDescription(url).c_str() + "'.\n");
}

void WebFrameTestClient::willSendRequest(
    blink::WebLocalFrame* frame,
    unsigned identifier,
    blink::WebURLRequest& request,
    const blink::WebURLResponse& redirect_response) {
  // Need to use GURL for host() and SchemeIs()
  GURL url = request.url();
  std::string request_url = url.possibly_invalid_spec();

  GURL main_document_url = request.firstPartyForCookies();

  if (redirect_response.isNull() &&
      (test_runner_->shouldDumpResourceLoadCallbacks() ||
       test_runner_->shouldDumpResourcePriorities())) {
    DCHECK(resource_identifier_map_.find(identifier) ==
           resource_identifier_map_.end());
    resource_identifier_map_[identifier] =
        DescriptionSuitableForTestResult(request_url);
  }

  if (test_runner_->shouldDumpResourceLoadCallbacks()) {
    if (resource_identifier_map_.find(identifier) ==
        resource_identifier_map_.end())
      delegate_->PrintMessage("<unknown>");
    else
      delegate_->PrintMessage(resource_identifier_map_[identifier]);
    delegate_->PrintMessage(" - willSendRequest <NSURLRequest URL ");
    delegate_->PrintMessage(
        DescriptionSuitableForTestResult(request_url).c_str());
    delegate_->PrintMessage(", main document URL ");
    delegate_->PrintMessage(URLDescription(main_document_url).c_str());
    delegate_->PrintMessage(", http method ");
    delegate_->PrintMessage(request.httpMethod().utf8().data());
    delegate_->PrintMessage("> redirectResponse ");
    PrintResponseDescription(delegate_, redirect_response);
    delegate_->PrintMessage("\n");
  }

  if (test_runner_->shouldDumpResourcePriorities()) {
    delegate_->PrintMessage(
        DescriptionSuitableForTestResult(request_url).c_str());
    delegate_->PrintMessage(" has priority ");
    delegate_->PrintMessage(PriorityDescription(request.getPriority()));
    delegate_->PrintMessage("\n");
  }

  if (test_runner_->httpHeadersToClear()) {
    const std::set<std::string>* clearHeaders =
        test_runner_->httpHeadersToClear();
    for (std::set<std::string>::const_iterator header = clearHeaders->begin();
         header != clearHeaders->end(); ++header)
      request.clearHTTPHeaderField(blink::WebString::fromUTF8(*header));
  }

  std::string host = url.host();
  if (!host.empty() &&
      (url.SchemeIs(url::kHttpScheme) || url.SchemeIs(url::kHttpsScheme))) {
    if (!IsLocalHost(host) && !IsTestHost(host) &&
        !HostIsUsedBySomeTestsToGenerateError(host) &&
        ((!main_document_url.SchemeIs(url::kHttpScheme) &&
          !main_document_url.SchemeIs(url::kHttpsScheme)) ||
         IsLocalHost(main_document_url.host())) &&
        !delegate_->AllowExternalPages()) {
      delegate_->PrintMessage(std::string("Blocked access to external URL ") +
                              request_url + "\n");
      BlockRequest(request);
      return;
    }
  }

  // Set the new substituted URL.
  request.setURL(
      delegate_->RewriteLayoutTestsURL(request.url().string().utf8()));
}

void WebFrameTestClient::didReceiveResponse(
    unsigned identifier,
    const blink::WebURLResponse& response) {
  if (test_runner_->shouldDumpResourceLoadCallbacks()) {
    if (resource_identifier_map_.find(identifier) ==
        resource_identifier_map_.end())
      delegate_->PrintMessage("<unknown>");
    else
      delegate_->PrintMessage(resource_identifier_map_[identifier]);
    delegate_->PrintMessage(" - didReceiveResponse ");
    PrintResponseDescription(delegate_, response);
    delegate_->PrintMessage("\n");
  }
  if (test_runner_->shouldDumpResourceResponseMIMETypes()) {
    GURL url = response.url();
    blink::WebString mime_type = response.mimeType();
    delegate_->PrintMessage(url.ExtractFileName());
    delegate_->PrintMessage(" has MIME type ");
    // Simulate NSURLResponse's mapping of empty/unknown MIME types to
    // application/octet-stream
    delegate_->PrintMessage(mime_type.isEmpty() ? "application/octet-stream"
                                                : mime_type.utf8().data());
    delegate_->PrintMessage("\n");
  }
}

void WebFrameTestClient::didChangeResourcePriority(
    unsigned identifier,
    const blink::WebURLRequest::Priority& priority,
    int intra_priority_value) {
  if (test_runner_->shouldDumpResourcePriorities()) {
    if (resource_identifier_map_.find(identifier) ==
        resource_identifier_map_.end())
      delegate_->PrintMessage("<unknown>");
    else
      delegate_->PrintMessage(resource_identifier_map_[identifier]);
    delegate_->PrintMessage(base::StringPrintf(
        " changed priority to %s, intra_priority %d\n",
        PriorityDescription(priority).c_str(), intra_priority_value));
  }
}

void WebFrameTestClient::didFinishResourceLoad(blink::WebLocalFrame* frame,
                                               unsigned identifier) {
  if (test_runner_->shouldDumpResourceLoadCallbacks()) {
    if (resource_identifier_map_.find(identifier) ==
        resource_identifier_map_.end())
      delegate_->PrintMessage("<unknown>");
    else
      delegate_->PrintMessage(resource_identifier_map_[identifier]);
    delegate_->PrintMessage(" - didFinishLoading\n");
  }
  resource_identifier_map_.erase(identifier);
}

void WebFrameTestClient::didAddMessageToConsole(
    const blink::WebConsoleMessage& message,
    const blink::WebString& source_name,
    unsigned source_line,
    const blink::WebString& stack_trace) {
  std::string level;
  switch (message.level) {
    case blink::WebConsoleMessage::LevelDebug:
      level = "DEBUG";
      break;
    case blink::WebConsoleMessage::LevelLog:
      level = "MESSAGE";
      break;
    case blink::WebConsoleMessage::LevelInfo:
      level = "INFO";
      break;
    case blink::WebConsoleMessage::LevelWarning:
      level = "WARNING";
      break;
    case blink::WebConsoleMessage::LevelError:
      level = "ERROR";
      break;
    default:
      level = "MESSAGE";
  }
  delegate_->PrintMessage(std::string("CONSOLE ") + level + ": ");
  if (source_line) {
    delegate_->PrintMessage(base::StringPrintf("line %d: ", source_line));
  }
  if (!message.text.isEmpty()) {
    std::string new_message;
    new_message = message.text.utf8();
    size_t file_protocol = new_message.find("file://");
    if (file_protocol != std::string::npos) {
      new_message = new_message.substr(0, file_protocol) +
                    URLSuitableForTestResult(new_message.substr(file_protocol));
    }
    delegate_->PrintMessage(new_message);
  }
  delegate_->PrintMessage(std::string("\n"));
}

blink::WebNavigationPolicy WebFrameTestClient::decidePolicyForNavigation(
    const blink::WebFrameClient::NavigationPolicyInfo& info) {
  if (test_runner_->shouldDumpNavigationPolicy()) {
    delegate_->PrintMessage("Default policy for navigation to '" +
                            URLDescription(info.urlRequest.url()) + "' is '" +
                            WebNavigationPolicyToString(info.defaultPolicy) +
                            "'\n");
  }

  blink::WebNavigationPolicy result;
  if (!test_runner_->policyDelegateEnabled())
    return info.defaultPolicy;

  delegate_->PrintMessage(
      std::string("Policy delegate: attempt to load ") +
      URLDescription(info.urlRequest.url()) + " with navigation type '" +
      WebNavigationTypeToString(info.navigationType) + "'\n");
  if (test_runner_->policyDelegateIsPermissive())
    result = blink::WebNavigationPolicyCurrentTab;
  else
    result = blink::WebNavigationPolicyIgnore;

  if (test_runner_->policyDelegateShouldNotifyDone()) {
    test_runner_->policyDelegateDone();
    result = blink::WebNavigationPolicyIgnore;
  }

  return result;
}

bool WebFrameTestClient::willCheckAndDispatchMessageEvent(
    blink::WebLocalFrame* source_frame,
    blink::WebFrame* target_frame,
    blink::WebSecurityOrigin target,
    blink::WebDOMMessageEvent event) {
  if (test_runner_->shouldInterceptPostMessage()) {
    delegate_->PrintMessage("intercepted postMessage\n");
    return true;
  }

  return false;
}

void WebFrameTestClient::checkIfAudioSinkExistsAndIsAuthorized(
    const blink::WebString& sink_id,
    const blink::WebSecurityOrigin& security_origin,
    blink::WebSetSinkIdCallbacks* web_callbacks) {
  std::unique_ptr<blink::WebSetSinkIdCallbacks> callback(web_callbacks);
  std::string device_id = sink_id.utf8();
  if (device_id == "valid" || device_id.empty())
    callback->onSuccess();
  else if (device_id == "unauthorized")
    callback->onError(blink::WebSetSinkIdError::NotAuthorized);
  else
    callback->onError(blink::WebSetSinkIdError::NotFound);
}

void WebFrameTestClient::didClearWindowObject(blink::WebLocalFrame* frame) {
  web_test_proxy_base_->test_interfaces()->BindTo(frame);
  web_test_proxy_base_->BindTo(frame);
}

}  // namespace test_runner
