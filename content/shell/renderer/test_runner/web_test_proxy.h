// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_WEB_TEST_PROXY_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_WEB_TEST_PROXY_H_

#include <deque>
#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "content/shell/renderer/test_runner/WebTask.h"
#include "third_party/WebKit/public/platform/WebCompositeAndReadbackAsyncCallback.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebAXEnums.h"
#include "third_party/WebKit/public/web/WebDOMMessageEvent.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDragOperation.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebHistoryCommitType.h"
#include "third_party/WebKit/public/web/WebIconURL.h"
#include "third_party/WebKit/public/web/WebNavigationPolicy.h"
#include "third_party/WebKit/public/web/WebNavigationType.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebTextAffinity.h"
#include "third_party/WebKit/public/web/WebTextDirection.h"

class SkCanvas;

namespace blink {
class WebAXObject;
class WebAudioDevice;
class WebCachedURLRequest;
class WebColorChooser;
class WebColorChooserClient;
class WebDataSource;
class WebDragData;
class WebFileChooserCompletion;
class WebFrame;
class WebImage;
class WebLocalFrame;
class WebMIDIAccessor;
class WebMIDIAccessorClient;
class WebMIDIClient;
class WebMIDIClientMock;
class WebNode;
class WebNotificationPresenter;
class WebPlugin;
class WebRange;
class WebSerializedScriptValue;
class WebSpeechRecognizer;
class WebSpellCheckClient;
class WebString;
class WebURL;
class WebURLResponse;
class WebUserMediaClient;
class WebView;
class WebWidget;
struct WebColorSuggestion;
struct WebConsoleMessage;
struct WebContextMenuData;
struct WebFileChooserParams;
struct WebPluginParams;
struct WebPoint;
struct WebSize;
struct WebWindowFeatures;
typedef unsigned WebColor;
}

namespace content {

class MockWebSpeechRecognizer;
class RenderFrame;
class SpellCheckClient;
class TestInterfaces;
class WebTestDelegate;
class WebTestInterfaces;
class WebUserMediaClientMock;

class WebTestProxyBase : public blink::WebCompositeAndReadbackAsyncCallback {
 public:
  void SetInterfaces(WebTestInterfaces* interfaces);
  void SetDelegate(WebTestDelegate* delegate);
  void set_widget(blink::WebWidget* widget) { web_widget_ = widget; }

  void Reset();

  blink::WebSpellCheckClient* GetSpellCheckClient() const;
  blink::WebColorChooser* CreateColorChooser(
      blink::WebColorChooserClient*, const blink::WebColor&,
      const blink::WebVector<blink::WebColorSuggestion>& suggestions);
  bool RunFileChooser(const blink::WebFileChooserParams&,
                      blink::WebFileChooserCompletion*);
  void ShowValidationMessage(const blink::WebRect& anchorInRootView,
                             const blink::WebString& mainText,
                             const blink::WebString& supplementalText,
                             blink::WebTextDirection);
  void HideValidationMessage();
  void MoveValidationMessage(const blink::WebRect& anchorInRootView);

  std::string CaptureTree(bool debugRenderTree);
  void CapturePixelsForPrinting(
      const base::Callback<void(const SkBitmap&)>& callback);
  void CapturePixelsAsync(
      const base::Callback<void(const SkBitmap&)>& callback);

  void SetLogConsoleOutput(bool enabled);

  // FIXME: Make this private again.
  void ScheduleComposite();

  void DidOpenChooser();
  void DidCloseChooser();
  bool IsChooserShown();

  void DisplayAsyncThen(const base::Closure& callback);

  void DiscardBackingStore();

  blink::WebMIDIClientMock* GetMIDIClientMock();
  MockWebSpeechRecognizer* GetSpeechRecognizerMock();

  WebTaskList* mutable_task_list() { return &task_list_; }

  blink::WebView* GetWebView() const;

  void DidForceResize();

