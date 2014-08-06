// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/web_test_proxy.h"

#include <cctype>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "content/public/common/content_switches.h"
#include "content/shell/renderer/test_runner/TestInterfaces.h"
#include "content/shell/renderer/test_runner/TestPlugin.h"
#include "content/shell/renderer/test_runner/WebTestDelegate.h"
#include "content/shell/renderer/test_runner/WebTestInterfaces.h"
#include "content/shell/renderer/test_runner/accessibility_controller.h"
#include "content/shell/renderer/test_runner/event_sender.h"
#include "content/shell/renderer/test_runner/mock_color_chooser.h"
#include "content/shell/renderer/test_runner/mock_screen_orientation_client.h"
#include "content/shell/renderer/test_runner/mock_web_push_client.h"
#include "content/shell/renderer/test_runner/mock_web_speech_recognizer.h"
#include "content/shell/renderer/test_runner/mock_web_user_media_client.h"
#include "content/shell/renderer/test_runner/spell_check_client.h"
#include "content/shell/renderer/test_runner/test_runner.h"
#include "content/shell/renderer/test_runner/web_test_runner.h"
// FIXME: Including platform_canvas.h here is a layering violation.
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebClipboard.h"
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

namespace content {

namespace {

class HostMethodTask : public WebMethodTask<WebTestProxyBase> {
 public:
  typedef void (WebTestProxyBase::*CallbackMethodType)();
  HostMethodTask(WebTestProxyBase* object, CallbackMethodType callback)
      : WebMethodTask<WebTestProxyBase>(object), callback_(callback) {}

  virtual void runIfValid() OVERRIDE { (m_object->*callback_)(); }

