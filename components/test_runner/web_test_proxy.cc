// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/test_runner/web_test_proxy.h"

#include <stddef.h>
#include <stdint.h>

#include <cctype>

#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/test_runner/accessibility_controller.h"
#include "components/test_runner/event_sender.h"
#include "components/test_runner/layout_dump.h"
#include "components/test_runner/mock_color_chooser.h"
#include "components/test_runner/mock_credential_manager_client.h"
#include "components/test_runner/mock_screen_orientation_client.h"
#include "components/test_runner/mock_web_speech_recognizer.h"
#include "components/test_runner/mock_web_user_media_client.h"
#include "components/test_runner/spell_check_client.h"
#include "components/test_runner/test_interfaces.h"
#include "components/test_runner/test_plugin.h"
#include "components/test_runner/test_runner.h"
#include "components/test_runner/web_test_delegate.h"
#include "components/test_runner/web_test_interfaces.h"
#include "components/test_runner/web_test_runner.h"
// FIXME: Including platform_canvas.h here is a layering violation.
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebClipboard.h"
#include "third_party/WebKit/public/platform/WebCompositeAndReadbackAsyncCallback.h"
#include "third_party/WebKit/public/platform/WebLayoutAndPaintAsyncCallback.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebAXEnums.h"
#include "third_party/WebKit/public/web/WebAXObject.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebHistoryItem.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebPagePopup.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "third_party/WebKit/public/web/WebPrintParams.h"
#include "third_party/WebKit/public/web/WebRange.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/WebKit/public/web/WebWidgetClient.h"

namespace test_runner {

namespace {

class CaptureCallback : public blink::WebCompositeAndReadbackAsyncCallback {
 public:
  CaptureCallback(const base::Callback<void(const SkBitmap&)>& callback);
  virtual ~CaptureCallback();

  void set_wait_for_popup(bool wait) { wait_for_popup_ = wait; }
  void set_popup_position(const gfx::Point& position) {
    popup_position_ = position;
  }

  // WebCompositeAndReadbackAsyncCallback implementation.
  void didCompositeAndReadback(const SkBitmap& bitmap) override;

 private:
  base::Callback<void(const SkBitmap&)> callback_;
  SkBitmap main_bitmap_;
  bool wait_for_popup_;
  gfx::Point popup_position_;
};

class LayoutAndPaintCallback : public blink::WebLayoutAndPaintAsyncCallback {
 public:
  LayoutAndPaintCallback(const base::Closure& callback)
      : callback_(callback), wait_for_popup_(false) {
  }
  virtual ~LayoutAndPaintCallback() {
  }

  void set_wait_for_popup(bool wait) { wait_for_popup_ = wait; }

  // WebLayoutAndPaintAsyncCallback implementation.
  void didLayoutAndPaint() override;

 private:
  base::Closure callback_;
  bool wait_for_popup_;
};

class HostMethodTask : public WebMethodTask<WebTestProxyBase> {
 public:
  typedef void (WebTestProxyBase::*CallbackMethodType)();
  HostMethodTask(WebTestProxyBase* object, CallbackMethodType callback)
      : WebMethodTask<WebTestProxyBase>(object), callback_(callback) {}

  void RunIfValid() override { (object_->*callback_)(); }