  void PostSpellCheckEvent(const blink::WebString& eventName);

  // WebCompositeAndReadbackAsyncCallback implementation.
  virtual void didCompositeAndReadback(const SkBitmap& bitmap);

 protected:
  WebTestProxyBase();
  ~WebTestProxyBase();

  void DidInvalidateRect(const blink::WebRect&);
  void DidScrollRect(int, int, const blink::WebRect&);
  void ScheduleAnimation();
  bool IsCompositorFramePending() const;
  // FIXME: Remove once we switch to use didForceResize.
  void SetWindowRect(const blink::WebRect&);
  void Show(blink::WebNavigationPolicy);
  void DidAutoResize(const blink::WebSize&);
  void PostAccessibilityEvent(const blink::WebAXObject&, blink::WebAXEvent);
  void StartDragging(blink::WebLocalFrame*, const blink::WebDragData&,
                     blink::WebDragOperationsMask, const blink::WebImage&,
                     const blink::WebPoint&);
  void DidChangeSelection(bool isEmptySelection);
  void DidChangeContents();
  void DidEndEditing();
  bool CreateView(blink::WebLocalFrame* creator, const blink::WebURLRequest&,
                  const blink::WebWindowFeatures&,
                  const blink::WebString& frameName, blink::WebNavigationPolicy,
                  bool suppressOpener);
  blink::WebPlugin* CreatePlugin(blink::WebLocalFrame*,
                                 const blink::WebPluginParams&);
  void SetStatusText(const blink::WebString&);
  void DidStopLoading();
  void ShowContextMenu(blink::WebLocalFrame*, const blink::WebContextMenuData&);
  blink::WebUserMediaClient* GetUserMediaClient();
  void PrintPage(blink::WebLocalFrame*);
  blink::WebNotificationPresenter* GetNotificationPresenter();
  blink::WebMIDIClient* GetWebMIDIClient();
  blink::WebSpeechRecognizer* GetSpeechRecognizer();
  bool RequestPointerLock();
  void RequestPointerUnlock();
  bool IsPointerLocked();
  void DidFocus();
  void DidBlur();
  void SetToolTipText(const blink::WebString&, blink::WebTextDirection);
  void DidAddMessageToConsole(const blink::WebConsoleMessage&,
                              const blink::WebString& sourceName,
                              unsigned sourceLine);
  void LoadURLExternally(blink::WebLocalFrame* frame,
                         const blink::WebURLRequest& request,
                         blink::WebNavigationPolicy policy,
                         const blink::WebString& suggested_name);
  void DidStartProvisionalLoad(blink::WebLocalFrame*);
  void DidReceiveServerRedirectForProvisionalLoad(blink::WebLocalFrame*);
  bool DidFailProvisionalLoad(blink::WebLocalFrame*, const blink::WebURLError&);
  void DidCommitProvisionalLoad(blink::WebLocalFrame*,
                                const blink::WebHistoryItem&,
                                blink::WebHistoryCommitType);
  void DidReceiveTitle(blink::WebLocalFrame*, const blink::WebString& title,
                       blink::WebTextDirection);
  void DidChangeIcon(blink::WebLocalFrame*, blink::WebIconURL::Type);
  void DidFinishDocumentLoad(blink::WebLocalFrame*);
  void DidHandleOnloadEvents(blink::WebLocalFrame*);
  void DidFailLoad(blink::WebLocalFrame*, const blink::WebURLError&);
  void DidFinishLoad(blink::WebLocalFrame*);
  void DidChangeLocationWithinPage(blink::WebLocalFrame*);
  void DidDetectXSS(blink::WebLocalFrame*, const blink::WebURL& insecureURL,
                    bool didBlockEntirePage);
  void DidDispatchPingLoader(blink::WebLocalFrame*, const blink::WebURL&);
  void WillRequestResource(blink::WebLocalFrame*,
                           const blink::WebCachedURLRequest&);
  void WillSendRequest(blink::WebLocalFrame*, unsigned identifier,
                       blink::WebURLRequest&,
                       const blink::WebURLResponse& redirectResponse);
  void DidReceiveResponse(blink::WebLocalFrame*, unsigned identifier,
                          const blink::WebURLResponse&);
  void DidChangeResourcePriority(blink::WebLocalFrame*, unsigned identifier,
                                 const blink::WebURLRequest::Priority&,
                                 int intra_priority_value);
  void DidFinishResourceLoad(blink::WebLocalFrame*, unsigned identifier);
  blink::WebNavigationPolicy DecidePolicyForNavigation(
      blink::WebLocalFrame*, blink::WebDataSource::ExtraData*,
      const blink::WebURLRequest&, blink::WebNavigationType,
      blink::WebNavigationPolicy defaultPolicy, bool isRedirect);
  bool WillCheckAndDispatchMessageEvent(blink::WebLocalFrame* sourceFrame,
                                        blink::WebFrame* targetFrame,
                                        blink::WebSecurityOrigin target,
                                        blink::WebDOMMessageEvent);
  void ResetInputMethod();

