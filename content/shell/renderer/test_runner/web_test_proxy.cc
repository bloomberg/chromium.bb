// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/web_test_proxy.h"

#include <cctype>

#include "base/callback_helpers.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "content/shell/renderer/test_runner/MockColorChooser.h"
#include "content/shell/renderer/test_runner/MockWebSpeechRecognizer.h"
#include "content/shell/renderer/test_runner/SpellCheckClient.h"
#include "content/shell/renderer/test_runner/TestCommon.h"
#include "content/shell/renderer/test_runner/TestInterfaces.h"
#include "content/shell/renderer/test_runner/TestPlugin.h"
#include "content/shell/renderer/test_runner/WebTestDelegate.h"
#include "content/shell/renderer/test_runner/WebTestInterfaces.h"
#include "content/shell/renderer/test_runner/WebTestRunner.h"
#include "content/shell/renderer/test_runner/WebUserMediaClientMock.h"
#include "content/shell/renderer/test_runner/accessibility_controller.h"
#include "content/shell/renderer/test_runner/event_sender.h"
#include "content/shell/renderer/test_runner/test_runner.h"
// FIXME: Including platform_canvas.h here is a layering violation.
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebAXEnums.h"
#include "third_party/WebKit/public/web/WebAXObject.h"
#include "third_party/WebKit/public/web/WebCachedURLRequest.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebHistoryItem.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebMIDIClientMock.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "third_party/WebKit/public/web/WebPrintParams.h"
#include "third_party/WebKit/public/web/WebRange.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "third_party/WebKit/public/web/WebView.h"

using namespace blink;
using namespace std;

namespace content {

namespace {

class HostMethodTask : public WebMethodTask<WebTestProxyBase> {
 public:
  typedef void (WebTestProxyBase::*CallbackMethodType)();
  HostMethodTask(WebTestProxyBase* object, CallbackMethodType callback)
      : WebMethodTask<WebTestProxyBase>(object), m_callback(callback) {}

  virtual void runIfValid() OVERRIDE { (m_object->*m_callback)(); }