 private:
  CallbackMethodType callback_;
};

void PrintFrameDescription(WebTestDelegate* delegate, blink::WebFrame* frame) {
  std::string name8 = frame->uniqueName().utf8();
  if (frame == frame->view()->mainFrame()) {
    if (!name8.length()) {
      delegate->printMessage("main frame");
      return;
    }
    delegate->printMessage(std::string("main frame \"") + name8 + "\"");
    return;
  }
  if (!name8.length()) {
    delegate->printMessage("frame (anonymous)");
    return;
  }
  delegate->printMessage(std::string("frame \"") + name8 + "\"");
}

void PrintFrameuserGestureStatus(WebTestDelegate* delegate,
                                 blink::WebFrame* frame,
                                 const char* msg) {
  bool is_user_gesture =
      blink::WebUserGestureIndicator::isProcessingUserGesture();
  delegate->printMessage(std::string("Frame with user gesture \"") +
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
    delegate->printMessage("(null)");
    return;
  }
  delegate->printMessage(base::StringPrintf(
      "<NSURLResponse %s, http status code %d>",
      DescriptionSuitableForTestResult(response.url().spec()).c_str(),
      response.httpStatusCode()));
}

std::string URLDescription(const GURL& url) {
  if (url.SchemeIs("file"))
    return url.ExtractFileName();
  return url.possibly_invalid_spec();
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
  return host == "127.0.0.1" || host == "localhost";
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

std::string DumpFrameHeaderIfNeeded(blink::WebFrame* frame) {
  std::string result;

  // Add header for all but the main frame. Skip empty frames.
  if (frame->parent() && !frame->document().documentElement().isNull()) {
    result.append("\n--------\nFrame: '");
    result.append(frame->uniqueName().utf8().data());
    result.append("'\n--------\n");
  }

  return result;
}

std::string DumpFramesAsMarkup(blink::WebFrame* frame, bool recursive) {
  std::string result = DumpFrameHeaderIfNeeded(frame);
  result.append(frame->contentAsMarkup().utf8());
  result.append("\n");

  if (recursive) {
    for (blink::WebFrame* child = frame->firstChild(); child;
         child = child->nextSibling())
      result.append(DumpFramesAsMarkup(child, recursive));
  }

  return result;
}

std::string DumpDocumentText(blink::WebFrame* frame) {
  // We use the document element's text instead of the body text here because
  // not all documents have a body, such as XML documents.
  blink::WebElement document_element = frame->document().documentElement();
  if (document_element.isNull())
    return std::string();
  return document_element.innerText().utf8();
}

std::string DumpFramesAsText(blink::WebFrame* frame, bool recursive) {
  std::string result = DumpFrameHeaderIfNeeded(frame);
  result.append(DumpDocumentText(frame));
  result.append("\n");

  if (recursive) {
    for (blink::WebFrame* child = frame->firstChild(); child;
         child = child->nextSibling())
      result.append(DumpFramesAsText(child, recursive));
  }

  return result;
}

std::string DumpFramesAsPrintedText(blink::WebFrame* frame, bool recursive) {
  // Cannot do printed format for anything other than HTML
  if (!frame->document().isHTMLDocument())
    return std::string();

  std::string result = DumpFrameHeaderIfNeeded(frame);
  result.append(
      frame->renderTreeAsText(blink::WebFrame::RenderAsTextPrinting).utf8());
  result.append("\n");

  if (recursive) {
    for (blink::WebFrame* child = frame->firstChild(); child;
         child = child->nextSibling())
      result.append(DumpFramesAsPrintedText(child, recursive));
  }

  return result;
}

std::string DumpFrameScrollPosition(blink::WebFrame* frame, bool recursive) {
  std::string result;
  blink::WebSize offset = frame->scrollOffset();
  if (offset.width > 0 || offset.height > 0) {
    if (frame->parent()) {
      result =
          std::string("frame '") + frame->uniqueName().utf8().data() + "' ";
    }
    base::StringAppendF(
        &result, "scrolled to %d,%d\n", offset.width, offset.height);
  }

  if (!recursive)
    return result;
  for (blink::WebFrame* child = frame->firstChild(); child;
       child = child->nextSibling())
    result += DumpFrameScrollPosition(child, recursive);
  return result;
}

std::string DumpAllBackForwardLists(TestInterfaces* interfaces,
                                    WebTestDelegate* delegate) {
  std::string result;
  const std::vector<WebTestProxyBase*>& window_list = interfaces->windowList();
  for (size_t i = 0; i < window_list.size(); ++i)
    result.append(delegate->dumpHistoryForWindow(window_list.at(i)));
  return result;
}
}

WebTestProxyBase::WebTestProxyBase()
    : test_interfaces_(NULL),
      delegate_(NULL),
      web_widget_(NULL),
      spellcheck_(new SpellCheckClient(this)),
      chooser_count_(0) {
  // TODO(enne): using the scheduler introduces additional composite steps
  // that create flakiness.  This should go away eventually.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableSingleThreadProxyScheduler);
  Reset();
}

WebTestProxyBase::~WebTestProxyBase() {
  test_interfaces_->windowClosed(this);
  // Tests must wait for readback requests to finish before notifying that
  // they are done.
  CHECK_EQ(0u, composite_and_readback_callbacks_.size());
}

void WebTestProxyBase::SetInterfaces(WebTestInterfaces* interfaces) {
  test_interfaces_ = interfaces->testInterfaces();
  test_interfaces_->windowOpened(this);
}

void WebTestProxyBase::SetDelegate(WebTestDelegate* delegate) {
  delegate_ = delegate;
  spellcheck_->SetDelegate(delegate);
  if (speech_recognizer_.get())
    speech_recognizer_->SetDelegate(delegate);
}

blink::WebView* WebTestProxyBase::GetWebView() const {
  DCHECK(web_widget_);
  // TestRunner does not support popup widgets. So |web_widget|_ is always a
  // WebView.
  return static_cast<blink::WebView*>(web_widget_);
}

void WebTestProxyBase::Reset() {
  animate_scheduled_ = false;
  resource_identifier_map_.clear();
  log_console_output_ = true;
  if (midi_client_.get())
    midi_client_->resetMock();
  accept_languages_ = "";
}

blink::WebSpellCheckClient* WebTestProxyBase::GetSpellCheckClient() const {
  return spellcheck_.get();
}

blink::WebColorChooser* WebTestProxyBase::CreateColorChooser(
    blink::WebColorChooserClient* client,
    const blink::WebColor& color,
    const blink::WebVector<blink::WebColorSuggestion>& suggestions) {
  // This instance is deleted by WebCore::ColorInputType
  return new MockColorChooser(client, delegate_, this);
}

bool WebTestProxyBase::RunFileChooser(
    const blink::WebFileChooserParams& params,
    blink::WebFileChooserCompletion* completion) {
  delegate_->printMessage("Mock: Opening a file chooser.\n");
  // FIXME: Add ability to set file names to a file upload control.
  return false;
}

void WebTestProxyBase::ShowValidationMessage(
    const blink::WebRect& anchor_in_root_view,
    const blink::WebString& message,
    const blink::WebString& sub_message,
    blink::WebTextDirection hint) {
  delegate_->printMessage(
      std::string("ValidationMessageClient: main-message=") +
      std::string(message.utf8()) + " sub-message=" +
      std::string(sub_message.utf8()) + "\n");
}

std::string WebTestProxyBase::CaptureTree(bool debug_render_tree) {
  bool should_dump_custom_text =
      test_interfaces_->testRunner()->shouldDumpAsCustomText();
  bool should_dump_as_text = test_interfaces_->testRunner()->shouldDumpAsText();
  bool should_dump_as_markup =
      test_interfaces_->testRunner()->shouldDumpAsMarkup();
  bool should_dump_as_printed = test_interfaces_->testRunner()->isPrinting();
  blink::WebFrame* frame = GetWebView()->mainFrame();
  std::string data_utf8;
  if (should_dump_custom_text) {
    // Append a newline for the test driver.
    data_utf8 = test_interfaces_->testRunner()->customDumpText() + "\n";
  } else if (should_dump_as_text) {
    bool recursive =
        test_interfaces_->testRunner()->shouldDumpChildFramesAsText();
    data_utf8 = should_dump_as_printed ?
        DumpFramesAsPrintedText(frame, recursive) :
        DumpFramesAsText(frame, recursive);
  } else if (should_dump_as_markup) {
    bool recursive =
        test_interfaces_->testRunner()->shouldDumpChildFramesAsMarkup();
    // Append a newline for the test driver.
    data_utf8 = DumpFramesAsMarkup(frame, recursive);
  } else {
    bool recursive =
        test_interfaces_->testRunner()->shouldDumpChildFrameScrollPositions();
    blink::WebFrame::RenderAsTextControls render_text_behavior =
        blink::WebFrame::RenderAsTextNormal;
    if (should_dump_as_printed)
      render_text_behavior |= blink::WebFrame::RenderAsTextPrinting;
    if (debug_render_tree)
      render_text_behavior |= blink::WebFrame::RenderAsTextDebug;
    data_utf8 = frame->renderTreeAsText(render_text_behavior).utf8();
    data_utf8 += DumpFrameScrollPosition(frame, recursive);
  }

  if (test_interfaces_->testRunner()->ShouldDumpBackForwardList())
    data_utf8 += DumpAllBackForwardLists(test_interfaces_, delegate_);

  return data_utf8;
}

void WebTestProxyBase::DrawSelectionRect(SkCanvas* canvas) {
  // See if we need to draw the selection bounds rect. Selection bounds
  // rect is the rect enclosing the (possibly transformed) selection.
  // The rect should be drawn after everything is laid out and painted.
  if (!test_interfaces_->testRunner()->shouldDumpSelectionRect())
    return;
  // If there is a selection rect - draw a red 1px border enclosing rect
  blink::WebRect wr = GetWebView()->mainFrame()->selectionBoundsRect();
  if (wr.isEmpty())
    return;
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
  TRACE_EVENT2("shell",
               "WebTestProxyBase::didCompositeAndReadback",
               "x",
               bitmap.info().fWidth,
               "y",
               bitmap.info().fHeight);
  SkCanvas canvas(bitmap);
  DrawSelectionRect(&canvas);
  DCHECK(!composite_and_readback_callbacks_.empty());
  composite_and_readback_callbacks_.front().Run(bitmap);
  composite_and_readback_callbacks_.pop_front();
}

void WebTestProxyBase::SetAcceptLanguages(const std::string& accept_languages) {
  bool notify = accept_languages_ != accept_languages;
  accept_languages_ = accept_languages;

  if (notify)
    GetWebView()->acceptLanguagesChanged();
}

void WebTestProxyBase::CopyImageAtAndCapturePixels(
    int x, int y, const base::Callback<void(const SkBitmap&)>& callback) {
  DCHECK(web_widget_->isAcceleratedCompositingActive());
  DCHECK(!callback.is_null());
  uint64_t sequence_number =  blink::Platform::current()->clipboard()->
      sequenceNumber(blink::WebClipboard::Buffer());
  GetWebView()->copyImageAt(blink::WebPoint(x, y));
  if (sequence_number == blink::Platform::current()->clipboard()->
      sequenceNumber(blink::WebClipboard::Buffer())) {
    SkBitmap emptyBitmap;
    callback.Run(emptyBitmap);
    return;
  }

  blink::WebData data = blink::Platform::current()->clipboard()->readImage(
      blink::WebClipboard::Buffer());
  blink::WebImage image = blink::WebImage::fromData(data, blink::WebSize());
  const SkBitmap& bitmap = image.getSkBitmap();
  SkAutoLockPixels autoLock(bitmap);
  callback.Run(bitmap);
}

void WebTestProxyBase::CapturePixelsForPrinting(
    const base::Callback<void(const SkBitmap&)>& callback) {
  web_widget_->layout();

  blink::WebSize page_size_in_pixels = web_widget_->size();
  blink::WebFrame* web_frame = GetWebView()->mainFrame();

  int page_count = web_frame->printBegin(page_size_in_pixels);
  int totalHeight = page_count * (page_size_in_pixels.height + 1) - 1;

  bool is_opaque = false;
  skia::RefPtr<SkCanvas> canvas(skia::AdoptRef(skia::TryCreateBitmapCanvas(
      page_size_in_pixels.width, totalHeight, is_opaque)));
  if (!canvas) {
    callback.Run(SkBitmap());
    return;
  }
  web_frame->printPagesWithBoundaries(canvas.get(), page_size_in_pixels);
  web_frame->printEnd();

  DrawSelectionRect(canvas.get());
  SkBaseDevice* device = skia::GetTopDevice(*canvas);
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
        FROM_HERE,
        base::Bind(&WebTestProxyBase::CapturePixelsForPrinting,
                   base::Unretained(this),
                   callback));
    return;
  }