 private:
  template <class, typename, typename>
  friend class WebFrameTestProxy;
  void LocationChangeDone(blink::WebFrame*);
  void PaintPagesWithBoundaries();
  SkCanvas* GetCanvas();
  void InvalidateAll();
  void AnimateNow();
  void DrawSelectionRect(SkCanvas* canvas);
  void DidDisplayAsync(const base::Closure& callback, const SkBitmap& bitmap);

  blink::WebWidget* web_widget() const { return web_widget_; }

  TestInterfaces* test_interfaces_;
  WebTestDelegate* delegate_;
  blink::WebWidget* web_widget_;

  WebTaskList task_list_;

  scoped_ptr<SpellCheckClient> spellcheck_;
  scoped_ptr<WebUserMediaClientMock> user_media_client_;

  // Painting.
  scoped_ptr<SkCanvas> canvas_;
  blink::WebRect paint_rect_;
  bool is_painting_;
  bool animate_scheduled_;
  std::map<unsigned, std::string> resource_identifier_map_;
  std::deque<base::Callback<void(const SkBitmap&)> >
      composite_and_readback_callbacks_;

  bool log_console_output_;
  int chooser_count_;

  scoped_ptr<blink::WebMIDIClientMock> m_midiClient;
  scoped_ptr<MockWebSpeechRecognizer> m_speechRecognizer;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebTestProxyBase);
};

// Use this template to inject methods into your WebViewClient/WebFrameClient
// implementation required for the running layout tests.
template <class Base, typename T>
class WebTestProxy : public Base, public WebTestProxyBase {
 public:
  explicit WebTestProxy(T t) : Base(t) {}

  virtual ~WebTestProxy() {}