 private:
  CallbackMethodType m_callback;
};

void printFrameDescription(WebTestDelegate* delegate, WebFrame* frame) {
  string name8 = frame->uniqueName().utf8();
  if (frame == frame->view()->mainFrame()) {
    if (!name8.length()) {
      delegate->printMessage("main frame");
      return;
    }
    delegate->printMessage(string("main frame \"") + name8 + "\"");
    return;
  }
  if (!name8.length()) {
    delegate->printMessage("frame (anonymous)");
    return;
  }
  delegate->printMessage(string("frame \"") + name8 + "\"");
}

void printFrameUserGestureStatus(WebTestDelegate* delegate, WebFrame* frame,
                                 const char* msg) {
  bool isUserGesture = WebUserGestureIndicator::isProcessingUserGesture();
  delegate->printMessage(string("Frame with user gesture \"") +
                         (isUserGesture ? "true" : "false") + "\"" + msg);
}

// Used to write a platform neutral file:/// URL by taking the
// filename and its directory. (e.g., converts
// "file:///tmp/foo/bar.txt" to just "bar.txt").
string descriptionSuitableForTestResult(const string& url) {
  if (url.empty() || string::npos == url.find("file://")) return url;

  size_t pos = url.rfind('/');
  if (pos == string::npos || !pos) return "ERROR:" + url;
  pos = url.rfind('/', pos - 1);
  if (pos == string::npos) return "ERROR:" + url;

  return url.substr(pos + 1);
}

void printResponseDescription(WebTestDelegate* delegate,
                              const WebURLResponse& response) {
  if (response.isNull()) {
    delegate->printMessage("(null)");
    return;
  }
  string url = response.url().spec();
  char data[100];
  snprintf(data, sizeof(data), "%d", response.httpStatusCode());
  delegate->printMessage(string("<NSURLResponse ") +
                         descriptionSuitableForTestResult(url) +
                         ", http status code " + data + ">");
}

string URLDescription(const GURL& url) {
  if (url.SchemeIs("file")) return url.ExtractFileName();
  return url.possibly_invalid_spec();
}

string PriorityDescription(const WebURLRequest::Priority& priority) {
  switch (priority) {
    case WebURLRequest::PriorityVeryLow:
      return "VeryLow";
    case WebURLRequest::PriorityLow:
      return "Low";
    case WebURLRequest::PriorityMedium:
      return "Medium";
    case WebURLRequest::PriorityHigh:
      return "High";
    case WebURLRequest::PriorityVeryHigh:
      return "VeryHigh";
    case WebURLRequest::PriorityUnresolved:
    default:
      return "Unresolved";
  }
}

void blockRequest(WebURLRequest& request) {
  request.setURL(GURL("255.255.255.255"));
}

bool isLocalhost(const string& host) {
  return host == "127.0.0.1" || host == "localhost";
}

bool hostIsUsedBySomeTestsToGenerateError(const string& host) {
  return host == "255.255.255.255";
}

// Used to write a platform neutral file:/// URL by only taking the filename
// (e.g., converts "file:///tmp/foo.txt" to just "foo.txt").
string urlSuitableForTestResult(const string& url) {
  if (url.empty() || string::npos == url.find("file://")) return url;

  size_t pos = url.rfind('/');
  if (pos == string::npos) {
#ifdef WIN32
    pos = url.rfind('\\');
    if (pos == string::npos) pos = 0;
#else
    pos = 0;
#endif
  }
  string filename = url.substr(pos + 1);
  if (filename.empty())
    return "file:";  // A WebKit test has this in its expected output.
  return filename;
}

// WebNavigationType debugging strings taken from PolicyDelegate.mm.
const char* linkClickedString = "link clicked";
const char* formSubmittedString = "form submitted";
const char* backForwardString = "back/forward";
const char* reloadString = "reload";
const char* formResubmittedString = "form resubmitted";
const char* otherString = "other";
const char* illegalString = "illegal value";

// Get a debugging string from a WebNavigationType.
const char* webNavigationTypeToString(WebNavigationType type) {
  switch (type) {
    case blink::WebNavigationTypeLinkClicked:
      return linkClickedString;
    case blink::WebNavigationTypeFormSubmitted:
      return formSubmittedString;
    case blink::WebNavigationTypeBackForward:
      return backForwardString;
    case blink::WebNavigationTypeReload:
      return reloadString;
    case blink::WebNavigationTypeFormResubmitted:
      return formResubmittedString;
    case blink::WebNavigationTypeOther:
      return otherString;
  }
  return illegalString;
}

string dumpDocumentText(WebFrame* frame) {
  // We use the document element's text instead of the body text here because
  // not all documents have a body, such as XML documents.
  WebElement documentElement = frame->document().documentElement();
  if (documentElement.isNull()) return string();
  return documentElement.innerText().utf8();
}

string dumpFramesAsText(WebFrame* frame, bool recursive) {
  string result;

  // Add header for all but the main frame. Skip empty frames.
  if (frame->parent() && !frame->document().documentElement().isNull()) {
    result.append("\n--------\nFrame: '");
    result.append(frame->uniqueName().utf8().data());
    result.append("'\n--------\n");
  }

  result.append(dumpDocumentText(frame));
  result.append("\n");

  if (recursive) {
    for (WebFrame* child = frame->firstChild(); child;
         child = child->nextSibling())
      result.append(dumpFramesAsText(child, recursive));
  }

  return result;
}

string dumpFramesAsPrintedText(WebFrame* frame, bool recursive) {
  string result;

  // Cannot do printed format for anything other than HTML
  if (!frame->document().isHTMLDocument()) return string();

  // Add header for all but the main frame. Skip empty frames.
  if (frame->parent() && !frame->document().documentElement().isNull()) {
    result.append("\n--------\nFrame: '");
    result.append(frame->uniqueName().utf8().data());
    result.append("'\n--------\n");
  }

  result.append(frame->renderTreeAsText(WebFrame::RenderAsTextPrinting).utf8());
  result.append("\n");

  if (recursive) {
    for (WebFrame* child = frame->firstChild(); child;
         child = child->nextSibling())
      result.append(dumpFramesAsPrintedText(child, recursive));
  }

  return result;
}

string dumpFrameScrollPosition(WebFrame* frame, bool recursive) {
  string result;
  WebSize offset = frame->scrollOffset();
  if (offset.width > 0 || offset.height > 0) {
    if (frame->parent())
      result = string("frame '") + frame->uniqueName().utf8().data() + "' ";
    char data[100];
    snprintf(data, sizeof(data), "scrolled to %d,%d\n", offset.width,
             offset.height);
    result += data;
  }

  if (!recursive) return result;
  for (WebFrame* child = frame->firstChild(); child;
       child = child->nextSibling())
    result += dumpFrameScrollPosition(child, recursive);
  return result;
}

string dumpAllBackForwardLists(TestInterfaces* interfaces,
                               WebTestDelegate* delegate) {
  string result;
  const vector<WebTestProxyBase*>& windowList = interfaces->windowList();
  for (unsigned i = 0; i < windowList.size(); ++i)
    result.append(delegate->dumpHistoryForWindow(windowList.at(i)));
  return result;
}
}

WebTestProxyBase::WebTestProxyBase()
    : test_interfaces_(NULL),
      delegate_(NULL),
      web_widget_(NULL),
      spellcheck_(new SpellCheckClient(this)),
      chooser_count_(0) {
  Reset();
}

WebTestProxyBase::~WebTestProxyBase() { test_interfaces_->windowClosed(this); }

void WebTestProxyBase::SetInterfaces(WebTestInterfaces* interfaces) {
  test_interfaces_ = interfaces->testInterfaces();
  test_interfaces_->windowOpened(this);
}

void WebTestProxyBase::SetDelegate(WebTestDelegate* delegate) {
  delegate_ = delegate;
  spellcheck_->setDelegate(delegate);
  if (m_speechRecognizer.get()) m_speechRecognizer->setDelegate(delegate);
}

WebView* WebTestProxyBase::GetWebView() const {
  DCHECK(web_widget_);
  // TestRunner does not support popup widgets. So |web_widget|_ is always a
  // WebView.
  return static_cast<WebView*>(web_widget_);
}

void WebTestProxyBase::DidForceResize() {
  InvalidateAll();
  DiscardBackingStore();
}

void WebTestProxyBase::Reset() {
  paint_rect_ = WebRect();
  canvas_.reset();
  is_painting_ = false;
  animate_scheduled_ = false;
  resource_identifier_map_.clear();
  log_console_output_ = true;
  if (m_midiClient.get()) m_midiClient->resetMock();
}

WebSpellCheckClient* WebTestProxyBase::GetSpellCheckClient() const {
  return spellcheck_.get();
}

WebColorChooser* WebTestProxyBase::CreateColorChooser(
    WebColorChooserClient* client, const blink::WebColor& color,
    const blink::WebVector<blink::WebColorSuggestion>& suggestions) {
  // This instance is deleted by WebCore::ColorInputType
  return new MockColorChooser(client, delegate_, this);
}

bool WebTestProxyBase::RunFileChooser(const blink::WebFileChooserParams&,
                                      blink::WebFileChooserCompletion*) {
  delegate_->printMessage("Mock: Opening a file chooser.\n");
  // FIXME: Add ability to set file names to a file upload control.
  return false;
}

void WebTestProxyBase::ShowValidationMessage(const WebRect&,
                                             const WebString& message,
                                             const WebString& subMessage,
                                             WebTextDirection) {
  delegate_->printMessage(
      std::string("ValidationMessageClient: main-message=") +
      std::string(message.utf8()) + " sub-message=" +
      std::string(subMessage.utf8()) + "\n");
}

void WebTestProxyBase::HideValidationMessage() {}

void WebTestProxyBase::MoveValidationMessage(const WebRect&) {}

string WebTestProxyBase::CaptureTree(bool debugRenderTree) {
  bool shouldDumpCustomText =
      test_interfaces_->testRunner()->shouldDumpAsCustomText();
  bool shouldDumpAsText = test_interfaces_->testRunner()->shouldDumpAsText();
  bool shouldDumpAsMarkup =
      test_interfaces_->testRunner()->shouldDumpAsMarkup();
  bool shouldDumpAsPrinted = test_interfaces_->testRunner()->isPrinting();
  WebFrame* frame = GetWebView()->mainFrame();
  string dataUtf8;
  if (shouldDumpCustomText) {
    // Append a newline for the test driver.
    dataUtf8 = test_interfaces_->testRunner()->customDumpText() + "\n";
  } else if (shouldDumpAsText) {
    bool recursive =
        test_interfaces_->testRunner()->shouldDumpChildFramesAsText();
    dataUtf8 = shouldDumpAsPrinted ? dumpFramesAsPrintedText(frame, recursive)
                                   : dumpFramesAsText(frame, recursive);
  } else if (shouldDumpAsMarkup) {
    // Append a newline for the test driver.
    dataUtf8 = frame->contentAsMarkup().utf8() + "\n";
  } else {
    bool recursive =
        test_interfaces_->testRunner()->shouldDumpChildFrameScrollPositions();
    WebFrame::RenderAsTextControls renderTextBehavior =
        WebFrame::RenderAsTextNormal;
    if (shouldDumpAsPrinted)
      renderTextBehavior |= WebFrame::RenderAsTextPrinting;
    if (debugRenderTree) renderTextBehavior |= WebFrame::RenderAsTextDebug;
    dataUtf8 = frame->renderTreeAsText(renderTextBehavior).utf8();
    dataUtf8 += dumpFrameScrollPosition(frame, recursive);
  }

  if (test_interfaces_->testRunner()->shouldDumpBackForwardList())
    dataUtf8 += dumpAllBackForwardLists(test_interfaces_, delegate_);

  return dataUtf8;
}

void WebTestProxyBase::DrawSelectionRect(SkCanvas* canvas) {
  // See if we need to draw the selection bounds rect. Selection bounds
  // rect is the rect enclosing the (possibly transformed) selection.
  // The rect should be drawn after everything is laid out and painted.
  if (!test_interfaces_->testRunner()->shouldDumpSelectionRect()) return;
  // If there is a selection rect - draw a red 1px border enclosing rect
  WebRect wr = GetWebView()->mainFrame()->selectionBoundsRect();
  if (wr.isEmpty()) return;
  // Render a red rectangle bounding selection rect
  SkPaint paint;
  paint.setColor(0xFFFF0000);  // Fully opaque red
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setFlags(SkPaint::kAntiAlias_Flag);
  paint.setStrokeWidth(1.0f);
  SkIRect rect;  // Bounding rect
  rect.set(wr.x, wr.y, wr.x + wr.width, wr.y + wr.height);
  canvas->drawIRect(rect, paint);
}

void WebTestProxyBase::didCompositeAndReadback(const SkBitmap& bitmap) {
  TRACE_EVENT2("shell", "WebTestProxyBase::didCompositeAndReadback", "x",
               bitmap.info().fWidth, "y", bitmap.info().fHeight);
  SkCanvas canvas(bitmap);
  DrawSelectionRect(&canvas);
  DCHECK(!composite_and_readback_callbacks_.empty());
  composite_and_readback_callbacks_.front().Run(bitmap);
  composite_and_readback_callbacks_.pop_front();
}

void WebTestProxyBase::CapturePixelsForPrinting(
    const base::Callback<void(const SkBitmap&)>& callback) {
  // TODO(enne): get rid of stateful canvas().
  web_widget_->layout();
  PaintPagesWithBoundaries();
  DrawSelectionRect(GetCanvas());
  SkBaseDevice* device = skia::GetTopDevice(*GetCanvas());
  const SkBitmap& bitmap = device->accessBitmap(false);
  callback.Run(bitmap);
}

void WebTestProxyBase::CapturePixelsAsync(
    const base::Callback<void(const SkBitmap&)>& callback) {
  TRACE_EVENT0("shell", "WebTestProxyBase::CapturePixelsAsync");

  DCHECK(web_widget_->isAcceleratedCompositingActive());
  DCHECK(!callback.is_null());

  if (test_interfaces_->testRunner()->isPrinting()) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(&WebTestProxyBase::CapturePixelsForPrinting,
                              base::Unretained(this), callback));
    return;
  }

  composite_and_readback_callbacks_.push_back(callback);
  web_widget_->compositeAndReadbackAsync(this);
}