  composite_and_readback_callbacks_.push_back(callback);
  web_widget_->compositeAndReadbackAsync(this);
}

void WebTestProxyBase::SetLogConsoleOutput(bool enabled) {
  log_console_output_ = enabled;
}

void WebTestProxyBase::DidDisplayAsync(const base::Closure& callback,
                                       const SkBitmap& bitmap) {
  // Verify we actually composited.
  CHECK_NE(0, bitmap.info().fWidth);
  CHECK_NE(0, bitmap.info().fHeight);
  if (!callback.is_null())
    callback.Run();
}

void WebTestProxyBase::DisplayAsyncThen(const base::Closure& callback) {
  TRACE_EVENT0("shell", "WebTestProxyBase::DisplayAsyncThen");

  CHECK(web_widget_->isAcceleratedCompositingActive());
  CapturePixelsAsync(base::Bind(
      &WebTestProxyBase::DidDisplayAsync, base::Unretained(this), callback));
}

void WebTestProxyBase::GetScreenOrientationForTesting(
    blink::WebScreenInfo& screen_info) {
  if (!screen_orientation_client_)
    return;
  // Override screen orientation information with mock data.
  screen_info.orientationType =
      screen_orientation_client_->CurrentOrientationType();
  screen_info.orientationAngle =
      screen_orientation_client_->CurrentOrientationAngle();
}