  // WebViewClient implementation.
  virtual void didInvalidateRect(const blink::WebRect& rect) {
    WebTestProxyBase::DidInvalidateRect(rect);
  }
  virtual void didScrollRect(int dx, int dy, const blink::WebRect& clipRect) {
    WebTestProxyBase::DidScrollRect(dx, dy, clipRect);
  }
  virtual void scheduleComposite() { WebTestProxyBase::ScheduleComposite(); }
  virtual void scheduleAnimation() { WebTestProxyBase::ScheduleAnimation(); }
  virtual bool isCompositorFramePending() const {
    return WebTestProxyBase::IsCompositorFramePending();
  }
  virtual void setWindowRect(const blink::WebRect& rect) {
    WebTestProxyBase::SetWindowRect(rect);
    Base::setWindowRect(rect);
  }
  virtual void show(blink::WebNavigationPolicy policy) {
    WebTestProxyBase::Show(policy);
    Base::show(policy);
  }
  virtual void didAutoResize(const blink::WebSize& newSize) {
    WebTestProxyBase::DidAutoResize(newSize);
    Base::didAutoResize(newSize);
  }
  virtual void postAccessibilityEvent(const blink::WebAXObject& object,
                                      blink::WebAXEvent event) {
    WebTestProxyBase::PostAccessibilityEvent(object, event);
    Base::postAccessibilityEvent(object, event);
  }
  virtual void startDragging(blink::WebLocalFrame* frame,
                             const blink::WebDragData& data,
                             blink::WebDragOperationsMask mask,
                             const blink::WebImage& image,
                             const blink::WebPoint& point) {
    WebTestProxyBase::StartDragging(frame, data, mask, image, point);
    // Don't forward this call to Base because we don't want to do a real
    // drag-and-drop.
  }
  virtual void didChangeContents() {
    WebTestProxyBase::DidChangeContents();
    Base::didChangeContents();
  }
  virtual blink::WebView* createView(blink::WebLocalFrame* creator,
                                     const blink::WebURLRequest& request,
                                     const blink::WebWindowFeatures& features,
                                     const blink::WebString& frameName,
                                     blink::WebNavigationPolicy policy,
                                     bool suppressOpener) {
    if (!WebTestProxyBase::CreateView(creator, request, features, frameName,
                                      policy, suppressOpener))
      return 0;
    return Base::createView(creator, request, features, frameName, policy,
                            suppressOpener);
  }
  virtual void setStatusText(const blink::WebString& text) {
    WebTestProxyBase::SetStatusText(text);
    Base::setStatusText(text);
  }
  virtual blink::WebUserMediaClient* userMediaClient() {
    return WebTestProxyBase::GetUserMediaClient();
  }
  virtual void printPage(blink::WebLocalFrame* frame) {
    WebTestProxyBase::PrintPage(frame);
  }
  virtual blink::WebMIDIClient* webMIDIClient() {
    return WebTestProxyBase::GetWebMIDIClient();
  }
  virtual blink::WebSpeechRecognizer* speechRecognizer() {
    return WebTestProxyBase::GetSpeechRecognizer();
  }
  virtual bool requestPointerLock() {
    return WebTestProxyBase::RequestPointerLock();
  }
  virtual void requestPointerUnlock() {
    WebTestProxyBase::RequestPointerUnlock();
  }
  virtual bool isPointerLocked() { return WebTestProxyBase::IsPointerLocked(); }
  virtual void didFocus() {
    WebTestProxyBase::DidFocus();
    Base::didFocus();
  }
  virtual void didBlur() {
    WebTestProxyBase::DidBlur();
    Base::didBlur();
  }
  virtual void setToolTipText(const blink::WebString& text,
                              blink::WebTextDirection hint) {
    WebTestProxyBase::SetToolTipText(text, hint);
    Base::setToolTipText(text, hint);
  }
  virtual void resetInputMethod() { WebTestProxyBase::ResetInputMethod(); }
  virtual bool runFileChooser(const blink::WebFileChooserParams& params,
                              blink::WebFileChooserCompletion* completion) {
    return WebTestProxyBase::RunFileChooser(params, completion);
  }
  virtual void showValidationMessage(const blink::WebRect& anchorInRootView,
                                     const blink::WebString& mainText,
                                     const blink::WebString& supplementalText,
                                     blink::WebTextDirection hint) {
    WebTestProxyBase::ShowValidationMessage(anchorInRootView, mainText,
                                            supplementalText, hint);
  }
  virtual void hideValidationMessage() {
    WebTestProxyBase::HideValidationMessage();
  }
  virtual void moveValidationMessage(const blink::WebRect& anchorInRootView) {
    WebTestProxyBase::MoveValidationMessage(anchorInRootView);
  }
  virtual void postSpellCheckEvent(const blink::WebString& eventName) {
    WebTestProxyBase::PostSpellCheckEvent(eventName);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebTestProxy);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_WEB_TEST_PROXY_H_