void WebTestProxyBase::SetLogConsoleOutput(bool enabled) {
  log_console_output_ = enabled;
}

void WebTestProxyBase::PaintPagesWithBoundaries() {
  DCHECK(!is_painting_);
  DCHECK(GetCanvas());
  is_painting_ = true;

  WebSize pageSizeInPixels = web_widget_->size();
  WebFrame* webFrame = GetWebView()->mainFrame();

  int pageCount = webFrame->printBegin(pageSizeInPixels);
  int totalHeight = pageCount * (pageSizeInPixels.height + 1) - 1;

  SkCanvas* testCanvas =
      skia::TryCreateBitmapCanvas(pageSizeInPixels.width, totalHeight, false);
  if (testCanvas) {
    DiscardBackingStore();
    canvas_.reset(testCanvas);
  } else {
    webFrame->printEnd();
    return;
  }

  webFrame->printPagesWithBoundaries(GetCanvas(), pageSizeInPixels);
  webFrame->printEnd();

  is_painting_ = false;
}

SkCanvas* WebTestProxyBase::GetCanvas() {
  if (canvas_.get()) return canvas_.get();
  WebSize widgetSize = web_widget_->size();
  float deviceScaleFactor = GetWebView()->deviceScaleFactor();
  int scaledWidth = static_cast<int>(
      ceil(static_cast<float>(widgetSize.width) * deviceScaleFactor));
  int scaledHeight = static_cast<int>(
      ceil(static_cast<float>(widgetSize.height) * deviceScaleFactor));
  // We're allocating the canvas to be non-opaque (third parameter), so we
  // don't end up with uninitialized memory if a layout test doesn't damage
  // the entire view.
  canvas_.reset(skia::CreateBitmapCanvas(scaledWidth, scaledHeight, false));
  return canvas_.get();
}

