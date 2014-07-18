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
#include "third_party/WebKit/public/platform/WebScreenInfo.h"
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
class WebPushClient;
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

class MockScreenOrientationClient;
class MockWebPushClient;
class MockWebSpeechRecognizer;
class MockWebUserMediaClient;
class RenderFrame;
class SpellCheckClient;
class TestInterfaces;
class WebTestDelegate;
class WebTestInterfaces;

// WebTestProxyBase is the "brain" of WebTestProxy in the sense that
// WebTestProxy does the bridge between RenderViewImpl and WebTestProxyBase and
// when it requires a behavior to be different from the usual, it will call
// WebTestProxyBase that implements the expected behavior.
// See WebTestProxy class comments for more information.
class WebTestProxyBase : public blink::WebCompositeAndReadbackAsyncCallback {
 public:
  void SetInterfaces(WebTestInterfaces* interfaces);
  void SetDelegate(WebTestDelegate* delegate);
  void set_widget(blink::WebWidget* widget) { web_widget_ = widget; }

  void Reset();

  blink::WebSpellCheckClient* GetSpellCheckClient() const;
  blink::WebColorChooser* CreateColorChooser(
      blink::WebColorChooserClient* client,
      const blink::WebColor& color,
      const blink::WebVector<blink::WebColorSuggestion>& suggestions);
  bool RunFileChooser(const blink::WebFileChooserParams& params,
                      blink::WebFileChooserCompletion* completion);
  void ShowValidationMessage(const blink::WebRect& anchor_in_root_view,
                             const blink::WebString& message,
                             const blink::WebString& sub_message,
                             blink::WebTextDirection hint);
  void HideValidationMessage();
  void MoveValidationMessage(const blink::WebRect& anchor_in_root_view);

  std::string CaptureTree(bool debug_render_tree);
  void CapturePixelsForPrinting(
      const base::Callback<void(const SkBitmap&)>& callback);
  void CopyImageAtAndCapturePixels(
      int x, int y, const base::Callback<void(const SkBitmap&)>& callback);
  void CapturePixelsAsync(
      const base::Callback<void(const SkBitmap&)>& callback);

  void SetLogConsoleOutput(bool enabled);

  void DidOpenChooser();
  void DidCloseChooser();
  bool IsChooserShown();

  void DisplayAsyncThen(const base::Closure& callback);

  void GetScreenOrientationForTesting(blink::WebScreenInfo&);
  MockScreenOrientationClient* GetScreenOrientationClientMock();
  blink::WebMIDIClientMock* GetMIDIClientMock();
  MockWebSpeechRecognizer* GetSpeechRecognizerMock();

  WebTaskList* mutable_task_list() { return &task_list_; }

  blink::WebView* GetWebView() const;

  void PostSpellCheckEvent(const blink::WebString& event_name);

  // WebCompositeAndReadbackAsyncCallback implementation.
  virtual void didCompositeAndReadback(const SkBitmap& bitmap);

  void SetAcceptLanguages(const std::string& accept_languages);

  MockWebPushClient* GetPushClientMock();

 protected:
  WebTestProxyBase();
  ~WebTestProxyBase();