 private:
  CallbackMethodType callback_;
};

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

std::string URLDescription(const GURL& url) {
  if (url.SchemeIs(url::kFileScheme))
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

const char* kPolicyIgnore = "Ignore";
const char* kPolicyDownload = "download";
const char* kPolicyCurrentTab = "current tab";
const char* kPolicyNewBackgroundTab = "new background tab";
const char* kPolicyNewForegroundTab = "new foreground tab";
const char* kPolicyNewWindow = "new window";
const char* kPolicyNewPopup = "new popup";

const char* WebNavigationPolicyToString(blink::WebNavigationPolicy policy) {
  switch (policy) {
    case blink::WebNavigationPolicyIgnore:
      return kPolicyIgnore;
    case blink::WebNavigationPolicyDownload:
      return kPolicyDownload;
    case blink::WebNavigationPolicyCurrentTab:
      return kPolicyCurrentTab;
    case blink::WebNavigationPolicyNewBackgroundTab:
      return kPolicyNewBackgroundTab;
    case blink::WebNavigationPolicyNewForegroundTab:
      return kPolicyNewForegroundTab;
    case blink::WebNavigationPolicyNewWindow:
      return kPolicyNewWindow;
    case blink::WebNavigationPolicyNewPopup:
      return kPolicyNewPopup;
    default:
      return kIllegalString;
  }
}

std::string DumpDeepLayout(blink::WebFrame* frame,
                           LayoutDumpFlags flags,
                           bool recursive) {
  std::string result = DumpLayout(frame->toWebLocalFrame(), flags);

  if (recursive) {
    for (blink::WebFrame* child = frame->firstChild(); child;
         child = child->nextSibling())
      result.append(DumpDeepLayout(child, flags, recursive));
  }

  return result;
}

std::string DumpAllBackForwardLists(TestInterfaces* interfaces,
                                    WebTestDelegate* delegate) {
  std::string result;
  const std::vector<WebTestProxyBase*>& window_list =
      interfaces->GetWindowList();
  for (size_t i = 0; i < window_list.size(); ++i)
    result.append(delegate->DumpHistoryForWindow(window_list.at(i)));
  return result;
}
}

WebTestProxyBase::WebTestProxyBase()
    : web_test_interfaces_(NULL),
      test_interfaces_(NULL),
      delegate_(NULL),
      web_widget_(NULL),
      spellcheck_(new SpellCheckClient(this)),
      chooser_count_(0) {
  Reset();
}

WebTestProxyBase::~WebTestProxyBase() {
  test_interfaces_->WindowClosed(this);
  delegate_->OnWebTestProxyBaseDestroy(this);
}

void WebTestProxyBase::SetInterfaces(WebTestInterfaces* interfaces) {
  web_test_interfaces_ = interfaces;
  test_interfaces_ = interfaces->GetTestInterfaces();
  test_interfaces_->WindowOpened(this);
}

WebTestInterfaces* WebTestProxyBase::GetInterfaces() {
  return web_test_interfaces_;
}

void WebTestProxyBase::SetDelegate(WebTestDelegate* delegate) {
  delegate_ = delegate;
  spellcheck_->SetDelegate(delegate);
  if (speech_recognizer_.get())
    speech_recognizer_->SetDelegate(delegate);
}

WebTestDelegate* WebTestProxyBase::GetDelegate() {
  return delegate_;
}

blink::WebView* WebTestProxyBase::GetWebView() const {
  DCHECK(web_widget_);
  // TestRunner does not support popup widgets. So |web_widget|_ is always a
  // WebView.
  return static_cast<blink::WebView*>(web_widget_);
}

void WebTestProxyBase::Reset() {
  drag_image_.reset();
  animate_scheduled_ = false;
  resource_identifier_map_.clear();
  log_console_output_ = true;
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
  delegate_->PrintMessage("Mock: Opening a file chooser.\n");
  // FIXME: Add ability to set file names to a file upload control.
  return false;
}

void WebTestProxyBase::ShowValidationMessage(
    const blink::WebString& main_message,
    blink::WebTextDirection main_message_hint,
    const blink::WebString& sub_message,
    blink::WebTextDirection sub_message_hint) {
  base::string16 wrapped_main_text = main_message;
  base::string16 wrapped_sub_text = sub_message;

  if (main_message_hint == blink::WebTextDirectionLeftToRight) {
    wrapped_main_text =
        base::i18n::GetDisplayStringInLTRDirectionality(wrapped_main_text);
  } else if (main_message_hint == blink::WebTextDirectionRightToLeft &&
             !base::i18n::IsRTL()) {
    base::i18n::WrapStringWithRTLFormatting(&wrapped_main_text);
  }

  if (!wrapped_sub_text.empty()) {
    if (sub_message_hint == blink::WebTextDirectionLeftToRight) {
      wrapped_sub_text =
          base::i18n::GetDisplayStringInLTRDirectionality(wrapped_sub_text);
    } else if (sub_message_hint == blink::WebTextDirectionRightToLeft) {
      base::i18n::WrapStringWithRTLFormatting(&wrapped_sub_text);
    }
  }
  delegate_->PrintMessage("ValidationMessageClient: main-message=" +
                          base::UTF16ToUTF8(wrapped_main_text) +
                          " sub-message=" +
                          base::UTF16ToUTF8(wrapped_sub_text) + "\n");
}

std::string WebTestProxyBase::CaptureTree(
    bool debug_render_tree,
    bool dump_line_box_trees) {
  TestRunner* test_runner = test_interfaces_->GetTestRunner();
  blink::WebFrame* frame = GetWebView()->mainFrame();
  std::string data_utf8;
  if (test_runner->shouldDumpAsCustomText()) {
    // Append a newline for the test driver.
    data_utf8 = test_interfaces_->GetTestRunner()->customDumpText() + "\n";
  } else {
    LayoutDumpFlags flags = test_runner->GetLayoutDumpFlags();
    data_utf8 = DumpDeepLayout(frame, flags, flags.dump_child_frames);
  }

  if (test_interfaces_->GetTestRunner()->ShouldDumpBackForwardList())
    data_utf8 += DumpBackForwardLists();

  return data_utf8;
}

std::string WebTestProxyBase::DumpBackForwardLists() {
  return DumpAllBackForwardLists(test_interfaces_, delegate_);
}

void WebTestProxyBase::DrawSelectionRect(SkCanvas* canvas) {
  // See if we need to draw the selection bounds rect. Selection bounds
  // rect is the rect enclosing the (possibly transformed) selection.
  // The rect should be drawn after everything is laid out and painted.
  if (!test_interfaces_->GetTestRunner()->shouldDumpSelectionRect())
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

void WebTestProxyBase::SetAcceptLanguages(const std::string& accept_languages) {
  bool notify = accept_languages_ != accept_languages;
  accept_languages_ = accept_languages;

  if (notify)
    GetWebView()->acceptLanguagesChanged();
}

void WebTestProxyBase::CopyImageAtAndCapturePixels(
    int x, int y, const base::Callback<void(const SkBitmap&)>& callback) {
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
  web_widget_->updateAllLifecyclePhases();

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
  const SkBitmap bitmap = skia::ReadPixels(canvas.get());
  callback.Run(bitmap);
}

CaptureCallback::CaptureCallback(
    const base::Callback<void(const SkBitmap&)>& callback)
    : callback_(callback), wait_for_popup_(false) {
}

CaptureCallback::~CaptureCallback() {
}

void CaptureCallback::didCompositeAndReadback(const SkBitmap& bitmap) {
  TRACE_EVENT2("shell",
               "CaptureCallback::didCompositeAndReadback",
               "x",
               bitmap.info().width(),
               "y",
               bitmap.info().height());
  if (!wait_for_popup_) {
    callback_.Run(bitmap);
    delete this;
    return;
  }
  if (main_bitmap_.isNull()) {
    bitmap.deepCopyTo(&main_bitmap_);
    return;
  }
  SkCanvas canvas(main_bitmap_);
  canvas.drawBitmap(bitmap, popup_position_.x(), popup_position_.y());
  callback_.Run(main_bitmap_);
  delete this;
}

void WebTestProxyBase::CapturePixelsAsync(
    const base::Callback<void(const SkBitmap&)>& callback) {
  TRACE_EVENT0("shell", "WebTestProxyBase::CapturePixelsAsync");
  DCHECK(!callback.is_null());

  if (test_interfaces_->GetTestRunner()->shouldDumpDragImage()) {
    if (drag_image_.isNull()) {
      // This means the test called dumpDragImage but did not initiate a drag.
      // Return a blank image so that the test fails.
      SkBitmap bitmap;
      bitmap.allocN32Pixels(1, 1);
      {
        SkAutoLockPixels lock(bitmap);
        bitmap.eraseColor(0);
      }
      callback.Run(bitmap);
      return;
    }

    callback.Run(drag_image_.getSkBitmap());
    return;
  }

  if (test_interfaces_->GetTestRunner()->isPrinting()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&WebTestProxyBase::CapturePixelsForPrinting,
                              base::Unretained(this), callback));
    return;
  }