void WebTestProxyBase::DidDisplayAsync(const base::Closure& callback,
                                       const SkBitmap& bitmap) {
  // Verify we actually composited.
  CHECK_NE(0, bitmap.info().fWidth);
  CHECK_NE(0, bitmap.info().fHeight);
  if (!callback.is_null()) callback.Run();
}

void WebTestProxyBase::DisplayAsyncThen(const base::Closure& callback) {
  TRACE_EVENT0("shell", "WebTestProxyBase::DisplayAsyncThen");

  CHECK(web_widget_->isAcceleratedCompositingActive());
  CapturePixelsAsync(base::Bind(&WebTestProxyBase::DidDisplayAsync,
                                base::Unretained(this), callback));
}

void WebTestProxyBase::DiscardBackingStore() { canvas_.reset(); }

WebMIDIClientMock* WebTestProxyBase::GetMIDIClientMock() {
  if (!m_midiClient.get()) m_midiClient.reset(new WebMIDIClientMock);
  return m_midiClient.get();
}

MockWebSpeechRecognizer* WebTestProxyBase::GetSpeechRecognizerMock() {
  if (!m_speechRecognizer.get()) {
    m_speechRecognizer.reset(new MockWebSpeechRecognizer());
    m_speechRecognizer->setDelegate(delegate_);
  }
  return m_speechRecognizer.get();
}

void WebTestProxyBase::DidInvalidateRect(const WebRect& rect) {
  // paint_rect_ = paint_rect_ U rect
  if (rect.isEmpty()) return;
  if (paint_rect_.isEmpty()) {
    paint_rect_ = rect;
    return;
  }
  int left = min(paint_rect_.x, rect.x);
  int top = min(paint_rect_.y, rect.y);
  int right = max(paint_rect_.x + paint_rect_.width, rect.x + rect.width);
  int bottom = max(paint_rect_.y + paint_rect_.height, rect.y + rect.height);
  paint_rect_ = WebRect(left, top, right - left, bottom - top);
}

void WebTestProxyBase::DidScrollRect(int, int, const WebRect& clipRect) {
  DidInvalidateRect(clipRect);
}

void WebTestProxyBase::InvalidateAll() {
  paint_rect_ = WebRect(0, 0, INT_MAX, INT_MAX);
}

void WebTestProxyBase::ScheduleComposite() { InvalidateAll(); }

void WebTestProxyBase::ScheduleAnimation() {
  if (!test_interfaces_->testRunner()->TestIsRunning()) return;

  if (!animate_scheduled_) {
    animate_scheduled_ = true;
    delegate_->postDelayedTask(
        new HostMethodTask(this, &WebTestProxyBase::AnimateNow), 1);
  }
}

void WebTestProxyBase::AnimateNow() {
  if (animate_scheduled_) {
    animate_scheduled_ = false;
    web_widget_->animate(0.0);
    web_widget_->layout();
  }
}

bool WebTestProxyBase::IsCompositorFramePending() const {
  return animate_scheduled_ || !paint_rect_.isEmpty();
}

void WebTestProxyBase::Show(WebNavigationPolicy) { InvalidateAll(); }

void WebTestProxyBase::SetWindowRect(const WebRect& rect) {
  InvalidateAll();
  DiscardBackingStore();
}

void WebTestProxyBase::DidAutoResize(const WebSize&) { InvalidateAll(); }