  void ScheduleAnimation();
  void PostAccessibilityEvent(const blink::WebAXObject&, blink::WebAXEvent);
  void StartDragging(blink::WebLocalFrame* frame,
                     const blink::WebDragData& data,
                     blink::WebDragOperationsMask mask,
                     const blink::WebImage& image,
                     const blink::WebPoint& point);
  void DidChangeSelection(bool isEmptySelection);
  void DidChangeContents();
  void DidEndEditing();
  bool CreateView(blink::WebLocalFrame* creator,
                  const blink::WebURLRequest& request,
                  const blink::WebWindowFeatures& features,
                  const blink::WebString& frame_name,
                  blink::WebNavigationPolicy policy,
                  bool suppress_opener);
  blink::WebPlugin* CreatePlugin(blink::WebLocalFrame* frame,
                                 const blink::WebPluginParams& params);
  void SetStatusText(const blink::WebString& text);
  void DidStopLoading();
  void ShowContextMenu(blink::WebLocalFrame* frame,
                       const blink::WebContextMenuData& data);
  blink::WebUserMediaClient* GetUserMediaClient();
  void PrintPage(blink::WebLocalFrame* frame);
  blink::WebNotificationPresenter* GetNotificationPresenter();
  blink::WebMIDIClient* GetWebMIDIClient();
  blink::WebSpeechRecognizer* GetSpeechRecognizer();
  bool RequestPointerLock();
  void RequestPointerUnlock();
  bool IsPointerLocked();
  void DidFocus();
  void DidBlur();
  void SetToolTipText(const blink::WebString& text,
                      blink::WebTextDirection direction);
  void DidAddMessageToConsole(const blink::WebConsoleMessage& text,
                              const blink::WebString& source_name,
                              unsigned source_line);
  void LoadURLExternally(blink::WebLocalFrame* frame,
                         const blink::WebURLRequest& request,
                         blink::WebNavigationPolicy policy,
                         const blink::WebString& suggested_name);
  void DidStartProvisionalLoad(blink::WebLocalFrame*);
  void DidReceiveServerRedirectForProvisionalLoad(blink::WebLocalFrame* frame);
  bool DidFailProvisionalLoad(blink::WebLocalFrame* frame,
                              const blink::WebURLError& error);
  void DidCommitProvisionalLoad(blink::WebLocalFrame* frame,
                                const blink::WebHistoryItem& history_item,
                                blink::WebHistoryCommitType history_type);
  void DidReceiveTitle(blink::WebLocalFrame* frame,
                       const blink::WebString& title,
                       blink::WebTextDirection text_direction);
  void DidChangeIcon(blink::WebLocalFrame* frame,
                     blink::WebIconURL::Type icon_type);
  void DidFinishDocumentLoad(blink::WebLocalFrame* frame);
  void DidHandleOnloadEvents(blink::WebLocalFrame* frame);
  void DidFailLoad(blink::WebLocalFrame* frame,
                   const blink::WebURLError& error);
  void DidFinishLoad(blink::WebLocalFrame* frame);
  void DidChangeLocationWithinPage(blink::WebLocalFrame* frame);
  void DidDetectXSS(blink::WebLocalFrame* frame,
                    const blink::WebURL& insecure_url,
                    bool did_block_entire_page);
  void DidDispatchPingLoader(blink::WebLocalFrame* frame,
                             const blink::WebURL& url);
  void WillRequestResource(blink::WebLocalFrame* frame,
                           const blink::WebCachedURLRequest& url_request);
  void WillSendRequest(blink::WebLocalFrame* frame,
                       unsigned identifier,
                       blink::WebURLRequest& request,
                       const blink::WebURLResponse& redirect_response);
  void DidReceiveResponse(blink::WebLocalFrame* frame,
                          unsigned identifier,
                          const blink::WebURLResponse& response);
  void DidChangeResourcePriority(blink::WebLocalFrame* frame,
                                 unsigned identifier,
                                 const blink::WebURLRequest::Priority& priority,
                                 int intra_priority_value);
  void DidFinishResourceLoad(blink::WebLocalFrame* frame, unsigned identifier);
  blink::WebNavigationPolicy DecidePolicyForNavigation(
      blink::WebLocalFrame* frame,
      blink::WebDataSource::ExtraData* data,
      const blink::WebURLRequest& request,
      blink::WebNavigationType navigation_type,
      blink::WebNavigationPolicy default_policy,
      bool is_redirect);
  bool WillCheckAndDispatchMessageEvent(blink::WebLocalFrame* source_frame,
                                        blink::WebFrame* target_frame,
                                        blink::WebSecurityOrigin target,
                                        blink::WebDOMMessageEvent event);
  void ResetInputMethod();

  blink::WebString acceptLanguages();
  blink::WebPushClient* GetWebPushClient();