MockScreenOrientationClient*
WebTestProxyBase::GetScreenOrientationClientMock() {
  if (!screen_orientation_client_.get()) {
    screen_orientation_client_.reset(new MockScreenOrientationClient);
  }
  return screen_orientation_client_.get();
}

blink::WebMIDIClientMock* WebTestProxyBase::GetMIDIClientMock() {
  if (!midi_client_.get())
    midi_client_.reset(new blink::WebMIDIClientMock);
  return midi_client_.get();
}

MockWebSpeechRecognizer* WebTestProxyBase::GetSpeechRecognizerMock() {
  if (!speech_recognizer_.get()) {
    speech_recognizer_.reset(new MockWebSpeechRecognizer());
    speech_recognizer_->SetDelegate(delegate_);
  }
  return speech_recognizer_.get();
}

void WebTestProxyBase::ScheduleAnimation() {
  if (!test_interfaces_->testRunner()->TestIsRunning())
    return;

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

void WebTestProxyBase::PostAccessibilityEvent(const blink::WebAXObject& obj,
                                              blink::WebAXEvent event) {
  // Only hook the accessibility events occured during the test run.
  // This check prevents false positives in WebLeakDetector.
  // The pending tasks in browser/renderer message queue may trigger
  // accessibility events,
  // and AccessibilityController will hold on to their target nodes if we don't
  // ignore them here.
  if (!test_interfaces_->testRunner()->TestIsRunning())
    return;

  if (event == blink::WebAXEventFocus)
    test_interfaces_->accessibilityController()->SetFocusedElement(obj);

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
    case blink::WebAXEventFocus:
      event_name = "Focus";
      break;
    case blink::WebAXEventHide:
      event_name = "Hide";
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

  test_interfaces_->accessibilityController()->NotificationReceived(obj,
                                                                    event_name);

  if (test_interfaces_->accessibilityController()
          ->ShouldLogAccessibilityEvents()) {
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

    delegate_->printMessage(message + "\n");
  }
}

void WebTestProxyBase::StartDragging(blink::WebLocalFrame* frame,
                                     const blink::WebDragData& data,
                                     blink::WebDragOperationsMask mask,
                                     const blink::WebImage& image,
                                     const blink::WebPoint& point) {
  // When running a test, we need to fake a drag drop operation otherwise
  // Windows waits for real mouse events to know when the drag is over.
  test_interfaces_->eventSender()->DoDragDrop(data, mask);
}

// The output from these methods in layout test mode should match that
// expected by the layout tests. See EditingDelegate.m in DumpRenderTree.

void WebTestProxyBase::DidChangeSelection(bool is_empty_callback) {
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

bool WebTestProxyBase::CreateView(blink::WebLocalFrame* frame,
                                  const blink::WebURLRequest& request,
                                  const blink::WebWindowFeatures& features,
                                  const blink::WebString& frame_name,
                                  blink::WebNavigationPolicy policy,
                                  bool suppress_opener) {
  if (!test_interfaces_->testRunner()->canOpenWindows())
    return false;
  if (test_interfaces_->testRunner()->shouldDumpCreateView())
    delegate_->printMessage(std::string("createView(") +
                            URLDescription(request.url()) + ")\n");
  return true;
}

blink::WebPlugin* WebTestProxyBase::CreatePlugin(
    blink::WebLocalFrame* frame,
    const blink::WebPluginParams& params) {
  if (TestPlugin::isSupportedMimeType(params.mimeType))
    return TestPlugin::create(frame, params, delegate_);
  return 0;
}

void WebTestProxyBase::SetStatusText(const blink::WebString& text) {
  if (!test_interfaces_->testRunner()->shouldDumpStatusCallbacks())
    return;
  delegate_->printMessage(
      std::string("UI DELEGATE STATUS CALLBACK: setStatusText:") +
      text.utf8().data() + "\n");
}

void WebTestProxyBase::DidStopLoading() {
  if (test_interfaces_->testRunner()->shouldDumpProgressFinishedCallback())
    delegate_->printMessage("postProgressFinishedNotification\n");
}

void WebTestProxyBase::ShowContextMenu(
    blink::WebLocalFrame* frame,
    const blink::WebContextMenuData& context_menu_data) {
  test_interfaces_->eventSender()->SetContextMenuData(context_menu_data);
}

blink::WebUserMediaClient* WebTestProxyBase::GetUserMediaClient() {
  if (!user_media_client_.get())
    user_media_client_.reset(new MockWebUserMediaClient(delegate_));
  return user_media_client_.get();
}

// Simulate a print by going into print mode and then exit straight away.
void WebTestProxyBase::PrintPage(blink::WebLocalFrame* frame) {
  blink::WebSize page_size_in_pixels = web_widget_->size();
  if (page_size_in_pixels.isEmpty())
    return;
  blink::WebPrintParams printParams(page_size_in_pixels);
  frame->printBegin(printParams);
  frame->printEnd();
}

blink::WebNotificationPresenter* WebTestProxyBase::GetNotificationPresenter() {
  return test_interfaces_->testRunner()->notification_presenter();
}

blink::WebMIDIClient* WebTestProxyBase::GetWebMIDIClient() {
  return GetMIDIClientMock();
}

blink::WebSpeechRecognizer* WebTestProxyBase::GetSpeechRecognizer() {
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

void WebTestProxyBase::DidFocus() {
  delegate_->setFocus(this, true);
}

void WebTestProxyBase::DidBlur() {
  delegate_->setFocus(this, false);
}

void WebTestProxyBase::SetToolTipText(const blink::WebString& text,
                                      blink::WebTextDirection direction) {
  test_interfaces_->testRunner()->setToolTipText(text);
}

void WebTestProxyBase::DidOpenChooser() {
  chooser_count_++;
}

void WebTestProxyBase::DidCloseChooser() {
  chooser_count_--;
}

bool WebTestProxyBase::IsChooserShown() {
  return 0 < chooser_count_;
}

void WebTestProxyBase::LoadURLExternally(
    blink::WebLocalFrame* frame,
    const blink::WebURLRequest& request,
    blink::WebNavigationPolicy policy,
    const blink::WebString& suggested_name) {
  if (test_interfaces_->testRunner()->shouldWaitUntilExternalURLLoad()) {
    if (policy == blink::WebNavigationPolicyDownload) {
      delegate_->printMessage(
          std::string("Downloading URL with suggested filename \"") +
          suggested_name.utf8() + "\"\n");
    } else {
      delegate_->printMessage(std::string("Loading URL externally - \"") +
                              URLDescription(request.url()) + "\"\n");
    }
    delegate_->testFinished();
  }
}

void WebTestProxyBase::DidStartProvisionalLoad(blink::WebLocalFrame* frame) {
  if (!test_interfaces_->testRunner()->topLoadingFrame())
    test_interfaces_->testRunner()->setTopLoadingFrame(frame, false);

  if (test_interfaces_->testRunner()->shouldDumpFrameLoadCallbacks()) {
    PrintFrameDescription(delegate_, frame);
    delegate_->printMessage(" - didStartProvisionalLoadForFrame\n");
  }

  if (test_interfaces_->testRunner()
          ->shouldDumpUserGestureInFrameLoadCallbacks()) {
    PrintFrameuserGestureStatus(
        delegate_, frame, " - in didStartProvisionalLoadForFrame\n");
  }
}

void WebTestProxyBase::DidReceiveServerRedirectForProvisionalLoad(
    blink::WebLocalFrame* frame) {
  if (test_interfaces_->testRunner()->shouldDumpFrameLoadCallbacks()) {
    PrintFrameDescription(delegate_, frame);
    delegate_->printMessage(
        " - didReceiveServerRedirectForProvisionalLoadForFrame\n");
  }
}

bool WebTestProxyBase::DidFailProvisionalLoad(blink::WebLocalFrame* frame,
                                              const blink::WebURLError& error) {
  if (test_interfaces_->testRunner()->shouldDumpFrameLoadCallbacks()) {
    PrintFrameDescription(delegate_, frame);
    delegate_->printMessage(" - didFailProvisionalLoadWithError\n");
  }
  LocationChangeDone(frame);
  return !frame->provisionalDataSource();
}

void WebTestProxyBase::DidCommitProvisionalLoad(
    blink::WebLocalFrame* frame,
    const blink::WebHistoryItem& history_item,
    blink::WebHistoryCommitType history_type) {
  if (test_interfaces_->testRunner()->shouldDumpFrameLoadCallbacks()) {
    PrintFrameDescription(delegate_, frame);
    delegate_->printMessage(" - didCommitLoadForFrame\n");
  }
}

void WebTestProxyBase::DidReceiveTitle(blink::WebLocalFrame* frame,
                                       const blink::WebString& title,
                                       blink::WebTextDirection direction) {
  blink::WebCString title8 = title.utf8();

  if (test_interfaces_->testRunner()->shouldDumpFrameLoadCallbacks()) {
    PrintFrameDescription(delegate_, frame);
    delegate_->printMessage(std::string(" - didReceiveTitle: ") +
                            title8.data() + "\n");
  }

  if (test_interfaces_->testRunner()->shouldDumpTitleChanges())
    delegate_->printMessage(std::string("TITLE CHANGED: '") + title8.data() +
                            "'\n");
}

void WebTestProxyBase::DidChangeIcon(blink::WebLocalFrame* frame,
                                     blink::WebIconURL::Type icon_type) {
  if (test_interfaces_->testRunner()->shouldDumpIconChanges()) {
    PrintFrameDescription(delegate_, frame);
    delegate_->printMessage(std::string(" - didChangeIcons\n"));
  }
}

void WebTestProxyBase::DidFinishDocumentLoad(blink::WebLocalFrame* frame) {
  if (test_interfaces_->testRunner()->shouldDumpFrameLoadCallbacks()) {
    PrintFrameDescription(delegate_, frame);
    delegate_->printMessage(" - didFinishDocumentLoadForFrame\n");
  } else {
    unsigned pendingUnloadEvents = frame->unloadListenerCount();
    if (pendingUnloadEvents) {
      PrintFrameDescription(delegate_, frame);
      delegate_->printMessage(base::StringPrintf(
          " - has %u onunload handler(s)\n", pendingUnloadEvents));
    }
  }
}

void WebTestProxyBase::DidHandleOnloadEvents(blink::WebLocalFrame* frame) {
  if (test_interfaces_->testRunner()->shouldDumpFrameLoadCallbacks()) {
    PrintFrameDescription(delegate_, frame);
    delegate_->printMessage(" - didHandleOnloadEventsForFrame\n");
  }
}

void WebTestProxyBase::DidFailLoad(blink::WebLocalFrame* frame,
                                   const blink::WebURLError& error) {
  if (test_interfaces_->testRunner()->shouldDumpFrameLoadCallbacks()) {
    PrintFrameDescription(delegate_, frame);
    delegate_->printMessage(" - didFailLoadWithError\n");
  }
  LocationChangeDone(frame);
}

void WebTestProxyBase::DidFinishLoad(blink::WebLocalFrame* frame) {
  if (test_interfaces_->testRunner()->shouldDumpFrameLoadCallbacks()) {
    PrintFrameDescription(delegate_, frame);
    delegate_->printMessage(" - didFinishLoadForFrame\n");
  }
  LocationChangeDone(frame);
}

void WebTestProxyBase::DidDetectXSS(blink::WebLocalFrame* frame,
                                    const blink::WebURL& insecure_url,
                                    bool did_block_entire_page) {
  if (test_interfaces_->testRunner()->shouldDumpFrameLoadCallbacks())
    delegate_->printMessage("didDetectXSS\n");
}

void WebTestProxyBase::DidDispatchPingLoader(blink::WebLocalFrame* frame,
                                             const blink::WebURL& url) {
  if (test_interfaces_->testRunner()->shouldDumpPingLoaderCallbacks())
    delegate_->printMessage(std::string("PingLoader dispatched to '") +
                            URLDescription(url).c_str() + "'.\n");
}

void WebTestProxyBase::WillRequestResource(
    blink::WebLocalFrame* frame,
    const blink::WebCachedURLRequest& request) {
  if (test_interfaces_->testRunner()->shouldDumpResourceRequestCallbacks()) {
    PrintFrameDescription(delegate_, frame);
    delegate_->printMessage(std::string(" - ") +
                            request.initiatorName().utf8().data());
    delegate_->printMessage(std::string(" requested '") +
                            URLDescription(request.urlRequest().url()).c_str() +
                            "'\n");
  }
}

void WebTestProxyBase::WillSendRequest(
    blink::WebLocalFrame* frame,
    unsigned identifier,
    blink::WebURLRequest& request,
    const blink::WebURLResponse& redirect_response) {
  // Need to use GURL for host() and SchemeIs()
  GURL url = request.url();
  std::string request_url = url.possibly_invalid_spec();

  GURL main_document_url = request.firstPartyForCookies();

  if (redirect_response.isNull() &&
      (test_interfaces_->testRunner()->shouldDumpResourceLoadCallbacks() ||
       test_interfaces_->testRunner()->shouldDumpResourcePriorities())) {
    DCHECK(resource_identifier_map_.find(identifier) ==
           resource_identifier_map_.end());
    resource_identifier_map_[identifier] =
        DescriptionSuitableForTestResult(request_url);
  }

  if (test_interfaces_->testRunner()->shouldDumpResourceLoadCallbacks()) {
    if (resource_identifier_map_.find(identifier) ==
        resource_identifier_map_.end())
      delegate_->printMessage("<unknown>");
    else
      delegate_->printMessage(resource_identifier_map_[identifier]);
    delegate_->printMessage(" - willSendRequest <NSURLRequest URL ");
    delegate_->printMessage(
        DescriptionSuitableForTestResult(request_url).c_str());
    delegate_->printMessage(", main document URL ");
    delegate_->printMessage(URLDescription(main_document_url).c_str());
    delegate_->printMessage(", http method ");
    delegate_->printMessage(request.httpMethod().utf8().data());
    delegate_->printMessage("> redirectResponse ");
    PrintResponseDescription(delegate_, redirect_response);
    delegate_->printMessage("\n");
  }

  if (test_interfaces_->testRunner()->shouldDumpResourcePriorities()) {
    delegate_->printMessage(
        DescriptionSuitableForTestResult(request_url).c_str());
    delegate_->printMessage(" has priority ");
    delegate_->printMessage(PriorityDescription(request.priority()));
    delegate_->printMessage("\n");
  }

  if (test_interfaces_->testRunner()->httpHeadersToClear()) {
    const std::set<std::string>* clearHeaders =
        test_interfaces_->testRunner()->httpHeadersToClear();
    for (std::set<std::string>::const_iterator header = clearHeaders->begin();
         header != clearHeaders->end();
         ++header)
      request.clearHTTPHeaderField(blink::WebString::fromUTF8(*header));
  }

  std::string host = url.host();
  if (!host.empty() && (url.SchemeIs("http") || url.SchemeIs("https"))) {
    if (!IsLocalHost(host) && !HostIsUsedBySomeTestsToGenerateError(host) &&
        ((!main_document_url.SchemeIs("http") &&
          !main_document_url.SchemeIs("https")) ||
         IsLocalHost(main_document_url.host())) &&
        !delegate_->allowExternalPages()) {
      delegate_->printMessage(std::string("Blocked access to external URL ") +
                              request_url + "\n");
      BlockRequest(request);
      return;
    }
  }

  // Set the new substituted URL.
  request.setURL(delegate_->rewriteLayoutTestsURL(request.url().spec()));
}

void WebTestProxyBase::DidReceiveResponse(
    blink::WebLocalFrame* frame,
    unsigned identifier,
    const blink::WebURLResponse& response) {
  if (test_interfaces_->testRunner()->shouldDumpResourceLoadCallbacks()) {
    if (resource_identifier_map_.find(identifier) ==
        resource_identifier_map_.end())
      delegate_->printMessage("<unknown>");
    else
      delegate_->printMessage(resource_identifier_map_[identifier]);
    delegate_->printMessage(" - didReceiveResponse ");
    PrintResponseDescription(delegate_, response);
    delegate_->printMessage("\n");
  }
  if (test_interfaces_->testRunner()->shouldDumpResourceResponseMIMETypes()) {
    GURL url = response.url();
    blink::WebString mime_type = response.mimeType();
    delegate_->printMessage(url.ExtractFileName());
    delegate_->printMessage(" has MIME type ");
    // Simulate NSURLResponse's mapping of empty/unknown MIME types to
    // application/octet-stream
    delegate_->printMessage(mime_type.isEmpty() ? "application/octet-stream"
                                                : mime_type.utf8().data());
    delegate_->printMessage("\n");
  }
}

void WebTestProxyBase::DidChangeResourcePriority(
    blink::WebLocalFrame* frame,
    unsigned identifier,
    const blink::WebURLRequest::Priority& priority,
    int intra_priority_value) {
  if (test_interfaces_->testRunner()->shouldDumpResourcePriorities()) {
    if (resource_identifier_map_.find(identifier) ==
        resource_identifier_map_.end())
      delegate_->printMessage("<unknown>");
    else
      delegate_->printMessage(resource_identifier_map_[identifier]);
    delegate_->printMessage(
        base::StringPrintf(" changed priority to %s, intra_priority %d\n",
                           PriorityDescription(priority).c_str(),
                           intra_priority_value));
  }
}

void WebTestProxyBase::DidFinishResourceLoad(blink::WebLocalFrame* frame,
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

void WebTestProxyBase::DidAddMessageToConsole(
    const blink::WebConsoleMessage& message,
    const blink::WebString& source_name,
    unsigned source_line) {
  // This matches win DumpRenderTree's UIDelegate.cpp.
  if (!log_console_output_)
    return;
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
  }
  delegate_->printMessage(std::string("CONSOLE ") + level + ": ");
  if (source_line) {
    delegate_->printMessage(base::StringPrintf("line %d: ", source_line));
  }
  if (!message.text.isEmpty()) {
    std::string new_message;
    new_message = message.text.utf8();
    size_t file_protocol = new_message.find("file://");
    if (file_protocol != std::string::npos) {
      new_message = new_message.substr(0, file_protocol) +
                    URLSuitableForTestResult(new_message.substr(file_protocol));
    }
    delegate_->printMessage(new_message);
  }
  delegate_->printMessage(std::string("\n"));
}

void WebTestProxyBase::LocationChangeDone(blink::WebFrame* frame) {
  if (frame != test_interfaces_->testRunner()->topLoadingFrame())
    return;
  test_interfaces_->testRunner()->setTopLoadingFrame(frame, true);
}

blink::WebNavigationPolicy WebTestProxyBase::DecidePolicyForNavigation(
    const blink::WebFrameClient::NavigationPolicyInfo& info) {
  blink::WebNavigationPolicy result;
  if (!test_interfaces_->testRunner()->policyDelegateEnabled())
    return info.defaultPolicy;

  delegate_->printMessage(std::string("Policy delegate: attempt to load ") +
                          URLDescription(info.urlRequest.url()) +
                          " with navigation type '" +
                          WebNavigationTypeToString(info.navigationType) + "'\n");
  if (test_interfaces_->testRunner()->policyDelegateIsPermissive())
    result = blink::WebNavigationPolicyCurrentTab;
  else
    result = blink::WebNavigationPolicyIgnore;

  if (test_interfaces_->testRunner()->policyDelegateShouldNotifyDone())
    test_interfaces_->testRunner()->policyDelegateDone();
  return result;
}

bool WebTestProxyBase::WillCheckAndDispatchMessageEvent(
    blink::WebLocalFrame* source_frame,
    blink::WebFrame* target_frame,
    blink::WebSecurityOrigin target,
    blink::WebDOMMessageEvent event) {
  if (test_interfaces_->testRunner()->shouldInterceptPostMessage()) {
    delegate_->printMessage("intercepted postMessage\n");
    return true;
  }

  return false;
}

void WebTestProxyBase::PostSpellCheckEvent(const blink::WebString& event_name) {
  if (test_interfaces_->testRunner()->shouldDumpSpellCheckCallbacks()) {
    delegate_->printMessage(std::string("SpellCheckEvent: ") +
                            event_name.utf8().data() + "\n");
  }
}

void WebTestProxyBase::ResetInputMethod() {
  // If a composition text exists, then we need to let the browser process
  // to cancel the input method's ongoing composition session.
  if (web_widget_)
    web_widget_->confirmComposition();
}

blink::WebString WebTestProxyBase::acceptLanguages() {
  return blink::WebString::fromUTF8(accept_languages_);
}

MockWebPushClient* WebTestProxyBase::GetPushClientMock() {
  if (!push_client_.get())
    push_client_.reset(new MockWebPushClient);
  return push_client_.get();
}

blink::WebPushClient* WebTestProxyBase::GetWebPushClient() {
  return GetPushClientMock();
}

}  // namespace content