void WebTestProxyBase::PostAccessibilityEvent(const blink::WebAXObject& obj,
                                              blink::WebAXEvent event) {
  // Only hook the accessibility events occured during the test run.
  // This check prevents false positives in WebLeakDetector.
  // The pending tasks in browser/renderer message queue may trigger
  // accessibility events,
  // and AccessibilityController will hold on to their target nodes if we don't
  // ignore them here.
  if (!test_interfaces_->testRunner()->TestIsRunning()) return;

  if (event == blink::WebAXEventFocus)
    test_interfaces_->accessibilityController()->SetFocusedElement(obj);

  const char* eventName = NULL;
  switch (event) {
    case blink::WebAXEventActiveDescendantChanged:
      eventName = "ActiveDescendantChanged";
      break;
    case blink::WebAXEventAlert:
      eventName = "Alert";
      break;
    case blink::WebAXEventAriaAttributeChanged:
      eventName = "AriaAttributeChanged";
      break;
    case blink::WebAXEventAutocorrectionOccured:
      eventName = "AutocorrectionOccured";
      break;
    case blink::WebAXEventBlur:
      eventName = "Blur";
      break;
    case blink::WebAXEventCheckedStateChanged:
      eventName = "CheckedStateChanged";
      break;
    case blink::WebAXEventChildrenChanged:
      eventName = "ChildrenChanged";
      break;
    case blink::WebAXEventFocus:
      eventName = "Focus";
      break;
    case blink::WebAXEventHide:
      eventName = "Hide";
      break;
    case blink::WebAXEventInvalidStatusChanged:
      eventName = "InvalidStatusChanged";
      break;
    case blink::WebAXEventLayoutComplete:
      eventName = "LayoutComplete";
      break;
    case blink::WebAXEventLiveRegionChanged:
      eventName = "LiveRegionChanged";
      break;
    case blink::WebAXEventLoadComplete:
      eventName = "LoadComplete";
      break;
    case blink::WebAXEventLocationChanged:
      eventName = "LocationChanged";
      break;
    case blink::WebAXEventMenuListItemSelected:
      eventName = "MenuListItemSelected";
      break;
    case blink::WebAXEventMenuListValueChanged:
      eventName = "MenuListValueChanged";
      break;
    case blink::WebAXEventRowCollapsed:
      eventName = "RowCollapsed";
      break;
    case blink::WebAXEventRowCountChanged:
      eventName = "RowCountChanged";
      break;
    case blink::WebAXEventRowExpanded:
      eventName = "RowExpanded";
      break;
    case blink::WebAXEventScrollPositionChanged:
      eventName = "ScrollPositionChanged";
      break;
    case blink::WebAXEventScrolledToAnchor:
      eventName = "ScrolledToAnchor";
      break;
    case blink::WebAXEventSelectedChildrenChanged:
      eventName = "SelectedChildrenChanged";
      break;
    case blink::WebAXEventSelectedTextChanged:
      eventName = "SelectedTextChanged";
      break;
    case blink::WebAXEventShow:
      eventName = "Show";
      break;
    case blink::WebAXEventTextChanged:
      eventName = "TextChanged";
      break;
    case blink::WebAXEventTextInserted:
      eventName = "TextInserted";
      break;
    case blink::WebAXEventTextRemoved:
      eventName = "TextRemoved";
      break;
    case blink::WebAXEventValueChanged:
      eventName = "ValueChanged";
      break;
    default:
      eventName = "Unknown";
      break;
  }

  test_interfaces_->accessibilityController()->NotificationReceived(obj,
                                                                    eventName);

  if (test_interfaces_->accessibilityController()
          ->ShouldLogAccessibilityEvents()) {
    string message("AccessibilityNotification - ");
    message += eventName;

    blink::WebNode node = obj.node();
    if (!node.isNull() && node.isElementNode()) {
      blink::WebElement element = node.to<blink::WebElement>();
      if (element.hasAttribute("id")) {
        message += " - id:";
        message += element.getAttribute("id").utf8().data();
      }
    }

    delegate_->printMessage(message + "\n");
  }
}

void WebTestProxyBase::StartDragging(WebLocalFrame*, const WebDragData& data,
                                     WebDragOperationsMask mask,
                                     const WebImage&, const WebPoint&) {
  // When running a test, we need to fake a drag drop operation otherwise
  // Windows waits for real mouse events to know when the drag is over.
  test_interfaces_->eventSender()->DoDragDrop(data, mask);
}

// The output from these methods in layout test mode should match that
// expected by the layout tests. See EditingDelegate.m in DumpRenderTree.

void WebTestProxyBase::DidChangeSelection(bool isEmptySelection) {
  if (test_interfaces_->testRunner()->shouldDumpEditingCallbacks())
    delegate_->printMessage(
        "EDITING DELEGATE: "
        "webViewDidChangeSelection:WebViewDidChangeSelectionNotification\n");
}

void WebTestProxyBase::DidChangeContents() {
  if (test_interfaces_->testRunner()->shouldDumpEditingCallbacks())
    delegate_->printMessage(
        "EDITING DELEGATE: webViewDidChange:WebViewDidChangeNotification\n");
}

bool WebTestProxyBase::CreateView(WebLocalFrame*, const WebURLRequest& request,
                                  const WebWindowFeatures&, const WebString&,
                                  WebNavigationPolicy, bool) {
  if (!test_interfaces_->testRunner()->canOpenWindows()) return false;
  if (test_interfaces_->testRunner()->shouldDumpCreateView())
    delegate_->printMessage(string("createView(") +
                            URLDescription(request.url()) + ")\n");
  return true;
}

WebPlugin* WebTestProxyBase::CreatePlugin(WebLocalFrame* frame,
                                          const WebPluginParams& params) {
  if (TestPlugin::isSupportedMimeType(params.mimeType))
    return TestPlugin::create(frame, params, delegate_);
  return 0;
}

void WebTestProxyBase::SetStatusText(const WebString& text) {
  if (!test_interfaces_->testRunner()->shouldDumpStatusCallbacks()) return;
  delegate_->printMessage(
      string("UI DELEGATE STATUS CALLBACK: setStatusText:") +
      text.utf8().data() + "\n");
}

void WebTestProxyBase::DidStopLoading() {
  if (test_interfaces_->testRunner()->shouldDumpProgressFinishedCallback())
    delegate_->printMessage("postProgressFinishedNotification\n");
}

void WebTestProxyBase::ShowContextMenu(
    WebLocalFrame*, const WebContextMenuData& contextMenuData) {
  test_interfaces_->eventSender()->SetContextMenuData(contextMenuData);
}

WebUserMediaClient* WebTestProxyBase::GetUserMediaClient() {
  if (!user_media_client_.get())
    user_media_client_.reset(new WebUserMediaClientMock(delegate_));
  return user_media_client_.get();
}