 private:
  template <class, typename, typename>
  friend class WebFrameTestProxy;
  void LocationChangeDone(blink::WebFrame* frame);
  void AnimateNow();
  void DrawSelectionRect(SkCanvas* canvas);
  void DidDisplayAsync(const base::Closure& callback, const SkBitmap& bitmap);

  blink::WebWidget* web_widget() const { return web_widget_; }

  TestInterfaces* test_interfaces_;
  WebTestDelegate* delegate_;
  blink::WebWidget* web_widget_;

  WebTaskList task_list_;

  scoped_ptr<SpellCheckClient> spellcheck_;
  scoped_ptr<MockWebUserMediaClient> user_media_client_;

  bool animate_scheduled_;
  std::map<unsigned, std::string> resource_identifier_map_;
  std::deque<base::Callback<void(const SkBitmap&)> >
      composite_and_readback_callbacks_;

  bool log_console_output_;
  int chooser_count_;

  scoped_ptr<blink::WebMIDIClientMock> midi_client_;
  scoped_ptr<MockWebSpeechRecognizer> speech_recognizer_;
  scoped_ptr<MockWebPushClient> push_client_;
  scoped_ptr<MockScreenOrientationClient> screen_orientation_client_;

  std::string accept_languages_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebTestProxyBase);
};

// WebTestProxy is used during LayoutTests and always instantiated, at time of
// writing with Base=RenderViewImpl. It does not directly inherit from it for
// layering purposes.
// The intent of that class is to wrap RenderViewImpl for tests purposes in
// order to reduce the amount of test specific code in the production code.
// WebTestProxy is only doing the glue between RenderViewImpl and
// WebTestProxyBase, that means that there is no logic living in this class
// except deciding which base class should be called (could be both).
//
// Examples of usage:
//  * when a fooClient has a mock implementation, WebTestProxy can override the
//    fooClient() call and have WebTestProxyBase return the mock implementation.
//  * when a value needs to be overridden by LayoutTests, WebTestProxy can
//    override RenderViewImpl's getter and call a getter from WebTestProxyBase
//    instead. In addition, WebTestProxyBase will have a public setter that
//    could be called from the TestRunner.
template <class Base, typename T>
class WebTestProxy : public Base, public WebTestProxyBase {
 public:
  explicit WebTestProxy(T t) : Base(t) {}

  virtual ~WebTestProxy() {}

  // WebWidgetClient implementation.
  virtual blink::WebScreenInfo screenInfo() {
    blink::WebScreenInfo info = Base::screenInfo();
    WebTestProxyBase::GetScreenOrientationForTesting(info);
    return info;
  }

  // WebViewClient implementation.
  virtual void scheduleAnimation() { WebTestProxyBase::ScheduleAnimation(); }
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
                                     const blink::WebString& frame_name,
                                     blink::WebNavigationPolicy policy,
                                     bool suppress_opener) {
    if (!WebTestProxyBase::CreateView(
            creator, request, features, frame_name, policy, suppress_opener))
      return 0;
    return Base::createView(
        creator, request, features, frame_name, policy, suppress_opener);
  }
  virtual void setStatusText(const blink::WebString& text) {
    WebTestProxyBase::SetStatusText(text);
    Base::setStatusText(text);
  }
  virtual void printPage(blink::WebLocalFrame* frame) {
    WebTestProxyBase::PrintPage(frame);
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
  virtual void showValidationMessage(const blink::WebRect& anchor_in_root_view,
                                     const blink::WebString& message,
                                     const blink::WebString& sub_message,
                                     blink::WebTextDirection hint) {
    WebTestProxyBase::ShowValidationMessage(
        anchor_in_root_view, message, sub_message, hint);
  }
  virtual void postSpellCheckEvent(const blink::WebString& event_name) {
    WebTestProxyBase::PostSpellCheckEvent(event_name);
  }
  virtual blink::WebString acceptLanguages() {
    return WebTestProxyBase::acceptLanguages();
  }
  virtual blink::WebPushClient* webPushClient() {
    return WebTestProxyBase::GetWebPushClient();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebTestProxy);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_WEB_TEST_PROXY_H_
