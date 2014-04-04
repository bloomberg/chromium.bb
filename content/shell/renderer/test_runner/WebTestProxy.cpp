// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/WebTestProxy.h"

#include <cctype>

#include "base/logging.h"
#include "content/shell/renderer/test_runner/event_sender.h"
#include "content/shell/renderer/test_runner/MockColorChooser.h"
#include "content/shell/renderer/test_runner/MockWebSpeechInputController.h"
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
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebHistoryItem.h"
#include "third_party/WebKit/public/web/WebMIDIClientMock.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "third_party/WebKit/public/web/WebPrintParams.h"
#include "third_party/WebKit/public/web/WebRange.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "third_party/WebKit/public/web/WebView.h"

using namespace blink;
using namespace std;

namespace WebTestRunner {

namespace {

class HostMethodTask : public WebMethodTask<WebTestProxyBase> {
public:
    typedef void (WebTestProxyBase::*CallbackMethodType)();
    HostMethodTask(WebTestProxyBase* object, CallbackMethodType callback)
        : WebMethodTask<WebTestProxyBase>(object)
        , m_callback(callback)
    { }

    virtual void runIfValid() OVERRIDE { (m_object->*m_callback)(); }

private:
    CallbackMethodType m_callback;
};

void printFrameDescription(WebTestDelegate* delegate, WebFrame* frame)
{
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

void printFrameUserGestureStatus(WebTestDelegate* delegate, WebFrame* frame, const char* msg)
{
    bool isUserGesture = WebUserGestureIndicator::isProcessingUserGesture();
    delegate->printMessage(string("Frame with user gesture \"") + (isUserGesture ? "true" : "false") + "\"" + msg);
}

// Used to write a platform neutral file:/// URL by taking the
// filename and its directory. (e.g., converts
// "file:///tmp/foo/bar.txt" to just "bar.txt").
string descriptionSuitableForTestResult(const string& url)
{
    if (url.empty() || string::npos == url.find("file://"))
        return url;

    size_t pos = url.rfind('/');
    if (pos == string::npos || !pos)
        return "ERROR:" + url;
    pos = url.rfind('/', pos - 1);
    if (pos == string::npos)
        return "ERROR:" + url;

    return url.substr(pos + 1);
}

void printResponseDescription(WebTestDelegate* delegate, const WebURLResponse& response)
{
    if (response.isNull()) {
        delegate->printMessage("(null)");
        return;
    }
    string url = response.url().spec();
    char data[100];
    snprintf(data, sizeof(data), "%d", response. httpStatusCode());
    delegate->printMessage(string("<NSURLResponse ") + descriptionSuitableForTestResult(url) + ", http status code " + data + ">");
}

string URLDescription(const GURL& url)
{
    if (url.SchemeIs("file"))
        return url.ExtractFileName();
    return url.possibly_invalid_spec();
}

string PriorityDescription(const WebURLRequest::Priority& priority)
{
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

void blockRequest(WebURLRequest& request)
{
    request.setURL(GURL("255.255.255.255"));
}

bool isLocalhost(const string& host)
{
    return host == "127.0.0.1" || host == "localhost";
}

bool hostIsUsedBySomeTestsToGenerateError(const string& host)
{
    return host == "255.255.255.255";
}

// Used to write a platform neutral file:/// URL by only taking the filename
// (e.g., converts "file:///tmp/foo.txt" to just "foo.txt").
string urlSuitableForTestResult(const string& url)
{
    if (url.empty() || string::npos == url.find("file://"))
        return url;

    size_t pos = url.rfind('/');
    if (pos == string::npos) {
#ifdef WIN32
        pos = url.rfind('\\');
        if (pos == string::npos)
            pos = 0;
#else
        pos = 0;
#endif
    }
    string filename = url.substr(pos + 1);
    if (filename.empty())
        return "file:"; // A WebKit test has this in its expected output.
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
const char* webNavigationTypeToString(WebNavigationType type)
{
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

string dumpDocumentText(WebFrame* frame)
{
    // We use the document element's text instead of the body text here because
    // not all documents have a body, such as XML documents.
    WebElement documentElement = frame->document().documentElement();
    if (documentElement.isNull())
        return string();
    return documentElement.innerText().utf8();
}

string dumpFramesAsText(WebFrame* frame, bool recursive)
{
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
        for (WebFrame* child = frame->firstChild(); child; child = child->nextSibling())
            result.append(dumpFramesAsText(child, recursive));
    }

    return result;
}

string dumpFramesAsPrintedText(WebFrame* frame, bool recursive)
{
    string result;

    // Cannot do printed format for anything other than HTML
    if (!frame->document().isHTMLDocument())
        return string();

    // Add header for all but the main frame. Skip empty frames.
    if (frame->parent() && !frame->document().documentElement().isNull()) {
        result.append("\n--------\nFrame: '");
        result.append(frame->uniqueName().utf8().data());
        result.append("'\n--------\n");
    }

    result.append(frame->renderTreeAsText(WebFrame::RenderAsTextPrinting).utf8());
    result.append("\n");

    if (recursive) {
        for (WebFrame* child = frame->firstChild(); child; child = child->nextSibling())
            result.append(dumpFramesAsPrintedText(child, recursive));
    }

    return result;
}

string dumpFrameScrollPosition(WebFrame* frame, bool recursive)
{
    string result;
    WebSize offset = frame->scrollOffset();
    if (offset.width > 0 || offset.height > 0) {
        if (frame->parent())
            result = string("frame '") + frame->uniqueName().utf8().data() + "' ";
        char data[100];
        snprintf(data, sizeof(data), "scrolled to %d,%d\n", offset.width, offset.height);
        result += data;
    }

    if (!recursive)
        return result;
    for (WebFrame* child = frame->firstChild(); child; child = child->nextSibling())
        result += dumpFrameScrollPosition(child, recursive);
    return result;
}

struct ToLower {
    base::char16 operator()(base::char16 c) { return tolower(c); }
};

// Returns True if item1 < item2.
bool HistoryItemCompareLess(const WebHistoryItem& item1, const WebHistoryItem& item2)
{
    base::string16 target1 = item1.target();
    base::string16 target2 = item2.target();
    std::transform(target1.begin(), target1.end(), target1.begin(), ToLower());
    std::transform(target2.begin(), target2.end(), target2.begin(), ToLower());
    return target1 < target2;
}

string dumpHistoryItem(const WebHistoryItem& item, int indent, bool isCurrent)
{
    string result;

    if (isCurrent) {
        result.append("curr->");
        result.append(indent - 6, ' '); // 6 == "curr->".length()
    } else
        result.append(indent, ' ');

    string url = normalizeLayoutTestURL(item.urlString().utf8());
    result.append(url);
    if (!item.target().isEmpty()) {
        result.append(" (in frame \"");
        result.append(item.target().utf8());
        result.append("\")");
    }
    result.append("\n");

    const WebVector<WebHistoryItem>& children = item.children();
    if (!children.isEmpty()) {
        // Must sort to eliminate arbitrary result ordering which defeats
        // reproducible testing.
        // FIXME: WebVector should probably just be a std::vector!!
        std::vector<WebHistoryItem> sortedChildren;
        for (size_t i = 0; i < children.size(); ++i)
            sortedChildren.push_back(children[i]);
        std::sort(sortedChildren.begin(), sortedChildren.end(), HistoryItemCompareLess);
        for (size_t i = 0; i < sortedChildren.size(); ++i)
            result += dumpHistoryItem(sortedChildren[i], indent + 4, false);
    }

    return result;
}

void dumpBackForwardList(const WebVector<WebHistoryItem>& history, size_t currentEntryIndex, string& result)
{
    result.append("\n============== Back Forward List ==============\n");
    for (size_t index = 0; index < history.size(); ++index)
        result.append(dumpHistoryItem(history[index], 8, index == currentEntryIndex));
    result.append("===============================================\n");
}

string dumpAllBackForwardLists(TestInterfaces* interfaces, WebTestDelegate* delegate)
{
    string result;
    const vector<WebTestProxyBase*>& windowList = interfaces->windowList();
    for (unsigned i = 0; i < windowList.size(); ++i) {
        size_t currentEntryIndex = 0;
        WebVector<WebHistoryItem> history;
        delegate->captureHistoryForWindow(windowList.at(i), &history, &currentEntryIndex);
        dumpBackForwardList(history, currentEntryIndex, result);
    }
    return result;
}

}

WebTestProxyBase::WebTestProxyBase()
    : m_testInterfaces(0)
    , m_delegate(0)
    , m_webWidget(0)
    , m_spellcheck(new SpellCheckClient(this))
    , m_chooserCount(0)
{
    reset();
}

WebTestProxyBase::~WebTestProxyBase()
{
    m_testInterfaces->windowClosed(this);
}

void WebTestProxyBase::setInterfaces(WebTestInterfaces* interfaces)
{
    m_testInterfaces = interfaces->testInterfaces();
    m_testInterfaces->windowOpened(this);
}

void WebTestProxyBase::setDelegate(WebTestDelegate* delegate)
{
    m_delegate = delegate;
    m_spellcheck->setDelegate(delegate);
#if ENABLE_INPUT_SPEECH
    if (m_speechInputController.get())
        m_speechInputController->setDelegate(delegate);
#endif
    if (m_speechRecognizer.get())
        m_speechRecognizer->setDelegate(delegate);
}

void WebTestProxyBase::setWidget(WebWidget* widget)
{
    m_webWidget = widget;
}

WebWidget* WebTestProxyBase::webWidget()
{
    return m_webWidget;
}

WebView* WebTestProxyBase::webView()
{
    DCHECK(m_webWidget);
    // TestRunner does not support popup widgets. So m_webWidget is always a WebView.
    return static_cast<WebView*>(m_webWidget);
}

void WebTestProxyBase::didForceResize()
{
    invalidateAll();
    discardBackingStore();
}

void WebTestProxyBase::reset()
{
    m_paintRect = WebRect();
    m_canvas.reset();
    m_isPainting = false;
    m_animateScheduled = false;
    m_resourceIdentifierMap.clear();
    m_logConsoleOutput = true;
    if (m_midiClient.get())
        m_midiClient->resetMock();
#if ENABLE_INPUT_SPEECH
    if (m_speechInputController.get())
        m_speechInputController->clearResults();
#endif
}

WebSpellCheckClient* WebTestProxyBase::spellCheckClient() const
{
    return m_spellcheck.get();
}

WebColorChooser* WebTestProxyBase::createColorChooser(WebColorChooserClient* client, const blink::WebColor& color)
{
    // This instance is deleted by WebCore::ColorInputType
    return new MockColorChooser(client, m_delegate, this);
}

WebColorChooser* WebTestProxyBase::createColorChooser(WebColorChooserClient* client, const blink::WebColor& color, const blink::WebVector<blink::WebColorSuggestion>& suggestions)
{
    // This instance is deleted by WebCore::ColorInputType
    return new MockColorChooser(client, m_delegate, this);
}

bool WebTestProxyBase::runFileChooser(const blink::WebFileChooserParams&, blink::WebFileChooserCompletion*)
{
    m_delegate->printMessage("Mock: Opening a file chooser.\n");
    // FIXME: Add ability to set file names to a file upload control.
    return false;
}

void WebTestProxyBase::showValidationMessage(const WebRect&, const WebString& message, const WebString& subMessage, WebTextDirection)
{
    m_delegate->printMessage(std::string("ValidationMessageClient: main-message=") + std::string(message.utf8()) + " sub-message=" + std::string(subMessage.utf8()) + "\n");
}

void WebTestProxyBase::hideValidationMessage()
{
}

void WebTestProxyBase::moveValidationMessage(const WebRect&)
{
}

string WebTestProxyBase::captureTree(bool debugRenderTree)
{
    bool shouldDumpAsText = m_testInterfaces->testRunner()->shouldDumpAsText();
    bool shouldDumpAsMarkup = m_testInterfaces->testRunner()->shouldDumpAsMarkup();
    bool shouldDumpAsPrinted = m_testInterfaces->testRunner()->isPrinting();
    WebFrame* frame = webView()->mainFrame();
    string dataUtf8;
    if (shouldDumpAsText) {
        bool recursive = m_testInterfaces->testRunner()->shouldDumpChildFramesAsText();
        dataUtf8 = shouldDumpAsPrinted ? dumpFramesAsPrintedText(frame, recursive) : dumpFramesAsText(frame, recursive);
    } else if (shouldDumpAsMarkup) {
        // Append a newline for the test driver.
        dataUtf8 = frame->contentAsMarkup().utf8() + "\n";
    } else {
        bool recursive = m_testInterfaces->testRunner()->shouldDumpChildFrameScrollPositions();
        WebFrame::RenderAsTextControls renderTextBehavior = WebFrame::RenderAsTextNormal;
        if (shouldDumpAsPrinted)
            renderTextBehavior |= WebFrame::RenderAsTextPrinting;
        if (debugRenderTree)
            renderTextBehavior |= WebFrame::RenderAsTextDebug;
        dataUtf8 = frame->renderTreeAsText(renderTextBehavior).utf8();
        dataUtf8 += dumpFrameScrollPosition(frame, recursive);
    }

    if (m_testInterfaces->testRunner()->shouldDumpBackForwardList())
        dataUtf8 += dumpAllBackForwardLists(m_testInterfaces, m_delegate);

    return dataUtf8;
}

SkCanvas* WebTestProxyBase::capturePixels()
{
    webWidget()->layout();
    if (m_testInterfaces->testRunner()->testRepaint()) {
        WebSize viewSize = webWidget()->size();
        int width = viewSize.width;
        int height = viewSize.height;
        if (m_testInterfaces->testRunner()->sweepHorizontally()) {
            for (WebRect column(0, 0, 1, height); column.x < width; column.x++)
                paintRect(column);
        } else {
            for (WebRect line(0, 0, width, 1); line.y < height; line.y++)
                paintRect(line);
        }
    } else if (m_testInterfaces->testRunner()->isPrinting())
        paintPagesWithBoundaries();
    else
        paintInvalidatedRegion();

    // See if we need to draw the selection bounds rect. Selection bounds
    // rect is the rect enclosing the (possibly transformed) selection.
    // The rect should be drawn after everything is laid out and painted.
    if (m_testInterfaces->testRunner()->shouldDumpSelectionRect()) {
        // If there is a selection rect - draw a red 1px border enclosing rect
        WebRect wr = webView()->mainFrame()->selectionBoundsRect();
        if (!wr.isEmpty()) {
            // Render a red rectangle bounding selection rect
            SkPaint paint;
            paint.setColor(0xFFFF0000); // Fully opaque red
            paint.setStyle(SkPaint::kStroke_Style);
            paint.setFlags(SkPaint::kAntiAlias_Flag);
            paint.setStrokeWidth(1.0f);
            SkIRect rect; // Bounding rect
            rect.set(wr.x, wr.y, wr.x + wr.width, wr.y + wr.height);
            canvas()->drawIRect(rect, paint);
        }
    }

    return canvas();
}

void WebTestProxyBase::setLogConsoleOutput(bool enabled)
{
    m_logConsoleOutput = enabled;
}

void WebTestProxyBase::paintRect(const WebRect& rect)
{
    DCHECK(!m_isPainting);
    DCHECK(canvas());
    m_isPainting = true;
    float deviceScaleFactor = webView()->deviceScaleFactor();
    int scaledX = static_cast<int>(static_cast<float>(rect.x) * deviceScaleFactor);
    int scaledY = static_cast<int>(static_cast<float>(rect.y) * deviceScaleFactor);
    int scaledWidth = static_cast<int>(ceil(static_cast<float>(rect.width) * deviceScaleFactor));
    int scaledHeight = static_cast<int>(ceil(static_cast<float>(rect.height) * deviceScaleFactor));
    WebRect deviceRect(scaledX, scaledY, scaledWidth, scaledHeight);
    webWidget()->paint(canvas(), deviceRect);
    m_isPainting = false;
}

void WebTestProxyBase::paintInvalidatedRegion()
{
    webWidget()->animate(0.0);
    webWidget()->layout();
    WebSize widgetSize = webWidget()->size();
    WebRect clientRect(0, 0, widgetSize.width, widgetSize.height);

    // Paint the canvas if necessary. Allow painting to generate extra rects
    // for the first two calls. This is necessary because some WebCore rendering
    // objects update their layout only when painted.
    // Store the total area painted in total_paint. Then tell the gdk window
    // to update that area after we're done painting it.
    for (int i = 0; i < 3; ++i) {
        // rect = intersect(m_paintRect , clientRect)
        WebRect damageRect = m_paintRect;
        int left = max(damageRect.x, clientRect.x);
        int top = max(damageRect.y, clientRect.y);
        int right = min(damageRect.x + damageRect.width, clientRect.x + clientRect.width);
        int bottom = min(damageRect.y + damageRect.height, clientRect.y + clientRect.height);
        WebRect rect;
        if (left < right && top < bottom)
            rect = WebRect(left, top, right - left, bottom - top);

        m_paintRect = WebRect();
        if (rect.isEmpty())
            continue;
        paintRect(rect);
    }
    DCHECK(m_paintRect.isEmpty());
}

void WebTestProxyBase::paintPagesWithBoundaries()
{
    DCHECK(!m_isPainting);
    DCHECK(canvas());
    m_isPainting = true;

    WebSize pageSizeInPixels = webWidget()->size();
    WebFrame* webFrame = webView()->mainFrame();

    int pageCount = webFrame->printBegin(pageSizeInPixels);
    int totalHeight = pageCount * (pageSizeInPixels.height + 1) - 1;

    SkCanvas* testCanvas = skia::TryCreateBitmapCanvas(pageSizeInPixels.width, totalHeight, false);
    if (testCanvas) {
        discardBackingStore();
        m_canvas.reset(testCanvas);
    } else {
        webFrame->printEnd();
        return;
    }

    webFrame->printPagesWithBoundaries(canvas(), pageSizeInPixels);
    webFrame->printEnd();

    m_isPainting = false;
}

SkCanvas* WebTestProxyBase::canvas()
{
    if (m_canvas.get())
        return m_canvas.get();
    WebSize widgetSize = webWidget()->size();
    float deviceScaleFactor = webView()->deviceScaleFactor();
    int scaledWidth = static_cast<int>(ceil(static_cast<float>(widgetSize.width) * deviceScaleFactor));
    int scaledHeight = static_cast<int>(ceil(static_cast<float>(widgetSize.height) * deviceScaleFactor));
    // We're allocating the canvas to be non-opaque (third parameter), so we
    // don't end up with uninitialized memory if a layout test doesn't damage
    // the entire view.
    m_canvas.reset(skia::CreateBitmapCanvas(scaledWidth, scaledHeight, false));
    return m_canvas.get();
}

void WebTestProxyBase::display()
{
    const blink::WebSize& size = webWidget()->size();
    WebRect rect(0, 0, size.width, size.height);
    m_paintRect = rect;
    paintInvalidatedRegion();
}

void WebTestProxyBase::discardBackingStore()
{
    m_canvas.reset();
}

WebMIDIClientMock* WebTestProxyBase::midiClientMock()
{
    if (!m_midiClient.get())
        m_midiClient.reset(new WebMIDIClientMock);
    return m_midiClient.get();
}

#if ENABLE_INPUT_SPEECH
MockWebSpeechInputController* WebTestProxyBase::speechInputControllerMock()
{
    DCHECK(m_speechInputController.get());
    return m_speechInputController.get();
}
#endif

MockWebSpeechRecognizer* WebTestProxyBase::speechRecognizerMock()
{
    if (!m_speechRecognizer.get()) {
        m_speechRecognizer.reset(new MockWebSpeechRecognizer());
        m_speechRecognizer->setDelegate(m_delegate);
    }
    return m_speechRecognizer.get();
}

void WebTestProxyBase::didInvalidateRect(const WebRect& rect)
{
    // m_paintRect = m_paintRect U rect
    if (rect.isEmpty())
        return;
    if (m_paintRect.isEmpty()) {
        m_paintRect = rect;
        return;
    }
    int left = min(m_paintRect.x, rect.x);
    int top = min(m_paintRect.y, rect.y);
    int right = max(m_paintRect.x + m_paintRect.width, rect.x + rect.width);
    int bottom = max(m_paintRect.y + m_paintRect.height, rect.y + rect.height);
    m_paintRect = WebRect(left, top, right - left, bottom - top);
}

void WebTestProxyBase::didScrollRect(int, int, const WebRect& clipRect)
{
    didInvalidateRect(clipRect);
}

void WebTestProxyBase::invalidateAll()
{
    m_paintRect = WebRect(0, 0, INT_MAX, INT_MAX);
}

void WebTestProxyBase::scheduleComposite()
{
    invalidateAll();
}

void WebTestProxyBase::scheduleAnimation()
{
    if (!m_testInterfaces->testRunner()->TestIsRunning())
        return;

    if (!m_animateScheduled) {
        m_animateScheduled = true;
        m_delegate->postDelayedTask(new HostMethodTask(this, &WebTestProxyBase::animateNow), 1);
    }
}

void WebTestProxyBase::animateNow()
{
    if (m_animateScheduled) {
        m_animateScheduled = false;
        webWidget()->animate(0.0);
        webWidget()->layout();
    }
}

bool WebTestProxyBase::isCompositorFramePending() const
{
    return m_animateScheduled || !m_paintRect.isEmpty();
}

void WebTestProxyBase::show(WebNavigationPolicy)
{
    invalidateAll();
}

void WebTestProxyBase::setWindowRect(const WebRect& rect)
{
    invalidateAll();
    discardBackingStore();
}

void WebTestProxyBase::didAutoResize(const WebSize&)
{
    invalidateAll();
}

void WebTestProxyBase::postAccessibilityEvent(const blink::WebAXObject& obj, blink::WebAXEvent event)
{
    if (event == blink::WebAXEventFocus)
        m_testInterfaces->accessibilityController()->SetFocusedElement(obj);

    const char* eventName = 0;
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

    m_testInterfaces->accessibilityController()->NotificationReceived(obj, eventName);

    if (m_testInterfaces->accessibilityController()->ShouldLogAccessibilityEvents()) {
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

        m_delegate->printMessage(message + "\n");
    }
}

void WebTestProxyBase::startDragging(WebLocalFrame*, const WebDragData& data, WebDragOperationsMask mask, const WebImage&, const WebPoint&)
{
    // When running a test, we need to fake a drag drop operation otherwise
    // Windows waits for real mouse events to know when the drag is over.
    m_testInterfaces->eventSender()->DoDragDrop(data, mask);
}

// The output from these methods in layout test mode should match that
// expected by the layout tests. See EditingDelegate.m in DumpRenderTree.

void WebTestProxyBase::didChangeSelection(bool isEmptySelection)
{
    if (m_testInterfaces->testRunner()->shouldDumpEditingCallbacks())
        m_delegate->printMessage("EDITING DELEGATE: webViewDidChangeSelection:WebViewDidChangeSelectionNotification\n");
}

void WebTestProxyBase::didChangeContents()
{
    if (m_testInterfaces->testRunner()->shouldDumpEditingCallbacks())
        m_delegate->printMessage("EDITING DELEGATE: webViewDidChange:WebViewDidChangeNotification\n");
}

bool WebTestProxyBase::createView(WebLocalFrame*, const WebURLRequest& request, const WebWindowFeatures&, const WebString&, WebNavigationPolicy, bool)
{
    if (!m_testInterfaces->testRunner()->canOpenWindows())
        return false;
    if (m_testInterfaces->testRunner()->shouldDumpCreateView())
        m_delegate->printMessage(string("createView(") + URLDescription(request.url()) + ")\n");
    return true;
}

WebPlugin* WebTestProxyBase::createPlugin(WebLocalFrame* frame, const WebPluginParams& params)
{
    if (TestPlugin::isSupportedMimeType(params.mimeType))
        return TestPlugin::create(frame, params, m_delegate);
    return 0;
}

void WebTestProxyBase::setStatusText(const WebString& text)
{
    if (!m_testInterfaces->testRunner()->shouldDumpStatusCallbacks())
        return;
    m_delegate->printMessage(string("UI DELEGATE STATUS CALLBACK: setStatusText:") + text.utf8().data() + "\n");
}

void WebTestProxyBase::didStopLoading()
{
    if (m_testInterfaces->testRunner()->shouldDumpProgressFinishedCallback())
        m_delegate->printMessage("postProgressFinishedNotification\n");
}

void WebTestProxyBase::showContextMenu(WebLocalFrame*, const WebContextMenuData& contextMenuData)
{
    m_testInterfaces->eventSender()->SetContextMenuData(contextMenuData);
}

WebUserMediaClient* WebTestProxyBase::userMediaClient()
{
    if (!m_userMediaClient.get())
        m_userMediaClient.reset(new WebUserMediaClientMock(m_delegate));
    return m_userMediaClient.get();
}

// Simulate a print by going into print mode and then exit straight away.
void WebTestProxyBase::printPage(WebLocalFrame* frame)
{
    WebSize pageSizeInPixels = webWidget()->size();
    if (pageSizeInPixels.isEmpty())
        return;
    WebPrintParams printParams(pageSizeInPixels);
    frame->printBegin(printParams);
    frame->printEnd();
}

WebNotificationPresenter* WebTestProxyBase::notificationPresenter()
{
    return m_testInterfaces->testRunner()->notification_presenter();
}

WebMIDIClient* WebTestProxyBase::webMIDIClient()
{
    return midiClientMock();
}

WebSpeechInputController* WebTestProxyBase::speechInputController(WebSpeechInputListener* listener)
{
#if ENABLE_INPUT_SPEECH
    if (!m_speechInputController.get()) {
        m_speechInputController.reset(new MockWebSpeechInputController(listener));
        m_speechInputController->setDelegate(m_delegate);
    }
    return m_speechInputController.get();
#else
    DCHECK(listener);
    return 0;
#endif
}

WebSpeechRecognizer* WebTestProxyBase::speechRecognizer()
{
    return speechRecognizerMock();
}

bool WebTestProxyBase::requestPointerLock()
{
    return m_testInterfaces->testRunner()->RequestPointerLock();
}

void WebTestProxyBase::requestPointerUnlock()
{
    m_testInterfaces->testRunner()->RequestPointerUnlock();
}

bool WebTestProxyBase::isPointerLocked()
{
    return m_testInterfaces->testRunner()->isPointerLocked();
}

void WebTestProxyBase::didFocus()
{
    m_delegate->setFocus(this, true);
}

void WebTestProxyBase::didBlur()
{
    m_delegate->setFocus(this, false);
}

void WebTestProxyBase::setToolTipText(const WebString& text, WebTextDirection)
{
    m_testInterfaces->testRunner()->setToolTipText(text);
}

void WebTestProxyBase::didOpenChooser()
{
    m_chooserCount++;
}

void WebTestProxyBase::didCloseChooser()
{
    m_chooserCount--;
}

bool WebTestProxyBase::isChooserShown()
{
    return 0 < m_chooserCount;
}

void WebTestProxyBase::didStartProvisionalLoad(WebLocalFrame* frame)
{
    if (!m_testInterfaces->testRunner()->topLoadingFrame())
        m_testInterfaces->testRunner()->setTopLoadingFrame(frame, false);

    if (m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(" - didStartProvisionalLoadForFrame\n");
    }

    if (m_testInterfaces->testRunner()->shouldDumpUserGestureInFrameLoadCallbacks())
        printFrameUserGestureStatus(m_delegate, frame, " - in didStartProvisionalLoadForFrame\n");
}

void WebTestProxyBase::didReceiveServerRedirectForProvisionalLoad(WebLocalFrame* frame)
{
    if (m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(" - didReceiveServerRedirectForProvisionalLoadForFrame\n");
    }
}

bool WebTestProxyBase::didFailProvisionalLoad(WebLocalFrame* frame, const WebURLError&)
{
    if (m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(" - didFailProvisionalLoadWithError\n");
    }
    locationChangeDone(frame);
    return !frame->provisionalDataSource();
}

void WebTestProxyBase::didCommitProvisionalLoad(WebLocalFrame* frame, bool)
{
    if (m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(" - didCommitLoadForFrame\n");
    }
}

void WebTestProxyBase::didReceiveTitle(WebLocalFrame* frame, const WebString& title, WebTextDirection direction)
{
    WebCString title8 = title.utf8();

    if (m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(string(" - didReceiveTitle: ") + title8.data() + "\n");
    }

    if (m_testInterfaces->testRunner()->shouldDumpTitleChanges())
        m_delegate->printMessage(string("TITLE CHANGED: '") + title8.data() + "'\n");
}

void WebTestProxyBase::didChangeIcon(WebLocalFrame* frame, WebIconURL::Type)
{
    if (m_testInterfaces->testRunner()->shouldDumpIconChanges()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(string(" - didChangeIcons\n"));
    }
}

void WebTestProxyBase::didFinishDocumentLoad(WebLocalFrame* frame)
{
    if (m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(" - didFinishDocumentLoadForFrame\n");
    } else {
        unsigned pendingUnloadEvents = frame->unloadListenerCount();
        if (pendingUnloadEvents) {
            printFrameDescription(m_delegate, frame);
            char buffer[100];
            snprintf(buffer, sizeof(buffer), " - has %u onunload handler(s)\n", pendingUnloadEvents);
            m_delegate->printMessage(buffer);
        }
    }
}

void WebTestProxyBase::didHandleOnloadEvents(WebLocalFrame* frame)
{
    if (m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(" - didHandleOnloadEventsForFrame\n");
    }
}

void WebTestProxyBase::didFailLoad(WebLocalFrame* frame, const WebURLError&)
{
    if (m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(" - didFailLoadWithError\n");
    }
    locationChangeDone(frame);
}

void WebTestProxyBase::didFinishLoad(WebLocalFrame* frame)
{
    if (m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(" - didFinishLoadForFrame\n");
    }
    locationChangeDone(frame);
}

void WebTestProxyBase::didDetectXSS(WebLocalFrame*, const WebURL&, bool)
{
    if (m_testInterfaces->testRunner()->shouldDumpFrameLoadCallbacks())
        m_delegate->printMessage("didDetectXSS\n");
}

void WebTestProxyBase::didDispatchPingLoader(WebLocalFrame*, const WebURL& url)
{
    if (m_testInterfaces->testRunner()->shouldDumpPingLoaderCallbacks())
        m_delegate->printMessage(string("PingLoader dispatched to '") + URLDescription(url).c_str() + "'.\n");
}

void WebTestProxyBase::willRequestResource(WebLocalFrame* frame, const blink::WebCachedURLRequest& request)
{
    if (m_testInterfaces->testRunner()->shouldDumpResourceRequestCallbacks()) {
        printFrameDescription(m_delegate, frame);
        m_delegate->printMessage(string(" - ") + request.initiatorName().utf8().data());
        m_delegate->printMessage(string(" requested '") + URLDescription(request.urlRequest().url()).c_str() + "'\n");
    }
}

void WebTestProxyBase::willSendRequest(WebLocalFrame*, unsigned identifier, blink::WebURLRequest& request, const blink::WebURLResponse& redirectResponse)
{
    // Need to use GURL for host() and SchemeIs()
    GURL url = request.url();
    string requestURL = url.possibly_invalid_spec();

    GURL mainDocumentURL = request.firstPartyForCookies();

    if (redirectResponse.isNull() && (m_testInterfaces->testRunner()->shouldDumpResourceLoadCallbacks() || m_testInterfaces->testRunner()->shouldDumpResourcePriorities())) {
        DCHECK(m_resourceIdentifierMap.find(identifier) == m_resourceIdentifierMap.end());
        m_resourceIdentifierMap[identifier] = descriptionSuitableForTestResult(requestURL);
    }

    if (m_testInterfaces->testRunner()->shouldDumpResourceLoadCallbacks()) {
        if (m_resourceIdentifierMap.find(identifier) == m_resourceIdentifierMap.end())
            m_delegate->printMessage("<unknown>");
        else
            m_delegate->printMessage(m_resourceIdentifierMap[identifier]);
        m_delegate->printMessage(" - willSendRequest <NSURLRequest URL ");
        m_delegate->printMessage(descriptionSuitableForTestResult(requestURL).c_str());
        m_delegate->printMessage(", main document URL ");
        m_delegate->printMessage(URLDescription(mainDocumentURL).c_str());
        m_delegate->printMessage(", http method ");
        m_delegate->printMessage(request.httpMethod().utf8().data());
        m_delegate->printMessage("> redirectResponse ");
        printResponseDescription(m_delegate, redirectResponse);
        m_delegate->printMessage("\n");
    }

    if (m_testInterfaces->testRunner()->shouldDumpResourcePriorities()) {
        m_delegate->printMessage(descriptionSuitableForTestResult(requestURL).c_str());
        m_delegate->printMessage(" has priority ");
        m_delegate->printMessage(PriorityDescription(request.priority()));
        m_delegate->printMessage("\n");
    }

    if (m_testInterfaces->testRunner()->httpHeadersToClear()) {
        const set<string> *clearHeaders = m_testInterfaces->testRunner()->httpHeadersToClear();
        for (set<string>::const_iterator header = clearHeaders->begin(); header != clearHeaders->end(); ++header)
            request.clearHTTPHeaderField(WebString::fromUTF8(*header));
    }

    string host = url.host();
    if (!host.empty() && (url.SchemeIs("http") || url.SchemeIs("https"))) {
        if (!isLocalhost(host) && !hostIsUsedBySomeTestsToGenerateError(host)
            && ((!mainDocumentURL.SchemeIs("http") && !mainDocumentURL.SchemeIs("https")) || isLocalhost(mainDocumentURL.host()))
            && !m_delegate->allowExternalPages()) {
            m_delegate->printMessage(string("Blocked access to external URL ") + requestURL + "\n");
            blockRequest(request);
            return;
        }
    }

    // Set the new substituted URL.
    request.setURL(m_delegate->rewriteLayoutTestsURL(request.url().spec()));
}

void WebTestProxyBase::didReceiveResponse(WebLocalFrame*, unsigned identifier, const blink::WebURLResponse& response)
{
    if (m_testInterfaces->testRunner()->shouldDumpResourceLoadCallbacks()) {
        if (m_resourceIdentifierMap.find(identifier) == m_resourceIdentifierMap.end())
            m_delegate->printMessage("<unknown>");
        else
            m_delegate->printMessage(m_resourceIdentifierMap[identifier]);
        m_delegate->printMessage(" - didReceiveResponse ");
        printResponseDescription(m_delegate, response);
        m_delegate->printMessage("\n");
    }
    if (m_testInterfaces->testRunner()->shouldDumpResourceResponseMIMETypes()) {
        GURL url = response.url();
        WebString mimeType = response.mimeType();
        m_delegate->printMessage(url.ExtractFileName());
        m_delegate->printMessage(" has MIME type ");
        // Simulate NSURLResponse's mapping of empty/unknown MIME types to application/octet-stream
        m_delegate->printMessage(mimeType.isEmpty() ? "application/octet-stream" : mimeType.utf8().data());
        m_delegate->printMessage("\n");
    }
}

void WebTestProxyBase::didChangeResourcePriority(WebLocalFrame*, unsigned identifier, const blink::WebURLRequest::Priority& priority)
{
    if (m_testInterfaces->testRunner()->shouldDumpResourcePriorities()) {
        if (m_resourceIdentifierMap.find(identifier) == m_resourceIdentifierMap.end())
            m_delegate->printMessage("<unknown>");
        else
            m_delegate->printMessage(m_resourceIdentifierMap[identifier]);
        m_delegate->printMessage(" changed priority to ");
        m_delegate->printMessage(PriorityDescription(priority));
        m_delegate->printMessage("\n");
    }
}

void WebTestProxyBase::didFinishResourceLoad(WebLocalFrame*, unsigned identifier)
{
    if (m_testInterfaces->testRunner()->shouldDumpResourceLoadCallbacks()) {
        if (m_resourceIdentifierMap.find(identifier) == m_resourceIdentifierMap.end())
            m_delegate->printMessage("<unknown>");
        else
            m_delegate->printMessage(m_resourceIdentifierMap[identifier]);
        m_delegate->printMessage(" - didFinishLoading\n");
    }
    m_resourceIdentifierMap.erase(identifier);
}

void WebTestProxyBase::didAddMessageToConsole(const WebConsoleMessage& message, const WebString& sourceName, unsigned sourceLine)
{
    // This matches win DumpRenderTree's UIDelegate.cpp.
    if (!m_logConsoleOutput)
        return;
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
    m_delegate->printMessage(string("CONSOLE ") + level + ": ");
    if (sourceLine) {
        char buffer[40];
        snprintf(buffer, sizeof(buffer), "line %d: ", sourceLine);
        m_delegate->printMessage(buffer);
    }
    if (!message.text.isEmpty()) {
        string newMessage;
        newMessage = message.text.utf8();
        size_t fileProtocol = newMessage.find("file://");
        if (fileProtocol != string::npos) {
            newMessage = newMessage.substr(0, fileProtocol)
                + urlSuitableForTestResult(newMessage.substr(fileProtocol));
        }
        m_delegate->printMessage(newMessage);
    }
    m_delegate->printMessage(string("\n"));
}

void WebTestProxyBase::runModalAlertDialog(WebLocalFrame*, const WebString& message)
{
    m_delegate->printMessage(string("ALERT: ") + message.utf8().data() + "\n");
}

bool WebTestProxyBase::runModalConfirmDialog(WebLocalFrame*, const WebString& message)
{
    m_delegate->printMessage(string("CONFIRM: ") + message.utf8().data() + "\n");
    return true;
}

bool WebTestProxyBase::runModalPromptDialog(WebLocalFrame* frame, const WebString& message, const WebString& defaultValue, WebString*)
{
    m_delegate->printMessage(string("PROMPT: ") + message.utf8().data() + ", default text: " + defaultValue.utf8().data() + "\n");
    return true;
}

bool WebTestProxyBase::runModalBeforeUnloadDialog(WebLocalFrame*, const WebString& message)
{
    m_delegate->printMessage(string("CONFIRM NAVIGATION: ") + message.utf8().data() + "\n");
    return !m_testInterfaces->testRunner()->shouldStayOnPageAfterHandlingBeforeUnload();
}

void WebTestProxyBase::locationChangeDone(WebFrame* frame)
{
    if (frame != m_testInterfaces->testRunner()->topLoadingFrame())
        return;
    m_testInterfaces->testRunner()->setTopLoadingFrame(frame, true);
}

WebNavigationPolicy WebTestProxyBase::decidePolicyForNavigation(WebLocalFrame*, WebDataSource::ExtraData*, const WebURLRequest& request, WebNavigationType type, WebNavigationPolicy defaultPolicy, bool isRedirect)
{
    WebNavigationPolicy result;
    if (!m_testInterfaces->testRunner()->policyDelegateEnabled())
        return defaultPolicy;

    m_delegate->printMessage(string("Policy delegate: attempt to load ") + URLDescription(request.url()) + " with navigation type '" + webNavigationTypeToString(type) + "'\n");
    if (m_testInterfaces->testRunner()->policyDelegateIsPermissive())
        result = blink::WebNavigationPolicyCurrentTab;
    else
        result = blink::WebNavigationPolicyIgnore;

    if (m_testInterfaces->testRunner()->policyDelegateShouldNotifyDone())
        m_testInterfaces->testRunner()->policyDelegateDone();
    return result;
}

bool WebTestProxyBase::willCheckAndDispatchMessageEvent(WebLocalFrame*, WebFrame*, WebSecurityOrigin, WebDOMMessageEvent)
{
    if (m_testInterfaces->testRunner()->shouldInterceptPostMessage()) {
        m_delegate->printMessage("intercepted postMessage\n");
        return true;
    }

    return false;
}

void WebTestProxyBase::postSpellCheckEvent(const WebString& eventName)
{
    if (m_testInterfaces->testRunner()->shouldDumpSpellCheckCallbacks()) {
        m_delegate->printMessage(string("SpellCheckEvent: ") + eventName.utf8().data() + "\n");
    }
}

void WebTestProxyBase::resetInputMethod()
{
    // If a composition text exists, then we need to let the browser process
    // to cancel the input method's ongoing composition session.
    if (m_webWidget)
        m_webWidget->confirmComposition();
}

}