// Simulate a print by going into print mode and then exit straight away.
void WebTestProxyBase::PrintPage(WebLocalFrame* frame) {
  WebSize pageSizeInPixels = web_widget_->size();
  if (pageSizeInPixels.isEmpty()) return;
  WebPrintParams printParams(pageSizeInPixels);
  frame->printBegin(printParams);
  frame->printEnd();
}

WebNotificationPresenter* WebTestProxyBase::GetNotificationPresenter() {
  return test_interfaces_->testRunner()->notification_presenter();
}

WebMIDIClient* WebTestProxyBase::GetWebMIDIClient() {
  return GetMIDIClientMock();
}

WebSpeechRecognizer* WebTestProxyBase::GetSpeechRecognizer() {
  return GetSpeechRecognizerMock();
}

bool WebTestProxyBase::RequestPointerLock() {
  return test_interfaces_->testRunner()->RequestPointerLock();
}

void WebTestProxyBase::RequestPointerUnlock() {
  test_interfaces_->testRunner()->RequestPointerUnlock();
}

bool WebTestProxyBase::IsPointerLocked() {
  return test_interfaces_->testRunner()->isPointerLocked();
}

void WebTestProxyBase::DidFocus() { delegate_->setFocus(this, true); }

void WebTestProxyBase::DidBlur() { delegate_->setFocus(this, false); }

void WebTestProxyBase::SetToolTipText(const WebString& text, WebTextDirection) {
  test_interfaces_->testRunner()->setToolTipText(text);
}

void WebTestProxyBase::DidOpenChooser() { chooser_count_++; }

void WebTestProxyBase::DidCloseChooser() { chooser_count_--; }

bool WebTestProxyBase::IsChooserShown() { return 0 < chooser_count_; }

void WebTestProxyBase::LoadURLExternally(WebLocalFrame* frame,
                                         const WebURLRequest& request,
                                         WebNavigationPolicy policy,
                                         const WebString& suggested_name) {
  if (test_interfaces_->testRunner()->shouldWaitUntilExternalURLLoad()) {
    if (policy == WebNavigationPolicyDownload) {
      delegate_->printMessage(
          string("Downloading URL with suggested filename \"") +
          suggested_name.utf8() + "\"\n");
    } else {
      delegate_->printMessage(string("Loading URL externally - \"") +
                              URLDescription(request.url()) + "\"\n");
    }
    delegate_->testFinished();
  }
}

void WebTestProxyBase::DidStartProvisionalLoad(WebLocalFrame* frame) {
  if (!test_interfaces_->testRunner()->topLoadingFrame())
    test_interfaces_->testRunner()->setTopLoadingFrame(frame, false);

  if (test_interfaces_->testRunner()->shouldDumpFrameLoadCallbacks()) {
    printFrameDescription(delegate_, frame);
    delegate_->printMessage(" - didStartProvisionalLoadForFrame\n");
  }

  if (test_interfaces_->testRunner()
          ->shouldDumpUserGestureInFrameLoadCallbacks())
    printFrameUserGestureStatus(delegate_, frame,
                                " - in didStartProvisionalLoadForFrame\n");
}

void WebTestProxyBase::DidReceiveServerRedirectForProvisionalLoad(
    WebLocalFrame* frame) {
  if (test_interfaces_->testRunner()->shouldDumpFrameLoadCallbacks()) {
    printFrameDescription(delegate_, frame);
    delegate_->printMessage(
        " - didReceiveServerRedirectForProvisionalLoadForFrame\n");
  }
}

bool WebTestProxyBase::DidFailProvisionalLoad(WebLocalFrame* frame,
                                              const WebURLError&) {
  if (test_interfaces_->testRunner()->shouldDumpFrameLoadCallbacks()) {
    printFrameDescription(delegate_, frame);
    delegate_->printMessage(" - didFailProvisionalLoadWithError\n");
  }
  LocationChangeDone(frame);
  return !frame->provisionalDataSource();
}

void WebTestProxyBase::DidCommitProvisionalLoad(WebLocalFrame* frame,
                                                const WebHistoryItem&,
                                                blink::WebHistoryCommitType) {
  if (test_interfaces_->testRunner()->shouldDumpFrameLoadCallbacks()) {
    printFrameDescription(delegate_, frame);
    delegate_->printMessage(" - didCommitLoadForFrame\n");
  }
}

void WebTestProxyBase::DidReceiveTitle(WebLocalFrame* frame,
                                       const WebString& title,
                                       WebTextDirection direction) {
  WebCString title8 = title.utf8();

  if (test_interfaces_->testRunner()->shouldDumpFrameLoadCallbacks()) {
    printFrameDescription(delegate_, frame);
    delegate_->printMessage(string(" - didReceiveTitle: ") + title8.data() +
                            "\n");
  }

  if (test_interfaces_->testRunner()->shouldDumpTitleChanges())
    delegate_->printMessage(string("TITLE CHANGED: '") + title8.data() + "'\n");
}

void WebTestProxyBase::DidChangeIcon(WebLocalFrame* frame, WebIconURL::Type) {
  if (test_interfaces_->testRunner()->shouldDumpIconChanges()) {
    printFrameDescription(delegate_, frame);
    delegate_->printMessage(string(" - didChangeIcons\n"));
  }
}

void WebTestProxyBase::DidFinishDocumentLoad(WebLocalFrame* frame) {
  if (test_interfaces_->testRunner()->shouldDumpFrameLoadCallbacks()) {
    printFrameDescription(delegate_, frame);
    delegate_->printMessage(" - didFinishDocumentLoadForFrame\n");
  } else {
    unsigned pendingUnloadEvents = frame->unloadListenerCount();
    if (pendingUnloadEvents) {
      printFrameDescription(delegate_, frame);
      char buffer[100];
      snprintf(buffer, sizeof(buffer), " - has %u onunload handler(s)\n",
               pendingUnloadEvents);
      delegate_->printMessage(buffer);
    }
  }
}