  CaptureCallback* capture_callback = new CaptureCallback(base::Bind(
      &WebTestProxyBase::DidCapturePixelsAsync, base::Unretained(this),
      callback));
  web_widget_->compositeAndReadbackAsync(capture_callback);
  if (blink::WebPagePopup* popup = web_widget_->pagePopup()) {
    capture_callback->set_wait_for_popup(true);
    capture_callback->set_popup_position(
        delegate_->ConvertDIPToNative(popup->positionRelativeToOwner()));
    popup->compositeAndReadbackAsync(capture_callback);
  }
}

void WebTestProxyBase::DidCapturePixelsAsync(
    const base::Callback<void(const SkBitmap&)>& callback,
    const SkBitmap& bitmap) {
  SkCanvas canvas(bitmap);
  DrawSelectionRect(&canvas);
  if (!callback.is_null())
    callback.Run(bitmap);
}

void WebTestProxyBase::SetLogConsoleOutput(bool enabled) {
  log_console_output_ = enabled;
}

void LayoutAndPaintCallback::didLayoutAndPaint() {
  TRACE_EVENT0("shell", "LayoutAndPaintCallback::didLayoutAndPaint");
  if (wait_for_popup_) {
    wait_for_popup_ = false;
    return;
  }

  if (!callback_.is_null())
    callback_.Run();
  delete this;
}

void WebTestProxyBase::LayoutAndPaintAsyncThen(const base::Closure& callback) {
  TRACE_EVENT0("shell", "WebTestProxyBase::LayoutAndPaintAsyncThen");

  LayoutAndPaintCallback* layout_and_paint_callback =
      new LayoutAndPaintCallback(callback);
  web_widget_->layoutAndPaintAsync(layout_and_paint_callback);
  if (blink::WebPagePopup* popup = web_widget_->pagePopup()) {
    layout_and_paint_callback->set_wait_for_popup(true);
    popup->layoutAndPaintAsync(layout_and_paint_callback);
  }
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

MockWebSpeechRecognizer* WebTestProxyBase::GetSpeechRecognizerMock() {
  if (!speech_recognizer_.get()) {
    speech_recognizer_.reset(new MockWebSpeechRecognizer());
    speech_recognizer_->SetDelegate(delegate_);
  }
  return speech_recognizer_.get();
}

MockCredentialManagerClient*
WebTestProxyBase::GetCredentialManagerClientMock() {
  if (!credential_manager_client_.get())
    credential_manager_client_.reset(new MockCredentialManagerClient());
  return credential_manager_client_.get();
}

void WebTestProxyBase::ScheduleAnimation() {
  if (!test_interfaces_->GetTestRunner()->TestIsRunning())
    return;

  if (!animate_scheduled_) {
    animate_scheduled_ = true;
    delegate_->PostDelayedTask(
        new HostMethodTask(this, &WebTestProxyBase::AnimateNow), 1);
  }
}

void WebTestProxyBase::AnimateNow() {
  if (animate_scheduled_) {
    base::TimeDelta animate_time = base::TimeTicks::Now() - base::TimeTicks();
    animate_scheduled_ = false;
    web_widget_->beginFrame(animate_time.InSecondsF());
    web_widget_->updateAllLifecyclePhases();
    if (blink::WebPagePopup* popup = web_widget_->pagePopup()) {
      popup->beginFrame(animate_time.InSecondsF());
      popup->updateAllLifecyclePhases();
    }
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
  if (!test_interfaces_->GetTestRunner()->TestIsRunning())
    return;

  if (event == blink::WebAXEventFocus)
    test_interfaces_->GetAccessibilityController()->SetFocusedElement(obj);

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

  test_interfaces_->GetAccessibilityController()->NotificationReceived(
      obj, event_name);

  if (test_interfaces_->GetAccessibilityController()
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

    delegate_->PrintMessage(message + "\n");
  }
}

void WebTestProxyBase::StartDragging(blink::WebLocalFrame* frame,
                                     const blink::WebDragData& data,
                                     blink::WebDragOperationsMask mask,
                                     const blink::WebImage& image,
                                     const blink::WebPoint& point) {
  if (test_interfaces_->GetTestRunner()->shouldDumpDragImage()) {
    if (drag_image_.isNull())
      drag_image_ = image;
  }
  // When running a test, we need to fake a drag drop operation otherwise
  // Windows waits for real mouse events to know when the drag is over.
  test_interfaces_->GetEventSender()->DoDragDrop(data, mask);
}

// The output from these methods in layout test mode should match that
// expected by the layout tests. See EditingDelegate.m in DumpRenderTree.

void WebTestProxyBase::DidChangeSelection(bool is_empty_callback) {
  if (test_interfaces_->GetTestRunner()->shouldDumpEditingCallbacks())
    delegate_->PrintMessage(
        "EDITING DELEGATE: "
        "webViewDidChangeSelection:WebViewDidChangeSelectionNotification\n");
}

void WebTestProxyBase::DidChangeContents() {
  if (test_interfaces_->GetTestRunner()->shouldDumpEditingCallbacks())
    delegate_->PrintMessage(
        "EDITING DELEGATE: webViewDidChange:WebViewDidChangeNotification\n");
}

bool WebTestProxyBase::CreateView(blink::WebLocalFrame* frame,
                                  const blink::WebURLRequest& request,
                                  const blink::WebWindowFeatures& features,
                                  const blink::WebString& frame_name,
                                  blink::WebNavigationPolicy policy,
                                  bool suppress_opener) {
  if (test_interfaces_->GetTestRunner()->shouldDumpNavigationPolicy()) {
    delegate_->PrintMessage("Default policy for createView for '" +
                            URLDescription(request.url()) + "' is '" +
                            WebNavigationPolicyToString(policy) + "'\n");
  }

  if (!test_interfaces_->GetTestRunner()->canOpenWindows())
    return false;
  if (test_interfaces_->GetTestRunner()->shouldDumpCreateView())
    delegate_->PrintMessage(std::string("createView(") +
                            URLDescription(request.url()) + ")\n");
  return true;
}

blink::WebPlugin* WebTestProxyBase::CreatePlugin(
    blink::WebLocalFrame* frame,
    const blink::WebPluginParams& params) {
  if (TestPlugin::IsSupportedMimeType(params.mimeType))
    return TestPlugin::create(frame, params, delegate_);
  return delegate_->CreatePluginPlaceholder(frame, params);
}

void WebTestProxyBase::SetStatusText(const blink::WebString& text) {
  if (!test_interfaces_->GetTestRunner()->shouldDumpStatusCallbacks())
    return;
  delegate_->PrintMessage(
      std::string("UI DELEGATE STATUS CALLBACK: setStatusText:") +
      text.utf8().data() + "\n");
}

void WebTestProxyBase::ShowContextMenu(
    const blink::WebContextMenuData& context_menu_data) {
  test_interfaces_->GetEventSender()->SetContextMenuData(context_menu_data);
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

blink::WebSpeechRecognizer* WebTestProxyBase::GetSpeechRecognizer() {
  return GetSpeechRecognizerMock();
}

bool WebTestProxyBase::RequestPointerLock() {
  return test_interfaces_->GetTestRunner()->RequestPointerLock();
}

void WebTestProxyBase::RequestPointerUnlock() {
  test_interfaces_->GetTestRunner()->RequestPointerUnlock();
}

bool WebTestProxyBase::IsPointerLocked() {
  return test_interfaces_->GetTestRunner()->isPointerLocked();
}

void WebTestProxyBase::DidFocus() {
  delegate_->SetFocus(this, true);
}

void WebTestProxyBase::SetToolTipText(const blink::WebString& text,
                                      blink::WebTextDirection direction) {
  test_interfaces_->GetTestRunner()->setToolTipText(text);
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

void WebTestProxyBase::LoadURLExternally(const blink::WebURLRequest& request,
                                         blink::WebNavigationPolicy policy,
                                         const blink::WebString& suggested_name,
                                         bool replaces_current_history_item) {
  if (test_interfaces_->GetTestRunner()->shouldWaitUntilExternalURLLoad()) {
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

void WebTestProxyBase::DidStartProvisionalLoad(blink::WebLocalFrame* frame) {
  if (!test_interfaces_->GetTestRunner()->topLoadingFrame())
    test_interfaces_->GetTestRunner()->setTopLoadingFrame(frame, false);

  if (test_interfaces_->GetTestRunner()->shouldDumpFrameLoadCallbacks()) {
    PrintFrameDescription(delegate_, frame);
    delegate_->PrintMessage(" - didStartProvisionalLoadForFrame\n");
  }

  if (test_interfaces_->GetTestRunner()
          ->shouldDumpUserGestureInFrameLoadCallbacks()) {
    PrintFrameuserGestureStatus(
        delegate_, frame, " - in didStartProvisionalLoadForFrame\n");
  }
}

void WebTestProxyBase::DidReceiveServerRedirectForProvisionalLoad(
    blink::WebLocalFrame* frame) {
  if (test_interfaces_->GetTestRunner()->shouldDumpFrameLoadCallbacks()) {
    PrintFrameDescription(delegate_, frame);
    delegate_->PrintMessage(
        " - didReceiveServerRedirectForProvisionalLoadForFrame\n");
  }
}

bool WebTestProxyBase::DidFailProvisionalLoad(
    blink::WebLocalFrame* frame,
    const blink::WebURLError& error,
    blink::WebHistoryCommitType commit_type) {
  if (test_interfaces_->GetTestRunner()->shouldDumpFrameLoadCallbacks()) {
    PrintFrameDescription(delegate_, frame);
    delegate_->PrintMessage(" - didFailProvisionalLoadWithError\n");
  }
  CheckDone(frame, MainResourceLoadFailed);
  return !frame->provisionalDataSource();
}

void WebTestProxyBase::DidCommitProvisionalLoad(
    blink::WebLocalFrame* frame,
    const blink::WebHistoryItem& history_item,
    blink::WebHistoryCommitType history_type) {
  if (test_interfaces_->GetTestRunner()->shouldDumpFrameLoadCallbacks()) {
    PrintFrameDescription(delegate_, frame);
    delegate_->PrintMessage(" - didCommitLoadForFrame\n");
  }
}

void WebTestProxyBase::DidReceiveTitle(blink::WebLocalFrame* frame,
                                       const blink::WebString& title,
                                       blink::WebTextDirection direction) {
  blink::WebCString title8 = title.utf8();

  if (test_interfaces_->GetTestRunner()->shouldDumpFrameLoadCallbacks()) {
    PrintFrameDescription(delegate_, frame);
    delegate_->PrintMessage(std::string(" - didReceiveTitle: ") +
                            title8.data() + "\n");
  }

  if (test_interfaces_->GetTestRunner()->shouldDumpTitleChanges())
    delegate_->PrintMessage(std::string("TITLE CHANGED: '") + title8.data() +
                            "'\n");
}

void WebTestProxyBase::DidChangeIcon(blink::WebLocalFrame* frame,
                                     blink::WebIconURL::Type icon_type) {
  if (test_interfaces_->GetTestRunner()->shouldDumpIconChanges()) {
    PrintFrameDescription(delegate_, frame);
    delegate_->PrintMessage(std::string(" - didChangeIcons\n"));
  }
}

void WebTestProxyBase::DidFinishDocumentLoad(blink::WebLocalFrame* frame) {
  if (test_interfaces_->GetTestRunner()->shouldDumpFrameLoadCallbacks()) {
    PrintFrameDescription(delegate_, frame);
    delegate_->PrintMessage(" - didFinishDocumentLoadForFrame\n");
  }
}

void WebTestProxyBase::DidHandleOnloadEvents(blink::WebLocalFrame* frame) {
  if (test_interfaces_->GetTestRunner()->shouldDumpFrameLoadCallbacks()) {
    PrintFrameDescription(delegate_, frame);
    delegate_->PrintMessage(" - didHandleOnloadEventsForFrame\n");
  }
}

void WebTestProxyBase::DidFailLoad(blink::WebLocalFrame* frame,
                                   const blink::WebURLError& error,
                                   blink::WebHistoryCommitType commit_type) {
  if (test_interfaces_->GetTestRunner()->shouldDumpFrameLoadCallbacks()) {
    PrintFrameDescription(delegate_, frame);
    delegate_->PrintMessage(" - didFailLoadWithError\n");
  }
  CheckDone(frame, MainResourceLoadFailed);
}

void WebTestProxyBase::DidFinishLoad(blink::WebLocalFrame* frame) {
  if (test_interfaces_->GetTestRunner()->shouldDumpFrameLoadCallbacks()) {
    PrintFrameDescription(delegate_, frame);
    delegate_->PrintMessage(" - didFinishLoadForFrame\n");
  }
  CheckDone(frame, LoadFinished);
}

void WebTestProxyBase::DidDetectXSS(const blink::WebURL& insecure_url,
                                    bool did_block_entire_page) {
  if (test_interfaces_->GetTestRunner()->shouldDumpFrameLoadCallbacks())
    delegate_->PrintMessage("didDetectXSS\n");
}

void WebTestProxyBase::DidDispatchPingLoader(blink::WebLocalFrame* frame,
                                             const blink::WebURL& url) {
  if (test_interfaces_->GetTestRunner()->shouldDumpPingLoaderCallbacks())
    delegate_->PrintMessage(std::string("PingLoader dispatched to '") +
                            URLDescription(url).c_str() + "'.\n");
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
      (test_interfaces_->GetTestRunner()->shouldDumpResourceLoadCallbacks() ||
       test_interfaces_->GetTestRunner()->shouldDumpResourcePriorities())) {
    DCHECK(resource_identifier_map_.find(identifier) ==
           resource_identifier_map_.end());
    resource_identifier_map_[identifier] =
        DescriptionSuitableForTestResult(request_url);
  }

  if (test_interfaces_->GetTestRunner()->shouldDumpResourceLoadCallbacks()) {
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

  if (test_interfaces_->GetTestRunner()->shouldDumpResourcePriorities()) {
    delegate_->PrintMessage(
        DescriptionSuitableForTestResult(request_url).c_str());
    delegate_->PrintMessage(" has priority ");
    delegate_->PrintMessage(PriorityDescription(request.priority()));
    delegate_->PrintMessage("\n");
  }

  if (test_interfaces_->GetTestRunner()->httpHeadersToClear()) {
    const std::set<std::string>* clearHeaders =
        test_interfaces_->GetTestRunner()->httpHeadersToClear();
    for (std::set<std::string>::const_iterator header = clearHeaders->begin();
         header != clearHeaders->end();
         ++header)
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

void WebTestProxyBase::DidReceiveResponse(
    blink::WebLocalFrame* frame,
    unsigned identifier,
    const blink::WebURLResponse& response) {
  if (test_interfaces_->GetTestRunner()->shouldDumpResourceLoadCallbacks()) {
    if (resource_identifier_map_.find(identifier) ==
        resource_identifier_map_.end())
      delegate_->PrintMessage("<unknown>");
    else
      delegate_->PrintMessage(resource_identifier_map_[identifier]);
    delegate_->PrintMessage(" - didReceiveResponse ");
    PrintResponseDescription(delegate_, response);
    delegate_->PrintMessage("\n");
  }
  if (test_interfaces_->GetTestRunner()
          ->shouldDumpResourceResponseMIMETypes()) {
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

void WebTestProxyBase::DidChangeResourcePriority(
    blink::WebLocalFrame* frame,
    unsigned identifier,
    const blink::WebURLRequest::Priority& priority,
    int intra_priority_value) {
  if (test_interfaces_->GetTestRunner()->shouldDumpResourcePriorities()) {
    if (resource_identifier_map_.find(identifier) ==
        resource_identifier_map_.end())
      delegate_->PrintMessage("<unknown>");
    else
      delegate_->PrintMessage(resource_identifier_map_[identifier]);
    delegate_->PrintMessage(
        base::StringPrintf(" changed priority to %s, intra_priority %d\n",
                           PriorityDescription(priority).c_str(),
                           intra_priority_value));
  }
}

void WebTestProxyBase::DidFinishResourceLoad(blink::WebLocalFrame* frame,
                                             unsigned identifier) {
  if (test_interfaces_->GetTestRunner()->shouldDumpResourceLoadCallbacks()) {
    if (resource_identifier_map_.find(identifier) ==
        resource_identifier_map_.end())
      delegate_->PrintMessage("<unknown>");
    else
      delegate_->PrintMessage(resource_identifier_map_[identifier]);
    delegate_->PrintMessage(" - didFinishLoading\n");
  }
  resource_identifier_map_.erase(identifier);
  CheckDone(frame, ResourceLoadCompleted);
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

void WebTestProxyBase::CheckDone(blink::WebLocalFrame* frame,
                                 CheckDoneReason reason) {
  if (frame != test_interfaces_->GetTestRunner()->topLoadingFrame())
    return;
  if (reason != MainResourceLoadFailed &&
      (frame->isResourceLoadInProgress() || frame->isLoading()))
    return;
  test_interfaces_->GetTestRunner()->setTopLoadingFrame(frame, true);
}

blink::WebNavigationPolicy WebTestProxyBase::DecidePolicyForNavigation(
    const blink::WebFrameClient::NavigationPolicyInfo& info) {
  if (test_interfaces_->GetTestRunner()->shouldDumpNavigationPolicy()) {
    delegate_->PrintMessage("Default policy for navigation to '" +
                            URLDescription(info.urlRequest.url()) + "' is '" +
                            WebNavigationPolicyToString(info.defaultPolicy) +
                            "'\n");
  }

  blink::WebNavigationPolicy result;
  if (!test_interfaces_->GetTestRunner()->policyDelegateEnabled())
    return info.defaultPolicy;

  delegate_->PrintMessage(
      std::string("Policy delegate: attempt to load ") +
      URLDescription(info.urlRequest.url()) + " with navigation type '" +
      WebNavigationTypeToString(info.navigationType) + "'\n");
  if (test_interfaces_->GetTestRunner()->policyDelegateIsPermissive())
    result = blink::WebNavigationPolicyCurrentTab;
  else
    result = blink::WebNavigationPolicyIgnore;

  if (test_interfaces_->GetTestRunner()->policyDelegateShouldNotifyDone()) {
    test_interfaces_->GetTestRunner()->policyDelegateDone();
    result = blink::WebNavigationPolicyIgnore;
  }

  return result;
}

bool WebTestProxyBase::WillCheckAndDispatchMessageEvent(
    blink::WebLocalFrame* source_frame,
    blink::WebFrame* target_frame,
    blink::WebSecurityOrigin target,
    blink::WebDOMMessageEvent event) {
  if (test_interfaces_->GetTestRunner()->shouldInterceptPostMessage()) {
    delegate_->PrintMessage("intercepted postMessage\n");
    return true;
  }

  return false;
}

void WebTestProxyBase::PostSpellCheckEvent(const blink::WebString& event_name) {
  if (test_interfaces_->GetTestRunner()->shouldDumpSpellCheckCallbacks()) {
    delegate_->PrintMessage(std::string("SpellCheckEvent: ") +
                            event_name.utf8().data() + "\n");
  }
}

void WebTestProxyBase::ResetInputMethod() {
  // If a composition text exists, then we need to let the browser process
  // to cancel the input method's ongoing composition session.
  if (web_widget_)
    web_widget_->confirmComposition();
}

void WebTestProxyBase::CheckIfAudioSinkExistsAndIsAuthorized(
    const blink::WebString& sink_id,
    const blink::WebSecurityOrigin& security_origin,
    blink::WebSetSinkIdCallbacks* web_callbacks) {
  scoped_ptr<blink::WebSetSinkIdCallbacks> callback(web_callbacks);
  std::string device_id = sink_id.utf8();
  if (device_id == "valid" || device_id.empty())
    callback->onSuccess();
  else if (device_id == "unauthorized")
    callback->onError(blink::WebSetSinkIdError::NotAuthorized);
  else
    callback->onError(blink::WebSetSinkIdError::NotFound);
}

blink::WebString WebTestProxyBase::acceptLanguages() {
  return blink::WebString::fromUTF8(accept_languages_);
}

}  // namespace test_runner