void WebTestProxyBase::DidHandleOnloadEvents(WebLocalFrame* frame) {
  if (test_interfaces_->testRunner()->shouldDumpFrameLoadCallbacks()) {
    printFrameDescription(delegate_, frame);
    delegate_->printMessage(" - didHandleOnloadEventsForFrame\n");
  }
}

void WebTestProxyBase::DidFailLoad(WebLocalFrame* frame, const WebURLError&) {
  if (test_interfaces_->testRunner()->shouldDumpFrameLoadCallbacks()) {
    printFrameDescription(delegate_, frame);
    delegate_->printMessage(" - didFailLoadWithError\n");
  }
  LocationChangeDone(frame);
}

void WebTestProxyBase::DidFinishLoad(WebLocalFrame* frame) {
  if (test_interfaces_->testRunner()->shouldDumpFrameLoadCallbacks()) {
    printFrameDescription(delegate_, frame);
    delegate_->printMessage(" - didFinishLoadForFrame\n");
  }
  LocationChangeDone(frame);
}

void WebTestProxyBase::DidDetectXSS(WebLocalFrame*, const WebURL&, bool) {
  if (test_interfaces_->testRunner()->shouldDumpFrameLoadCallbacks())
    delegate_->printMessage("didDetectXSS\n");
}

void WebTestProxyBase::DidDispatchPingLoader(WebLocalFrame*,
                                             const WebURL& url) {
  if (test_interfaces_->testRunner()->shouldDumpPingLoaderCallbacks())
    delegate_->printMessage(string("PingLoader dispatched to '") +
                            URLDescription(url).c_str() + "'.\n");
}

void WebTestProxyBase::WillRequestResource(
    WebLocalFrame* frame, const blink::WebCachedURLRequest& request) {
  if (test_interfaces_->testRunner()->shouldDumpResourceRequestCallbacks()) {
    printFrameDescription(delegate_, frame);
    delegate_->printMessage(string(" - ") +
                            request.initiatorName().utf8().data());
    delegate_->printMessage(string(" requested '") +
                            URLDescription(request.urlRequest().url()).c_str() +
                            "'\n");
  }
}

void WebTestProxyBase::WillSendRequest(
    WebLocalFrame*, unsigned identifier, blink::WebURLRequest& request,
    const blink::WebURLResponse& redirectResponse) {
  // Need to use GURL for host() and SchemeIs()
  GURL url = request.url();
  string requestURL = url.possibly_invalid_spec();

  GURL mainDocumentURL = request.firstPartyForCookies();

  if (redirectResponse.isNull() &&
      (test_interfaces_->testRunner()->shouldDumpResourceLoadCallbacks() ||
       test_interfaces_->testRunner()->shouldDumpResourcePriorities())) {
    DCHECK(resource_identifier_map_.find(identifier) ==
           resource_identifier_map_.end());
    resource_identifier_map_[identifier] =
        descriptionSuitableForTestResult(requestURL);
  }

  if (test_interfaces_->testRunner()->shouldDumpResourceLoadCallbacks()) {
    if (resource_identifier_map_.find(identifier) ==
        resource_identifier_map_.end())
      delegate_->printMessage("<unknown>");
    else
      delegate_->printMessage(resource_identifier_map_[identifier]);
    delegate_->printMessage(" - willSendRequest <NSURLRequest URL ");
    delegate_->printMessage(
        descriptionSuitableForTestResult(requestURL).c_str());
    delegate_->printMessage(", main document URL ");
    delegate_->printMessage(URLDescription(mainDocumentURL).c_str());
    delegate_->printMessage(", http method ");
    delegate_->printMessage(request.httpMethod().utf8().data());
    delegate_->printMessage("> redirectResponse ");
    printResponseDescription(delegate_, redirectResponse);
    delegate_->printMessage("\n");
  }

  if (test_interfaces_->testRunner()->shouldDumpResourcePriorities()) {
    delegate_->printMessage(
        descriptionSuitableForTestResult(requestURL).c_str());
    delegate_->printMessage(" has priority ");
    delegate_->printMessage(PriorityDescription(request.priority()));
    delegate_->printMessage("\n");
  }

  if (test_interfaces_->testRunner()->httpHeadersToClear()) {
    const set<string>* clearHeaders =
        test_interfaces_->testRunner()->httpHeadersToClear();
    for (set<string>::const_iterator header = clearHeaders->begin();
         header != clearHeaders->end(); ++header)
      request.clearHTTPHeaderField(WebString::fromUTF8(*header));
  }

  string host = url.host();
  if (!host.empty() && (url.SchemeIs("http") || url.SchemeIs("https"))) {
    if (!isLocalhost(host) && !hostIsUsedBySomeTestsToGenerateError(host) &&
        ((!mainDocumentURL.SchemeIs("http") &&
          !mainDocumentURL.SchemeIs("https")) ||
         isLocalhost(mainDocumentURL.host())) &&
        !delegate_->allowExternalPages()) {
      delegate_->printMessage(string("Blocked access to external URL ") +
                              requestURL + "\n");
      blockRequest(request);
      return;
    }
  }

  // Set the new substituted URL.
  request.setURL(delegate_->rewriteLayoutTestsURL(request.url().spec()));
}

void WebTestProxyBase::DidReceiveResponse(
    WebLocalFrame*, unsigned identifier,
    const blink::WebURLResponse& response) {
  if (test_interfaces_->testRunner()->shouldDumpResourceLoadCallbacks()) {
    if (resource_identifier_map_.find(identifier) ==
        resource_identifier_map_.end())
      delegate_->printMessage("<unknown>");
    else
      delegate_->printMessage(resource_identifier_map_[identifier]);
    delegate_->printMessage(" - didReceiveResponse ");
    printResponseDescription(delegate_, response);
    delegate_->printMessage("\n");
  }
  if (test_interfaces_->testRunner()->shouldDumpResourceResponseMIMETypes()) {
    GURL url = response.url();
    WebString mimeType = response.mimeType();
    delegate_->printMessage(url.ExtractFileName());
    delegate_->printMessage(" has MIME type ");
    // Simulate NSURLResponse's mapping of empty/unknown MIME types to
    // application/octet-stream
    delegate_->printMessage(mimeType.isEmpty() ? "application/octet-stream"
                                               : mimeType.utf8().data());
    delegate_->printMessage("\n");
  }
}

void WebTestProxyBase::DidChangeResourcePriority(
    WebLocalFrame*, unsigned identifier,
    const blink::WebURLRequest::Priority& priority, int intra_priority_value) {
  if (test_interfaces_->testRunner()->shouldDumpResourcePriorities()) {
    if (resource_identifier_map_.find(identifier) ==
        resource_identifier_map_.end())
      delegate_->printMessage("<unknown>");
    else
      delegate_->printMessage(resource_identifier_map_[identifier]);
    delegate_->printMessage(" changed priority to ");
    delegate_->printMessage(PriorityDescription(priority));
    char buffer[64];
    snprintf(buffer, sizeof(buffer), ", intra_priority %d",
             intra_priority_value);
    delegate_->printMessage(buffer);
    delegate_->printMessage("\n");
  }
}

void WebTestProxyBase::DidFinishResourceLoad(WebLocalFrame*,
                                             unsigned identifier) {
  if (test_interfaces_->testRunner()->shouldDumpResourceLoadCallbacks()) {
    if (resource_identifier_map_.find(identifier) ==
        resource_identifier_map_.end())
      delegate_->printMessage("<unknown>");
    else
      delegate_->printMessage(resource_identifier_map_[identifier]);
    delegate_->printMessage(" - didFinishLoading\n");
  }
  resource_identifier_map_.erase(identifier);
}

void WebTestProxyBase::DidAddMessageToConsole(const WebConsoleMessage& message,
                                              const WebString& sourceName,
                                              unsigned sourceLine) {
  // This matches win DumpRenderTree's UIDelegate.cpp.
  if (!log_console_output_) return;
  string level;
  switch (message.level) {
    case WebConsoleMessage::LevelDebug:
      level = "DEBUG";
      break;
    case WebConsoleMessage::LevelLog:
      level = "MESSAGE";
      break;
    case WebConsoleMessage::LevelInfo:
      level = "INFO";
      break;
    case WebConsoleMessage::LevelWarning:
      level = "WARNING";
      break;
    case WebConsoleMessage::LevelError:
      level = "ERROR";
      break;
  }
  delegate_->printMessage(string("CONSOLE ") + level + ": ");
  if (sourceLine) {
    char buffer[40];
    snprintf(buffer, sizeof(buffer), "line %d: ", sourceLine);
    delegate_->printMessage(buffer);
  }
  if (!message.text.isEmpty()) {
    string newMessage;
    newMessage = message.text.utf8();
    size_t fileProtocol = newMessage.find("file://");
    if (fileProtocol != string::npos) {
      newMessage = newMessage.substr(0, fileProtocol) +
                   urlSuitableForTestResult(newMessage.substr(fileProtocol));
    }
    delegate_->printMessage(newMessage);
  }
  delegate_->printMessage(string("\n"));
}

void WebTestProxyBase::LocationChangeDone(WebFrame* frame) {
  if (frame != test_interfaces_->testRunner()->topLoadingFrame()) return;
  test_interfaces_->testRunner()->setTopLoadingFrame(frame, true);
}

WebNavigationPolicy WebTestProxyBase::DecidePolicyForNavigation(
    WebLocalFrame*, WebDataSource::ExtraData*, const WebURLRequest& request,
    WebNavigationType type, WebNavigationPolicy defaultPolicy,
    bool isRedirect) {
  WebNavigationPolicy result;
  if (!test_interfaces_->testRunner()->policyDelegateEnabled())
    return defaultPolicy;

  delegate_->printMessage(string("Policy delegate: attempt to load ") +
                          URLDescription(request.url()) +
                          " with navigation type '" +
                          webNavigationTypeToString(type) + "'\n");
  if (test_interfaces_->testRunner()->policyDelegateIsPermissive())
    result = blink::WebNavigationPolicyCurrentTab;
  else
    result = blink::WebNavigationPolicyIgnore;

  if (test_interfaces_->testRunner()->policyDelegateShouldNotifyDone())
    test_interfaces_->testRunner()->policyDelegateDone();
  return result;
}

bool WebTestProxyBase::WillCheckAndDispatchMessageEvent(WebLocalFrame*,
                                                        WebFrame*,
                                                        WebSecurityOrigin,
                                                        WebDOMMessageEvent) {
  if (test_interfaces_->testRunner()->shouldInterceptPostMessage()) {
    delegate_->printMessage("intercepted postMessage\n");
    return true;
  }

  return false;
}

void WebTestProxyBase::PostSpellCheckEvent(const WebString& eventName) {
  if (test_interfaces_->testRunner()->shouldDumpSpellCheckCallbacks()) {
    delegate_->printMessage(string("SpellCheckEvent: ") +
                            eventName.utf8().data() + "\n");
  }
}

void WebTestProxyBase::ResetInputMethod() {
  // If a composition text exists, then we need to let the browser process
  // to cancel the input method's ongoing composition session.
  if (web_widget_) web_widget_->confirmComposition();
}

}  // namespace content
