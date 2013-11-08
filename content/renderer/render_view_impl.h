// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_VIEW_IMPL_H_
#define CONTENT_RENDERER_RENDER_VIEW_IMPL_H_

#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/id_map.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "cc/input/top_controls_state.h"
#include "content/common/content_export.h"
#include "content/common/drag_event_source_info.h"
#include "content/common/edit_command.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/common/navigation_gesture.h"
#include "content/common/view_message_enums.h"
#include "content/public/common/javascript_message_type.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/referrer.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/stop_find_action.h"
#include "content/public/common/top_controls_state.h"
#include "content/public/renderer/render_view.h"
#include "content/renderer/media/webmediaplayer_delegate.h"
#include "content/renderer/mouse_lock_dispatcher.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_widget.h"
#include "content/renderer/renderer_webcookiejar_impl.h"
#include "content/renderer/stats_collection_observer.h"
#include "ipc/ipc_platform_file.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/WebKit/public/web/WebAXObject.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"
#include "third_party/WebKit/public/web/WebHistoryItem.h"
#include "third_party/WebKit/public/web/WebIconURL.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebNavigationType.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebPageSerializerClient.h"
#include "third_party/WebKit/public/web/WebPageVisibilityState.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebViewClient.h"
#include "ui/base/ui_base_types.h"
#include "ui/surface/transport_dib.h"
#include "webkit/common/webpreferences.h"

#if defined(OS_ANDROID)
#include "content/renderer/android/content_detector.h"
#include "third_party/WebKit/public/web/WebContentDetectionResult.h"
#endif

#if defined(COMPILER_MSVC)
// RenderViewImpl is a diamond-shaped hierarchy, with WebWidgetClient at the
// root. VS warns when we inherit the WebWidgetClient method implementations
// from RenderWidget.  It's safe to ignore that warning.
#pragma warning(disable: 4250)
#endif

class CommandLine;
class PepperDeviceTest;
class SkBitmap;
struct PP_NetAddress_Private;
struct ViewMsg_Navigate_Params;
struct ViewMsg_PostMessage_Params;
struct ViewMsg_StopFinding_Params;

namespace ui {
struct SelectedFileInfo;
}  // namespace ui

namespace blink {
class WebApplicationCacheHost;
class WebApplicationCacheHostClient;
class WebDOMMessageEvent;
class WebDataSource;
class WebDateTimeChooserCompletion;
class WebDragData;
class WebGeolocationClient;
class WebGestureEvent;
class WebIconURL;
class WebImage;
class WebPeerConnection00Handler;
class WebPeerConnection00HandlerClient;
class WebMediaPlayer;
class WebMediaPlayerClient;
class WebMouseEvent;
class WebPeerConnectionHandler;
class WebPeerConnectionHandlerClient;
class WebSocketStreamHandle;
class WebSpeechInputController;
class WebSpeechInputListener;
class WebSpeechRecognizer;
class WebStorageNamespace;
class WebTouchEvent;
class WebURLRequest;
class WebUserMediaClient;
struct WebActiveWheelFlingParameters;
struct WebCursorInfo;
struct WebDateTimeChooserParams;
struct WebFileChooserParams;
struct WebFindOptions;
struct WebMediaPlayerAction;
struct WebPluginAction;
struct WebPoint;
struct WebWindowFeatures;

#if defined(OS_ANDROID)
class WebHitTestResult;
#endif
}

namespace webkit_glue {
class WebURLResponseExtraDataImpl;
}

namespace content {
class BrowserPluginManager;
class DeviceOrientationDispatcher;
class DevToolsAgent;
class DocumentState;
class DomAutomationController;
class ExternalPopupMenu;
class FaviconHelper;
class GeolocationDispatcher;
class ImageResourceFetcher;
class InputTagSpeechDispatcher;
class JavaBridgeDispatcher;
class LoadProgressTracker;
class MIDIDispatcher;
class MediaStreamClient;
class MediaStreamDispatcher;
class MouseLockDispatcher;
class NavigationState;
class NotificationProvider;
class PepperPluginInstanceImpl;
class RenderViewObserver;
class RenderViewTest;
class RendererAccessibility;
class RendererDateTimePicker;
class RendererPpapiHost;
class RendererWebColorChooserImpl;
class RenderWidgetFullscreenPepper;
class SpeechRecognitionDispatcher;
class StatsCollectionController;
class WebPluginDelegateProxy;
struct CustomContextMenuContext;
struct DropData;
struct FaviconURL;
struct FileChooserParams;
struct RenderViewImplParams;

#if defined(OS_ANDROID)
class RendererMediaPlayerManager;
class WebMediaPlayerProxyAndroid;
#endif

//
// RenderView is an object that manages a WebView object, and provides a
// communication interface with an embedding application process
//
class CONTENT_EXPORT RenderViewImpl
    : public RenderWidget,
      NON_EXPORTED_BASE(public blink::WebViewClient),
      NON_EXPORTED_BASE(public blink::WebFrameClient),
      NON_EXPORTED_BASE(public blink::WebPageSerializerClient),
      public RenderView,
      NON_EXPORTED_BASE(public WebMediaPlayerDelegate),
      public base::SupportsWeakPtr<RenderViewImpl> {
 public:
  // Creates a new RenderView. |opener_id| is the routing ID of the RenderView
  // responsible for creating this RenderView.
  static RenderViewImpl* Create(
      int32 opener_id,
      const RendererPreferences& renderer_prefs,
      const WebPreferences& webkit_prefs,
      int32 routing_id,
      int32 main_frame_routing_id,
      int32 surface_id,
      int64 session_storage_namespace_id,
      const string16& frame_name,
      bool is_renderer_created,
      bool swapped_out,
      bool hidden,
      int32 next_page_id,
      const blink::WebScreenInfo& screen_info,
      AccessibilityMode accessibility_mode,
      bool allow_partial_swap);

  // Used by content_layouttest_support to hook into the creation of
  // RenderViewImpls.
  static void InstallCreateHook(
      RenderViewImpl* (*create_render_view_impl)(RenderViewImplParams*));

  // Returns the RenderViewImpl containing the given WebView.
  static RenderViewImpl* FromWebView(blink::WebView* webview);

  // Returns the RenderViewImpl for the given routing ID.
  static RenderViewImpl* FromRoutingID(int routing_id);

  // May return NULL when the view is closing.
  blink::WebView* webview() const;

  int history_list_offset() const { return history_list_offset_; }

  const WebPreferences& webkit_preferences() const {
    return webkit_preferences_;
  }

  void set_send_content_state_immediately(bool value) {
    send_content_state_immediately_ = value;
  }

  MediaStreamDispatcher* media_stream_dispatcher() {
    return media_stream_dispatcher_;
  }

  MouseLockDispatcher* mouse_lock_dispatcher() {
    return mouse_lock_dispatcher_;
  }

  RendererWebCookieJarImpl* cookie_jar() { return &cookie_jar_; }

  // Lazily initialize this view's BrowserPluginManager and return it.
  BrowserPluginManager* GetBrowserPluginManager();

  // Functions to add and remove observers for this object.
  void AddObserver(RenderViewObserver* observer);
  void RemoveObserver(RenderViewObserver* observer);

  // Returns the StatsCollectionObserver associated with this view, or NULL
  // if one wasn't created;
  StatsCollectionObserver* GetStatsCollectionObserver() {
    return stats_collection_observer_.get();
  }

  // Adds the given file chooser request to the file_chooser_completion_ queue
  // (see that var for more) and requests the chooser be displayed if there are
  // no other waiting items in the queue.
  //
  // Returns true if the chooser was successfully scheduled. False means we
  // didn't schedule anything.
  bool ScheduleFileChooser(const FileChooserParams& params,
                           blink::WebFileChooserCompletion* completion);

  void LoadNavigationErrorPage(
      blink::WebFrame* frame,
      const blink::WebURLRequest& failed_request,
      const blink::WebURLError& error,
      const std::string& html,
      bool replace);

  // Plugin-related functions --------------------------------------------------

#if defined(ENABLE_PLUGINS)
  // Indicates that the given instance has been created.
  void PepperInstanceCreated(PepperPluginInstanceImpl* instance);

  // Indicates that the given instance is being destroyed. This is called from
  // the destructor, so it's important that the instance is not dereferenced
  // from this call.
  void PepperInstanceDeleted(PepperPluginInstanceImpl* instance);

  // Notifies that |instance| has changed the cursor.
  // This will update the cursor appearance if it is currently over the plugin
  // instance.
  void PepperDidChangeCursor(PepperPluginInstanceImpl* instance,
                             const blink::WebCursorInfo& cursor);

  // Notifies that |instance| has received a mouse event.
  void PepperDidReceiveMouseEvent(PepperPluginInstanceImpl* instance);

  // Notification that the given plugin is focused or unfocused.
  void PepperFocusChanged(PepperPluginInstanceImpl* instance, bool focused);

  // Informs the render view that a PPAPI plugin has changed text input status.
  void PepperTextInputTypeChanged(PepperPluginInstanceImpl* instance);
  void PepperCaretPositionChanged(PepperPluginInstanceImpl* instance);

  // Cancels current composition.
  void PepperCancelComposition(PepperPluginInstanceImpl* instance);

  // Informs the render view that a PPAPI plugin has changed selection.
  void PepperSelectionChanged(PepperPluginInstanceImpl* instance);

  // Creates a fullscreen container for a pepper plugin instance.
  RenderWidgetFullscreenPepper* CreatePepperFullscreenContainer(
      PepperPluginInstanceImpl* plugin);

  // Notification that a PPAPI plugin has been created.
  void PepperPluginCreated(RendererPpapiHost* host);

  // Retrieves the current caret position if a PPAPI plugin has focus.
  bool GetPepperCaretBounds(gfx::Rect* rect);

  bool IsPepperAcceptingCompositionEvents() const;

  // Notification that the given plugin has crashed.
  void PluginCrashed(const base::FilePath& plugin_path,
                     base::ProcessId plugin_pid);

  // Simulates IME events for testing purpose.
  void SimulateImeSetComposition(
      const string16& text,
      const std::vector<blink::WebCompositionUnderline>& underlines,
      int selection_start,
      int selection_end);
  void SimulateImeConfirmComposition(const string16& text,
                                     const gfx::Range& replacement_range);

#if defined(OS_MACOSX) || defined(OS_WIN)
  // Informs the render view that the given plugin has gained or lost focus.
  void PluginFocusChanged(bool focused, int plugin_id);
#endif

#if defined(OS_MACOSX)
  // Starts plugin IME.
  void StartPluginIme();
#endif

  void RegisterPluginDelegate(WebPluginDelegateProxy* delegate);
  void UnregisterPluginDelegate(WebPluginDelegateProxy* delegate);

  // Helper function to retrieve information about a plugin for a URL and mime
  // type. Returns false if no plugin was found.
  // |actual_mime_type| is the actual mime type supported by the
  // plugin found that match the URL given (one for each item in
  // |info|).
  bool GetPluginInfo(const GURL& url,
                     const GURL& page_url,
                     const std::string& mime_type,
                     WebPluginInfo* plugin_info,
                     std::string* actual_mime_type);

#endif  // ENABLE_PLUGINS

  void TransferActiveWheelFlingAnimation(
      const blink::WebActiveWheelFlingParameters& params);

  // Returns true if the focused element is editable text from the perspective
  // of IME support (also used for on-screen keyboard). Works correctly inside
  // supported PPAPI plug-ins.
  bool HasIMETextFocus();

  // Callback for use with GetWindowSnapshot.
  typedef base::Callback<void(
      const gfx::Size&, const std::vector<unsigned char>&)>
      WindowSnapshotCallback;

  void GetWindowSnapshot(const WindowSnapshotCallback& callback);

  // Dispatches the current navigation state to the browser. Called on a
  // periodic timer so we don't send too many messages.
  void SyncNavigationState();

  // Returns the length of the session history of this RenderView. Note that
  // this only coincides with the actual length of the session history if this
  // RenderView is the currently active RenderView of a WebContents.
  unsigned GetLocalSessionHistoryLengthForTesting() const;

  // Invokes OnSetFocus and marks the widget as active depending on the value
  // of |enable|. This is used for layout tests that need to control the focus
  // synchronously from the renderer.
  void SetFocusAndActivateForTesting(bool enable);

  // Change the device scale factor and force the compositor to resize.
  void SetDeviceScaleFactorForTesting(float factor);

  // Used to force the size of a window when running layout tests.
  void ForceResizeForTesting(const gfx::Size& new_size);

  void UseSynchronousResizeModeForTesting(bool enable);

  // Control autoresize mode.
  void EnableAutoResizeForTesting(const gfx::Size& min_size,
                                  const gfx::Size& max_size);
  void DisableAutoResizeForTesting(const gfx::Size& new_size);

  // Overrides the MediaStreamClient used when creating MediaStream players.
  // Must be called before any players are created.
  void SetMediaStreamClientForTesting(MediaStreamClient* media_stream_client);

  // Determines whether plugins are allowed to enter fullscreen mode.
  bool IsPluginFullscreenAllowed();

  // IPC::Listener implementation ----------------------------------------------

  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  // blink::WebWidgetClient implementation ------------------------------------

  // Most methods are handled by RenderWidget.
  virtual void didFocus();
  virtual void didBlur();
  virtual void show(blink::WebNavigationPolicy policy);
  virtual void runModal();
  virtual bool enterFullScreen();
  virtual void exitFullScreen();
  virtual bool requestPointerLock();
  virtual void requestPointerUnlock();
  virtual bool isPointerLocked();
  virtual void didActivateCompositor(int input_handler_identifier);
  virtual void didHandleGestureEvent(const blink::WebGestureEvent& event,
                                     bool event_cancelled) OVERRIDE;
  virtual void initializeLayerTreeView() OVERRIDE;

  // blink::WebViewClient implementation --------------------------------------

  virtual blink::WebView* createView(
      blink::WebFrame* creator,
      const blink::WebURLRequest& request,
      const blink::WebWindowFeatures& features,
      const blink::WebString& frame_name,
      blink::WebNavigationPolicy policy,
      bool suppress_opener);
  virtual blink::WebWidget* createPopupMenu(blink::WebPopupType popup_type);
  virtual blink::WebExternalPopupMenu* createExternalPopupMenu(
      const blink::WebPopupMenuInfo& popup_menu_info,
      blink::WebExternalPopupMenuClient* popup_menu_client);
  virtual blink::WebStorageNamespace* createSessionStorageNamespace();
  virtual bool shouldReportDetailedMessageForSource(
      const blink::WebString& source);
  virtual void didAddMessageToConsole(
      const blink::WebConsoleMessage& message,
      const blink::WebString& source_name,
      unsigned source_line,
      const blink::WebString& stack_trace);
  virtual void printPage(blink::WebFrame* frame);
  virtual blink::WebNotificationPresenter* notificationPresenter();
  virtual bool enumerateChosenDirectory(
      const blink::WebString& path,
      blink::WebFileChooserCompletion* chooser_completion);
  virtual void initializeHelperPluginWebFrame(blink::WebHelperPlugin*);
  virtual void didStartLoading();
  virtual void didStopLoading();
  virtual void didChangeLoadProgress(blink::WebFrame* frame,
                                     double load_progress);
  virtual void didCancelCompositionOnSelectionChange();
  virtual void didChangeSelection(bool is_selection_empty);
  virtual void didExecuteCommand(const blink::WebString& command_name);
  virtual bool handleCurrentKeyboardEvent();
  virtual blink::WebColorChooser* createColorChooser(
      blink::WebColorChooserClient*, const blink::WebColor& initial_color);
  virtual bool runFileChooser(
      const blink::WebFileChooserParams& params,
      blink::WebFileChooserCompletion* chooser_completion);
  virtual void runModalAlertDialog(blink::WebFrame* frame,
                                   const blink::WebString& message);
  virtual bool runModalConfirmDialog(blink::WebFrame* frame,
                                     const blink::WebString& message);
  virtual bool runModalPromptDialog(blink::WebFrame* frame,
                                    const blink::WebString& message,
                                    const blink::WebString& default_value,
                                    blink::WebString* actual_value);
  virtual bool runModalBeforeUnloadDialog(blink::WebFrame* frame,
                                          bool is_reload,
                                          const blink::WebString& message);
  // DEPRECATED
  virtual bool runModalBeforeUnloadDialog(blink::WebFrame* frame,
                                          const blink::WebString& message);
  virtual void showContextMenu(blink::WebFrame* frame,
                               const blink::WebContextMenuData& data);
  virtual void clearContextMenu();
  virtual void setStatusText(const blink::WebString& text);
  virtual void setMouseOverURL(const blink::WebURL& url);
  virtual void setKeyboardFocusURL(const blink::WebURL& url);
  virtual void startDragging(blink::WebFrame* frame,
                             const blink::WebDragData& data,
                             blink::WebDragOperationsMask mask,
                             const blink::WebImage& image,
                             const blink::WebPoint& imageOffset);
  virtual bool acceptsLoadDrops();
  virtual void focusNext();
  virtual void focusPrevious();
  virtual void focusedNodeChanged(const blink::WebNode& node);
  virtual void numberOfWheelEventHandlersChanged(unsigned num_handlers);
  virtual void didUpdateLayout();
#if defined(OS_ANDROID)
  virtual bool didTapMultipleTargets(
      const blink::WebGestureEvent& event,
      const blink::WebVector<blink::WebRect>& target_rects);
#endif
  virtual void navigateBackForwardSoon(int offset);
  virtual int historyBackListCount();
  virtual int historyForwardListCount();
  virtual void postAccessibilityEvent(
      const blink::WebAXObject& obj, blink::WebAXEvent event);
  virtual void didUpdateInspectorSetting(const blink::WebString& key,
                                         const blink::WebString& value);
  virtual blink::WebGeolocationClient* geolocationClient();
  virtual blink::WebSpeechInputController* speechInputController(
      blink::WebSpeechInputListener* listener);
  virtual blink::WebSpeechRecognizer* speechRecognizer();
  virtual void zoomLimitsChanged(double minimum_level, double maximum_level);
  virtual void zoomLevelChanged();
  virtual double zoomLevelToZoomFactor(double zoom_level) const;
  virtual double zoomFactorToZoomLevel(double factor) const;
  virtual void registerProtocolHandler(const blink::WebString& scheme,
                                       const blink::WebString& base_url,
                                       const blink::WebString& url,
                                       const blink::WebString& title);
  virtual blink::WebPageVisibilityState visibilityState() const;
  virtual blink::WebUserMediaClient* userMediaClient();
  virtual blink::WebMIDIClient* webMIDIClient();
  virtual void draggableRegionsChanged();

#if defined(OS_ANDROID)
  virtual void scheduleContentIntent(const blink::WebURL& intent);
  virtual void cancelScheduledContentIntents();
  virtual blink::WebContentDetectionResult detectContentAround(
      const blink::WebHitTestResult& touch_hit);

  // Only used on Android since all other platforms implement
  // date and time input fields using MULTIPLE_FIELDS_UI
  virtual bool openDateTimeChooser(const blink::WebDateTimeChooserParams&,
                                   blink::WebDateTimeChooserCompletion*);
  virtual void didScrollWithKeyboard(const blink::WebSize& delta);
#endif

  // blink::WebFrameClient implementation -------------------------------------

  virtual blink::WebMediaPlayer* createMediaPlayer(
      blink::WebFrame* frame,
      const blink::WebURL& url,
      blink::WebMediaPlayerClient* client);
  virtual blink::WebCookieJar* cookieJar(blink::WebFrame* frame);
  virtual void didAccessInitialDocument(blink::WebFrame* frame);
  virtual void didDisownOpener(blink::WebFrame* frame);
  virtual void frameDetached(blink::WebFrame* frame);
  virtual void willClose(blink::WebFrame* frame);
  virtual void didMatchCSS(
      blink::WebFrame* frame,
      const blink::WebVector<blink::WebString>& newly_matching_selectors,
      const blink::WebVector<blink::WebString>& stopped_matching_selectors);

  // The WebDataSource::ExtraData* is assumed to be a DocumentState* subclass.
  virtual blink::WebNavigationPolicy decidePolicyForNavigation(
      blink::WebFrame* frame,
      blink::WebDataSource::ExtraData* extraData,
      const blink::WebURLRequest& request,
      blink::WebNavigationType type,
      blink::WebNavigationPolicy default_policy,
      bool is_redirect);
  // DEPRECATED.
  virtual blink::WebNavigationPolicy decidePolicyForNavigation(
      blink::WebFrame* frame,
      const blink::WebURLRequest& request,
      blink::WebNavigationType type,
      blink::WebNavigationPolicy default_policy,
      bool is_redirect);
  virtual void willSendSubmitEvent(blink::WebFrame* frame,
                                   const blink::WebFormElement& form);
  virtual void willSubmitForm(blink::WebFrame* frame,
                              const blink::WebFormElement& form);
  virtual void didCreateDataSource(blink::WebFrame* frame,
                                   blink::WebDataSource* datasource);
  virtual void didStartProvisionalLoad(blink::WebFrame* frame);
  virtual void didReceiveServerRedirectForProvisionalLoad(
      blink::WebFrame* frame);
  virtual void didFailProvisionalLoad(blink::WebFrame* frame,
                                      const blink::WebURLError& error);
  virtual void didCommitProvisionalLoad(blink::WebFrame* frame,
                                        bool is_new_navigation);
  virtual void didClearWindowObject(blink::WebFrame* frame);
  virtual void didCreateDocumentElement(blink::WebFrame* frame);
  virtual void didReceiveTitle(blink::WebFrame* frame,
                               const blink::WebString& title,
                               blink::WebTextDirection direction);
  virtual void didChangeIcon(blink::WebFrame*,
                             blink::WebIconURL::Type);
  virtual void didFinishDocumentLoad(blink::WebFrame* frame);
  virtual void didHandleOnloadEvents(blink::WebFrame* frame);
  virtual void didFailLoad(blink::WebFrame* frame,
                           const blink::WebURLError& error);
  virtual void didFinishLoad(blink::WebFrame* frame);
  virtual void didNavigateWithinPage(blink::WebFrame* frame,
                                     bool is_new_navigation);
  virtual void didUpdateCurrentHistoryItem(blink::WebFrame* frame);
  virtual void willSendRequest(blink::WebFrame* frame,
                               unsigned identifier,
                               blink::WebURLRequest& request,
                               const blink::WebURLResponse& redirect_response);
  virtual void didReceiveResponse(blink::WebFrame* frame,
                                  unsigned identifier,
                                  const blink::WebURLResponse& response);
  virtual void didFinishResourceLoad(blink::WebFrame* frame,
                                     unsigned identifier);
  virtual void didLoadResourceFromMemoryCache(
      blink::WebFrame* frame,
      const blink::WebURLRequest& request,
      const blink::WebURLResponse&);
  virtual void didDisplayInsecureContent(blink::WebFrame* frame);
  virtual void didRunInsecureContent(
      blink::WebFrame* frame,
      const blink::WebSecurityOrigin& origin,
      const blink::WebURL& target);
  virtual void didExhaustMemoryAvailableForScript(blink::WebFrame* frame);
  virtual void didCreateScriptContext(blink::WebFrame* frame,
                                      v8::Handle<v8::Context>,
                                      int extension_group,
                                      int world_id);
  virtual void willReleaseScriptContext(blink::WebFrame* frame,
                                        v8::Handle<v8::Context>,
                                        int world_id);
  virtual void didChangeScrollOffset(blink::WebFrame* frame);
  virtual void willInsertBody(blink::WebFrame* frame);
  virtual void didFirstVisuallyNonEmptyLayout(blink::WebFrame*);
  virtual void didChangeContentsSize(blink::WebFrame* frame,
                                     const blink::WebSize& size);
  virtual void reportFindInPageMatchCount(int request_id,
                                          int count,
                                          bool final_update);
  virtual void reportFindInPageSelection(int request_id,
                                         int active_match_ordinal,
                                         const blink::WebRect& sel);
  virtual void requestStorageQuota(
      blink::WebFrame* frame,
      blink::WebStorageQuotaType type,
      unsigned long long requested_size,
      blink::WebStorageQuotaCallbacks* callbacks);
  virtual void willOpenSocketStream(
      blink::WebSocketStreamHandle* handle);
  virtual void willStartUsingPeerConnectionHandler(blink::WebFrame* frame,
      blink::WebRTCPeerConnectionHandler* handler);
  virtual bool willCheckAndDispatchMessageEvent(
      blink::WebFrame* sourceFrame,
      blink::WebFrame* targetFrame,
      blink::WebSecurityOrigin targetOrigin,
      blink::WebDOMMessageEvent event);
  virtual blink::WebString acceptLanguages();
  virtual blink::WebString userAgentOverride(
      blink::WebFrame* frame,
      const blink::WebURL& url);
  virtual blink::WebString doNotTrackValue(blink::WebFrame* frame);
  virtual bool allowWebGL(blink::WebFrame* frame, bool default_value);
  virtual void didLoseWebGLContext(
      blink::WebFrame* frame,
      int arb_robustness_status_code);

  // blink::WebPageSerializerClient implementation ----------------------------

  virtual void didSerializeDataForFrame(
      const blink::WebURL& frame_url,
      const blink::WebCString& data,
      PageSerializationStatus status) OVERRIDE;

  // RenderView implementation -------------------------------------------------

  virtual bool Send(IPC::Message* message) OVERRIDE;
  virtual int GetRoutingID() const OVERRIDE;
  virtual int GetPageId() const OVERRIDE;
  virtual gfx::Size GetSize() const OVERRIDE;
  virtual WebPreferences& GetWebkitPreferences() OVERRIDE;
  virtual void SetWebkitPreferences(const WebPreferences& preferences) OVERRIDE;
  virtual blink::WebView* GetWebView() OVERRIDE;
  virtual blink::WebNode GetFocusedNode() const OVERRIDE;
  virtual blink::WebNode GetContextMenuNode() const OVERRIDE;
  virtual bool IsEditableNode(const blink::WebNode& node) const OVERRIDE;
  virtual blink::WebPlugin* CreatePlugin(
      blink::WebFrame* frame,
      const WebPluginInfo& info,
      const blink::WebPluginParams& params) OVERRIDE;
  virtual void EvaluateScript(const string16& frame_xpath,
                              const string16& jscript,
                              int id,
                              bool notify_result) OVERRIDE;
  virtual bool ShouldDisplayScrollbars(int width, int height) const OVERRIDE;
  virtual int GetEnabledBindings() const OVERRIDE;
  virtual bool GetContentStateImmediately() const OVERRIDE;
  virtual float GetFilteredTimePerFrame() const OVERRIDE;
  virtual int ShowContextMenu(ContextMenuClient* client,
                              const ContextMenuParams& params) OVERRIDE;
  virtual void CancelContextMenu(int request_id) OVERRIDE;
  virtual blink::WebPageVisibilityState GetVisibilityState() const OVERRIDE;
  virtual void RunModalAlertDialog(blink::WebFrame* frame,
                                   const blink::WebString& message) OVERRIDE;
  virtual void LoadURLExternally(
      blink::WebFrame* frame,
      const blink::WebURLRequest& request,
      blink::WebNavigationPolicy policy) OVERRIDE;
  virtual void DidStartLoading() OVERRIDE;
  virtual void DidStopLoading() OVERRIDE;
  virtual void Repaint(const gfx::Size& size) OVERRIDE;
  virtual void SetEditCommandForNextKeyEvent(const std::string& name,
                                             const std::string& value) OVERRIDE;
  virtual void ClearEditCommands() OVERRIDE;
  virtual SSLStatus GetSSLStatusOfFrame(blink::WebFrame* frame) const OVERRIDE;
  virtual const std::string& GetAcceptLanguages() const OVERRIDE;
#if defined(OS_ANDROID)
  virtual void UpdateTopControlsState(TopControlsState constraints,
                                      TopControlsState current,
                                      bool animate) OVERRIDE;
#endif

  // WebMediaPlayerDelegate implementation -----------------------

  virtual void DidPlay(blink::WebMediaPlayer* player) OVERRIDE;
  virtual void DidPause(blink::WebMediaPlayer* player) OVERRIDE;
  virtual void PlayerGone(blink::WebMediaPlayer* player) OVERRIDE;

  // Please do not add your stuff randomly to the end here. If there is an
  // appropriate section, add it there. If not, there are some random functions
  // nearer to the top you can add it to.

  // Cannot use std::set unfortunately since linked_ptr<> does not support
  // operator<.
  typedef std::vector<linked_ptr<ImageResourceFetcher> >
      ImageResourceFetcherList;

 protected:
  // RenderWidget overrides:
  virtual void Close() OVERRIDE;
  virtual void OnResize(const ViewMsg_Resize_Params& params) OVERRIDE;
  virtual void DidInitiatePaint() OVERRIDE;
  virtual void DidFlushPaint() OVERRIDE;
  virtual PepperPluginInstanceImpl* GetBitmapForOptimizedPluginPaint(
      const gfx::Rect& paint_bounds,
      TransportDIB** dib,
      gfx::Rect* location,
      gfx::Rect* clip,
      float* scale_factor) OVERRIDE;
  virtual gfx::Vector2d GetScrollOffset() OVERRIDE;
  virtual void DidHandleKeyEvent() OVERRIDE;
  virtual bool WillHandleMouseEvent(
      const blink::WebMouseEvent& event) OVERRIDE;
  virtual bool WillHandleKeyEvent(
      const blink::WebKeyboardEvent& event) OVERRIDE;
  virtual bool WillHandleGestureEvent(
      const blink::WebGestureEvent& event) OVERRIDE;
  virtual void DidHandleMouseEvent(const blink::WebMouseEvent& event) OVERRIDE;
  virtual void DidHandleTouchEvent(const blink::WebTouchEvent& event) OVERRIDE;
  virtual bool HasTouchEventHandlersAt(const gfx::Point& point) const OVERRIDE;
  virtual void OnSetFocus(bool enable) OVERRIDE;
  virtual void OnWasHidden() OVERRIDE;
  virtual void OnWasShown(bool needs_repainting) OVERRIDE;
  virtual GURL GetURLForGraphicsContext3D() OVERRIDE;
  virtual bool ForceCompositingModeEnabled() OVERRIDE;
  virtual void OnImeSetComposition(
      const string16& text,
      const std::vector<blink::WebCompositionUnderline>& underlines,
      int selection_start,
      int selection_end) OVERRIDE;
  virtual void OnImeConfirmComposition(const string16& text,
                                       const gfx::Range& replacement_range,
                                       bool keep_selection) OVERRIDE;
  virtual void SetDeviceScaleFactor(float device_scale_factor) OVERRIDE;
  virtual ui::TextInputType GetTextInputType() OVERRIDE;
  virtual void GetSelectionBounds(gfx::Rect* start, gfx::Rect* end) OVERRIDE;
#if defined(OS_MACOSX) || defined(OS_WIN) || defined(USE_AURA)
  virtual void GetCompositionCharacterBounds(
      std::vector<gfx::Rect>* character_bounds) OVERRIDE;
  virtual void GetCompositionRange(gfx::Range* range) OVERRIDE;
#endif
  virtual bool CanComposeInline() OVERRIDE;
  virtual void DidCommitCompositorFrame() OVERRIDE;
  virtual void InstrumentWillBeginFrame() OVERRIDE;
  virtual void InstrumentDidBeginFrame() OVERRIDE;
  virtual void InstrumentDidCancelFrame() OVERRIDE;
  virtual void InstrumentWillComposite() OVERRIDE;
  virtual bool AllowPartialSwap() const OVERRIDE;

 protected:
  explicit RenderViewImpl(RenderViewImplParams* params);

  void Initialize(RenderViewImplParams* params);
  virtual void SetScreenMetricsEmulationParameters(
      float device_scale_factor, float root_layer_scale) OVERRIDE;

  // Do not delete directly.  This class is reference counted.
  virtual ~RenderViewImpl();

 private:
  // For unit tests.
  friend class ExternalPopupMenuTest;
  friend class PepperDeviceTest;
  friend class RendererAccessibilityTest;
  friend class RenderViewTest;

  // TODO(nasko): Temporarily friend RenderFrameImpl, so we don't duplicate
  // utility functions needed in both classes, while we move frame specific
  // code away from this class.
  friend class RenderFrameImpl;

  FRIEND_TEST_ALL_PREFIXES(ExternalPopupMenuRemoveTest, RemoveOnChange);
  FRIEND_TEST_ALL_PREFIXES(ExternalPopupMenuTest, NormalCase);
  FRIEND_TEST_ALL_PREFIXES(ExternalPopupMenuTest, ShowPopupThenNavigate);
  FRIEND_TEST_ALL_PREFIXES(RendererAccessibilityTest,
                           AccessibilityMessagesQueueWhileSwappedOut);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, DecideNavigationPolicyForWebUI);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           DidFailProvisionalLoadWithErrorForError);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           DidFailProvisionalLoadWithErrorForCancellation);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           DontIgnoreBackAfterNavEntryLimit);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, ImeComposition);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, InsertCharacters);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, JSBlockSentAfterPageLoad);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, LastCommittedUpdateState);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, OnExtendSelectionAndDelete);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, OnHandleKeyboardEvent);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, OnImeTypeChanged);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, OnNavStateChanged);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, OnSetTextDirection);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, OnUpdateWebPreferences);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, SendSwapOutACK);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, ReloadWhileSwappedOut);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           SetEditableSelectionAndComposition);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, StaleNavigationsIgnored);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, UpdateTargetURLWithInvalidURL);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           GetCompositionCharacterBoundsTest);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, OnNavigationHttpPost);
#if defined(OS_MACOSX)
  FRIEND_TEST_ALL_PREFIXES(RenderViewTest, MacTestCmdUp);
#endif
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, SetHistoryLengthAndPrune);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, ZoomLimit);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, NavigateFrame);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           ShouldUpdateSelectionTextFromContextMenuParams);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, BasicRenderFrame);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, TextInputTypeWithPepper);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           MessageOrderInDidChangeSelection);
  FRIEND_TEST_ALL_PREFIXES(SuppressErrorPageTest, Suppresses);
  FRIEND_TEST_ALL_PREFIXES(SuppressErrorPageTest, DoesNotSuppress);

  typedef std::map<GURL, double> HostZoomLevels;

  enum ErrorPageType {
    DNS_ERROR,
    HTTP_404,
    CONNECTION_ERROR,
  };

  static blink::WebReferrerPolicy GetReferrerPolicyFromRequest(
      blink::WebFrame* frame,
      const blink::WebURLRequest& request);

  static Referrer GetReferrerFromRequest(
      blink::WebFrame* frame,
      const blink::WebURLRequest& request);

  static webkit_glue::WebURLResponseExtraDataImpl* GetExtraDataFromResponse(
      const blink::WebURLResponse& response);

  void UpdateURL(blink::WebFrame* frame);
  void UpdateTitle(blink::WebFrame* frame, const string16& title,
                   blink::WebTextDirection title_direction);
  void UpdateSessionHistory(blink::WebFrame* frame);
  void SendUpdateState(const blink::WebHistoryItem& item);

  // Update current main frame's encoding and send it to browser window.
  // Since we want to let users see the right encoding info from menu
  // before finishing loading, we call the UpdateEncoding in
  // a) function:DidCommitLoadForFrame. When this function is called,
  // that means we have got first data. In here we try to get encoding
  // of page if it has been specified in http header.
  // b) function:DidReceiveTitle. When this function is called,
  // that means we have got specified title. Because in most of webpages,
  // title tags will follow meta tags. In here we try to get encoding of
  // page if it has been specified in meta tag.
  // c) function:DidFinishDocumentLoadForFrame. When this function is
  // called, that means we have got whole html page. In here we should
  // finally get right encoding of page.
  void UpdateEncoding(blink::WebFrame* frame,
                      const std::string& encoding_name);

  void OpenURL(blink::WebFrame* frame,
               const GURL& url,
               const Referrer& referrer,
               blink::WebNavigationPolicy policy);

  bool RunJavaScriptMessage(JavaScriptMessageType type,
                            const string16& message,
                            const string16& default_value,
                            const GURL& frame_url,
                            string16* result);

  // Sends a message and runs a nested message loop.
  bool SendAndRunNestedMessageLoop(IPC::SyncMessage* message);

  // Called when the "pinned to left/right edge" state needs to be updated.
  void UpdateScrollState(blink::WebFrame* frame);

  // IPC message handlers ------------------------------------------------------
  //
  // The documentation for these functions should be in
  // content/common/*_messages.h for the message that the function is handling.

  void OnCopy();
  void OnCut();
  void OnDelete();
  void OnExecuteEditCommand(const std::string& name, const std::string& value);
  void OnMoveCaret(const gfx::Point& point);
  void OnPaste();
  void OnPasteAndMatchStyle();
  void OnRedo();
  void OnReplace(const string16& text);
  void OnReplaceMisspelling(const string16& text);
  void OnScrollFocusedEditableNodeIntoRect(const gfx::Rect& rect);
  void OnSelectAll();
  void OnSelectRange(const gfx::Point& start, const gfx::Point& end);
  void OnSetEditCommandsForNextKeyEvent(const EditCommands& edit_commands);
  void OnUndo();
  void OnUnselect();
  void OnAllowBindings(int enabled_bindings_flags);
  void OnAllowScriptToClose(bool script_can_close);
  void OnCancelDownload(int32 download_id);
  void OnClearFocusedNode();
  void OnClosePage();
  void OnContextMenuClosed(const CustomContextMenuContext& custom_context);
  void OnShowContextMenu(const gfx::Point& location);
  void OnCopyImageAt(int x, int y);
  void OnCSSInsertRequest(const string16& frame_xpath,
                          const std::string& css);
  void OnCustomContextMenuAction(const CustomContextMenuContext& custom_context,
      unsigned action);
  void OnSetName(const std::string& name);
  void OnDeterminePageLanguage();
  void OnDisableScrollbarsForSmallWindows(
      const gfx::Size& disable_scrollbars_size_limit);
  void OnDragSourceEndedOrMoved(const gfx::Point& client_point,
                                const gfx::Point& screen_point,
                                bool ended,
                                blink::WebDragOperation drag_operation);
  void OnDragSourceSystemDragEnded();
  void OnDragTargetDrop(const gfx::Point& client_pt,
                        const gfx::Point& screen_pt,
                        int key_modifiers);
  void OnDragTargetDragEnter(const DropData& drop_data,
                             const gfx::Point& client_pt,
                             const gfx::Point& screen_pt,
                             blink::WebDragOperationsMask operations_allowed,
                             int key_modifiers);
  void OnDragTargetDragLeave();
  void OnDragTargetDragOver(const gfx::Point& client_pt,
                            const gfx::Point& screen_pt,
                            blink::WebDragOperationsMask operations_allowed,
                            int key_modifiers);
  void OnEnablePreferredSizeChangedMode();
  void OnEnableAutoResize(const gfx::Size& min_size, const gfx::Size& max_size);
  void OnDisableAutoResize(const gfx::Size& new_size);
  void OnEnumerateDirectoryResponse(int id,
                                    const std::vector<base::FilePath>& paths);
  void OnExtendSelectionAndDelete(int before, int after);
  void OnFileChooserResponse(
      const std::vector<ui::SelectedFileInfo>& files);
  void OnFind(int request_id, const string16&, const blink::WebFindOptions&);
  void OnGetAllSavableResourceLinksForCurrentPage(const GURL& page_url);
  void OnGetSerializedHtmlDataForCurrentPageWithLocalLinks(
      const std::vector<GURL>& links,
      const std::vector<base::FilePath>& local_paths,
      const base::FilePath& local_directory_name);
  void OnMediaPlayerActionAt(const gfx::Point& location,
                             const blink::WebMediaPlayerAction& action);
  void OnOrientationChangeEvent(int orientation);
  void OnPluginActionAt(const gfx::Point& location,
                        const blink::WebPluginAction& action);
  void OnMoveOrResizeStarted();
  void OnNavigate(const ViewMsg_Navigate_Params& params);
  void OnPostMessageEvent(const ViewMsg_PostMessage_Params& params);
  void OnReleaseDisambiguationPopupDIB(TransportDIB::Handle dib_handle);
  void OnReloadFrame();
  void OnResetPageEncodingToDefault();
  void OnScriptEvalRequest(const string16& frame_xpath,
                           const string16& jscript,
                           int id,
                           bool notify_result);
  void OnSetAccessibilityMode(AccessibilityMode new_mode);
  void OnSetActive(bool active);
  void OnSetAltErrorPageURL(const GURL& gurl);
  void OnSetBackground(const SkBitmap& background);
  void OnSetCompositionFromExistingText(
      int start, int end,
      const std::vector<blink::WebCompositionUnderline>& underlines);
  void OnExitFullscreen();
  void OnSetEditableSelectionOffsets(int start, int end);
  void OnSetHistoryLengthAndPrune(int history_length, int32 minimum_page_id);
  void OnSetInitialFocus(bool reverse);
  void OnSetPageEncoding(const std::string& encoding_name);
  void OnSetRendererPrefs(const RendererPreferences& renderer_prefs);
  void OnSetWebUIProperty(const std::string& name, const std::string& value);
  void OnSetZoomLevel(double zoom_level);
  void OnSetZoomLevelForLoadingURL(const GURL& url, double zoom_level);
  void OnShouldClose();
  void OnStop();
  void OnStopFinding(StopFindAction action);
  void OnSuppressDialogsUntilSwapOut();
  void OnSwapOut();
  void OnThemeChanged();
  void OnUpdateTargetURLAck();
  void OnUpdateTimezone();
  void OnUpdateWebPreferences(const WebPreferences& prefs);
  void OnZoom(PageZoom zoom);
  void OnZoomFactor(PageZoom zoom, int zoom_center_x, int zoom_center_y);
  void OnEnableViewSourceMode();
  void OnDisownOpener();
  void OnWindowSnapshotCompleted(const int snapshot_id,
      const gfx::Size& size, const std::vector<unsigned char>& png);
#if defined(OS_ANDROID)
  void OnActivateNearestFindResult(int request_id, float x, float y);
  void OnFindMatchRects(int current_version);
  void OnSelectPopupMenuItems(bool canceled,
                              const std::vector<int>& selected_indices);
  void OnUndoScrollFocusedEditableNodeIntoRect();
  void OnUpdateTopControlsState(bool enable_hiding,
                                bool enable_showing,
                                bool animate);
  void OnPauseVideo();

#elif defined(OS_MACOSX)
  void OnCopyToFindPboard();
  void OnPluginImeCompositionCompleted(const string16& text, int plugin_id);
  void OnSelectPopupMenuItem(int selected_index);
  void OnSetInLiveResize(bool in_live_resize);
  void OnSetWindowVisibility(bool visible);
  void OnWindowFrameChanged(const gfx::Rect& window_frame,
                            const gfx::Rect& view_frame);
#endif

  // Adding a new message handler? Please add it in alphabetical order above
  // and put it in the same position in the .cc file.

  // Misc private functions ----------------------------------------------------
  void ZoomFactorHelper(PageZoom zoom, int zoom_center_x, int zoom_center_y,
                        float scaling_increment);

  void AltErrorPageFinished(blink::WebFrame* frame,
                            const blink::WebURLRequest& original_request,
                            const blink::WebURLError& original_error,
                            const std::string& html);

  // Check whether the preferred size has changed.
  void CheckPreferredSize();

  // Initializes |media_stream_client_|, returning true if successful. Returns
  // false if it wasn't possible to create a MediaStreamClient (e.g., WebRTC is
  // disabled) in which case |media_stream_client_| is NULL.
  bool InitializeMediaStreamClient();

  // This callback is triggered when DownloadFavicon completes, either
  // succesfully or with a failure. See DownloadFavicon for more
  // details.
  void DidDownloadFavicon(ImageResourceFetcher* fetcher,
                          const SkBitmap& image);

  // Requests to download a favicon image. When done, the RenderView is notified
  // by way of DidDownloadFavicon. Returns true if the request was successfully
  // started, false otherwise. id is used to uniquely identify the request and
  // passed back to the DidDownloadFavicon method. If the image has multiple
  // frames, the frame whose size is image_size is returned. If the image
  // doesn't have a frame at the specified size, the first is returned.
  bool DownloadFavicon(int id, const GURL& image_url, int image_size);

  GURL GetAlternateErrorPageURL(const GURL& failed_url,
                                ErrorPageType error_type);

  // Locates a sub frame with given xpath
  blink::WebFrame* GetChildFrame(const string16& frame_xpath) const;

  // Returns the URL being loaded by the given frame's request.
  GURL GetLoadingUrl(blink::WebFrame* frame) const;

  // Should only be called if this object wraps a PluginDocument.
  blink::WebPlugin* GetWebPluginFromPluginDocument();

  // Returns true if the |params| navigation is to an entry that has been
  // cropped due to a recent navigation the browser did not know about.
  bool IsBackForwardToStaleEntry(const ViewMsg_Navigate_Params& params,
                                 bool is_reload);

  bool MaybeLoadAlternateErrorPage(blink::WebFrame* frame,
                                   const blink::WebURLError& error,
                                   bool replace);

  // Make this RenderView show an empty, unscriptable page.
  void NavigateToSwappedOutURL(blink::WebFrame* frame);

  // If we initiated a navigation, this function will populate |document_state|
  // with the navigation information saved in OnNavigate().
  void PopulateDocumentStateFromPending(DocumentState* document_state);

  // Returns a new NavigationState populated with the navigation information
  // saved in OnNavigate().
  NavigationState* CreateNavigationStateFromPending();

  // Processes the command-line flags --enable-viewport,
  // --enable-fixed-layout[=w,h] and --enable-pinch.
  void ProcessViewLayoutFlags(const CommandLine& command_line);

#if defined(OS_ANDROID)
  // Launch an Android content intent with the given URL.
  void LaunchAndroidContentIntent(const GURL& intent_url, size_t request_id);

  blink::WebMediaPlayer* CreateAndroidWebMediaPlayer(
      blink::WebFrame* frame,
      const blink::WebURL& url,
      blink::WebMediaPlayerClient* client);
#endif

  blink::WebMediaPlayer* CreateWebMediaPlayerForMediaStream(
      blink::WebFrame* frame,
      const blink::WebURL& url,
      blink::WebMediaPlayerClient* client);

  // Sends a reply to the current find operation handling if it was a
  // synchronous find request.
  void SendFindReply(int request_id,
                     int match_count,
                     int ordinal,
                     const blink::WebRect& selection_rect,
                     bool final_status_update);

  // Returns whether |params.selection_text| should be synchronized to the
  // browser before bringing up the context menu. Static for testing.
  static bool ShouldUpdateSelectionTextFromContextMenuParams(
      const string16& selection_text,
      size_t selection_text_offset,
      const gfx::Range& selection_range,
      const ContextMenuParams& params);

  // Starts nav_state_sync_timer_ if it isn't already running.
  void StartNavStateSyncTimerIfNecessary();

  // Dispatches the current state of selection on the webpage to the browser if
  // it has changed.
  // TODO(varunjain): delete this method once we figure out how to keep
  // selection handles in sync with the webpage.
  void SyncSelectionIfRequired();

#if defined(OS_POSIX) && !defined(OS_MACOSX)
  void UpdateFontRenderingFromRendererPrefs();
#else
  void UpdateFontRenderingFromRendererPrefs() {}
#endif

  // Update the target url and tell the browser that the target URL has changed.
  // If |url| is empty, show |fallback_url|.
  void UpdateTargetURL(const GURL& url, const GURL& fallback_url);

  // Tells the browser what the new list of favicons for the webpage is.
  void SendUpdateFaviconURL(const std::vector<FaviconURL>& urls);

  // Invoked from DidStopLoading(). Sends the current list of loaded favicons to
  // the browser.
  void DidStopLoadingIcons();

  // Coordinate conversion -----------------------------------------------------

  gfx::RectF ClientRectToPhysicalWindowRect(const gfx::RectF& rect) const;

  // Helper for LatencyInfo construction.
  int64 GetLatencyComponentId();

  // RenderFrameImpl accessible state ------------------------------------------
  // The following section is the set of methods that RenderFrameImpl needs
  // to access RenderViewImpl state. The set of state variables are page-level
  // specific, so they don't belong in RenderFrameImpl and should remain in
  // this object.
  ObserverList<RenderViewObserver>& observers() {
    return observers_;
  }

  // TODO(nasko): Remove this method when we move to frame proxy objects, since
  // the concept of swapped out will be eliminated.
  void set_is_swapped_out(bool swapped_out) {
    is_swapped_out_ = swapped_out;
  }

  NavigationGesture navigation_gesture() {
    return navigation_gesture_;
  }
  void set_navigation_gesture(NavigationGesture gesture) {
    navigation_gesture_ = gesture;
  }

  // ---------------------------------------------------------------------------
  // ADDING NEW FUNCTIONS? Please keep private functions alphabetized and put
  // it in the same order in the .cc file as it was in the header.
  // ---------------------------------------------------------------------------

  // Settings ------------------------------------------------------------------

  WebPreferences webkit_preferences_;
  RendererPreferences renderer_preferences_;

  HostZoomLevels host_zoom_levels_;

  // Whether content state (such as form state, scroll position and page
  // contents) should be sent to the browser immediately. This is normally
  // false, but set to true by some tests.
  bool send_content_state_immediately_;

  // Bitwise-ORed set of extra bindings that have been enabled.  See
  // BindingsPolicy for details.
  int enabled_bindings_;

  // The alternate error page URL, if one exists.
  GURL alternate_error_page_url_;

  // If true, we send IPC messages when |preferred_size_| changes.
  bool send_preferred_size_changes_;

  // If non-empty, and |send_preferred_size_changes_| is true, disable drawing
  // scroll bars on windows smaller than this size.  Used for windows that the
  // browser resizes to the size of the content, such as browser action popups.
  // If a render view is set to the minimum size of its content, webkit may add
  // scroll bars.  This makes sense for fixed sized windows, but it does not
  // make sense when the size of the view was chosen to fit the content.
  // This setting ensures that no scroll bars are drawn.  The size limit exists
  // because if the view grows beyond a size known to the browser, scroll bars
  // should be drawn.
  gfx::Size disable_scrollbars_size_limit_;

  // Loading state -------------------------------------------------------------

  // True if the top level frame is currently being loaded.
  bool is_loading_;

  // The gesture that initiated the current navigation.
  NavigationGesture navigation_gesture_;

  // Used for popups.
  bool opened_by_user_gesture_;

  // Whether this RenderView was created by a frame that was suppressing its
  // opener. If so, we may want to load pages in a separate process.  See
  // decidePolicyForNavigation for details.
  bool opener_suppressed_;

  // Whether we must stop creating nested message loops for modal dialogs until
  // OnSwapOut is called.  This is necessary because modal dialogs have a
  // PageGroupLoadDeferrer on the stack that interferes with swapping out.
  bool suppress_dialogs_until_swap_out_;

  // Holds state pertaining to a navigation that we initiated.  This is held by
  // the WebDataSource::ExtraData attribute.  We use pending_navigation_state_
  // as a temporary holder for the state until the WebDataSource corresponding
  // to the new navigation is created.  See DidCreateDataSource.
  scoped_ptr<ViewMsg_Navigate_Params> pending_navigation_params_;

  // Timer used to delay the updating of nav state (see SyncNavigationState).
  base::OneShotTimer<RenderViewImpl> nav_state_sync_timer_;

  // Page IDs ------------------------------------------------------------------
  // See documentation in RenderView.
  int32 page_id_;

  // Indicates the ID of the last page that we sent a FrameNavigate to the
  // browser for. This is used to determine if the most recent transition
  // generated a history entry (less than page_id_), or not (equal to or
  // greater than). Note that this will be greater than page_id_ if the user
  // goes back.
  int32 last_page_id_sent_to_browser_;

  // The next available page ID to use for this RenderView.  These IDs are
  // specific to a given RenderView and the frames within it.
  int32 next_page_id_;

  // The offset of the current item in the history list.
  int history_list_offset_;

  // The RenderView's current impression of the history length.  This includes
  // any items that have committed in this process, but because of cross-process
  // navigations, the history may have some entries that were committed in other
  // processes.  We won't know about them until the next navigation in this
  // process.
  int history_list_length_;

  // The list of page IDs for each history item this RenderView knows about.
  // Some entries may be -1 if they were rendered by other processes or were
  // restored from a previous session.  This lets us detect attempts to
  // navigate to stale entries that have been cropped from our history.
  std::vector<int32> history_page_ids_;

  // Page info -----------------------------------------------------------------

  // The last gotten main frame's encoding.
  std::string last_encoding_name_;

  // UI state ------------------------------------------------------------------

  // The state of our target_url transmissions. When we receive a request to
  // send a URL to the browser, we set this to TARGET_INFLIGHT until an ACK
  // comes back - if a new request comes in before the ACK, we store the new
  // URL in pending_target_url_ and set the status to TARGET_PENDING. If an
  // ACK comes back and we are in TARGET_PENDING, we send the stored URL and
  // revert to TARGET_INFLIGHT.
  //
  // We don't need a queue of URLs to send, as only the latest is useful.
  enum {
    TARGET_NONE,
    TARGET_INFLIGHT,  // We have a request in-flight, waiting for an ACK
    TARGET_PENDING    // INFLIGHT + we have a URL waiting to be sent
  } target_url_status_;

  // The URL we show the user in the status bar. We use this to determine if we
  // want to send a new one (we do not need to send duplicates). It will be
  // equal to either |mouse_over_url_| or |focus_url_|, depending on which was
  // updated last.
  GURL target_url_;

  // The URL the user's mouse is hovering over.
  GURL mouse_over_url_;

  // The URL that has keyboard focus.
  GURL focus_url_;

  // The next target URL we want to send to the browser.
  GURL pending_target_url_;

  // The text selection the last time DidChangeSelection got called. May contain
  // additional characters before and after the selected text, for IMEs. The
  // portion of this string that is the actual selected text starts at index
  // |selection_range_.GetMin() - selection_text_offset_| and has length
  // |selection_range_.length()|.
  string16 selection_text_;
  // The offset corresponding to the start of |selection_text_| in the document.
  size_t selection_text_offset_;
  // Range over the document corresponding to the actual selected text (which
  // could correspond to a substring of |selection_text_|; see above).
  gfx::Range selection_range_;

  // External context menu requests we're waiting for. "Internal"
  // (WebKit-originated) context menu events will have an ID of 0 and will not
  // be in this map.
  //
  // We don't want to add internal ones since some of the "special" page
  // handlers in the browser process just ignore the context menu requests so
  // avoid showing context menus, and so this will cause right clicks to leak
  // entries in this map. Most users of the custom context menu (e.g. Pepper
  // plugins) are normally only on "regular" pages and the regular pages will
  // always respond properly to the request, so we don't have to worry so
  // much about leaks.
  IDMap<ContextMenuClient, IDMapExternalPointer> pending_context_menus_;

#if defined(OS_ANDROID)
  // Cache the old top controls state constraints. Used when updating
  // current value only without altering the constraints.
  cc::TopControlsState top_controls_constraints_;
#endif

  // View ----------------------------------------------------------------------

  // Cache the preferred size of the page in order to prevent sending the IPC
  // when layout() recomputes but doesn't actually change sizes.
  gfx::Size preferred_size_;

  // Used to delay determining the preferred size (to avoid intermediate
  // states for the sizes).
  base::OneShotTimer<RenderViewImpl> check_preferred_size_timer_;

  // These store the "is main frame is scrolled all the way to the left
  // or right" state that was last sent to the browser.
  bool cached_is_main_frame_pinned_to_left_;
  bool cached_is_main_frame_pinned_to_right_;

  // These store the "has scrollbars" state last sent to the browser.
  bool cached_has_main_frame_horizontal_scrollbar_;
  bool cached_has_main_frame_vertical_scrollbar_;

  // Helper objects ------------------------------------------------------------

  scoped_ptr<RenderFrameImpl> main_render_frame_;

  RendererWebCookieJarImpl cookie_jar_;

  // The next group of objects all implement RenderViewObserver, so are deleted
  // along with the RenderView automatically.  This is why we just store
  // weak references.

  // Holds a reference to the service which provides desktop notifications.
  NotificationProvider* notification_provider_;

  // The geolocation dispatcher attached to this view, lazily initialized.
  GeolocationDispatcher* geolocation_dispatcher_;

  // The speech dispatcher attached to this view, lazily initialized.
  InputTagSpeechDispatcher* input_tag_speech_dispatcher_;

  // The speech recognition dispatcher attached to this view, lazily
  // initialized.
  SpeechRecognitionDispatcher* speech_recognition_dispatcher_;

  // Device orientation dispatcher attached to this view; lazily initialized.
  DeviceOrientationDispatcher* device_orientation_dispatcher_;

  // MediaStream dispatcher attached to this view; lazily initialized.
  MediaStreamDispatcher* media_stream_dispatcher_;

  // BrowserPluginManager attached to this view; lazily initialized.
  scoped_refptr<BrowserPluginManager> browser_plugin_manager_;

  // MediaStreamClient attached to this view; lazily initialized.
  MediaStreamClient* media_stream_client_;
  blink::WebUserMediaClient* web_user_media_client_;

  // MIDIClient attached to this view; lazily initialized.
  MIDIDispatcher* midi_dispatcher_;

  DevToolsAgent* devtools_agent_;

  // The current accessibility mode.
  AccessibilityMode accessibility_mode_;

  // Only valid if |accessibility_mode_| is anything other than
  // AccessibilityModeOff.
  RendererAccessibility* renderer_accessibility_;

  // Mouse Lock dispatcher attached to this view.
  MouseLockDispatcher* mouse_lock_dispatcher_;

#if defined(OS_ANDROID)
  // Android Specific ---------------------------------------------------------

  // The background color of the document body element. This is used as the
  // default background color for filling the screen areas for which we don't
  // have the actual content.
  SkColor body_background_color_;

  // Expected id of the next content intent launched. Used to prevent scheduled
  // intents to be launched if aborted.
  size_t expected_content_intent_id_;

  // List of click-based content detectors.
  typedef std::vector< linked_ptr<ContentDetector> > ContentDetectorList;
  ContentDetectorList content_detectors_;

  // The media player manager for managing all the media players on this view
  // for communicating with the real media player objects in browser process.
  RendererMediaPlayerManager* media_player_manager_;

  // A date/time picker object for date and time related input elements.
  scoped_ptr<RendererDateTimePicker> date_time_picker_client_;
#endif

  // Plugins -------------------------------------------------------------------

  // All the currently active plugin delegates for this RenderView; kept so
  // that we can enumerate them to send updates about things like window
  // location or tab focus and visibily. These are non-owning references.
  std::set<WebPluginDelegateProxy*> plugin_delegates_;

#if defined(OS_WIN)
  // The ID of the focused NPAPI plug-in.
  int focused_plugin_id_;
#endif

#if defined(ENABLE_PLUGINS)
  typedef std::set<PepperPluginInstanceImpl*> PepperPluginSet;
  PepperPluginSet active_pepper_instances_;

  // Whether or not the focus is on a PPAPI plugin
  PepperPluginInstanceImpl* focused_pepper_plugin_;

  // Current text input composition text. Empty if no composition is in
  // progress.
  string16 pepper_composition_text_;

  // The plugin instance that received the last mouse event. It is set to NULL
  // if the last mouse event went to elements other than Pepper plugins.
  // |pepper_last_mouse_event_target_| is not owned by this class. We can know
  // about when it is destroyed via InstanceDeleted().
  PepperPluginInstanceImpl* pepper_last_mouse_event_target_;
#endif

  // Misc ----------------------------------------------------------------------

  // The current and pending file chooser completion objects. If the queue is
  // nonempty, the first item represents the currently running file chooser
  // callback, and the remaining elements are the other file chooser completion
  // still waiting to be run (in order).
  struct PendingFileChooser;
  std::deque< linked_ptr<PendingFileChooser> > file_chooser_completions_;

  // The current directory enumeration callback
  std::map<int, blink::WebFileChooserCompletion*> enumeration_completions_;
  int enumeration_completion_id_;

  // Reports load progress to the browser.
  scoped_ptr<LoadProgressTracker> load_progress_tracker_;

  // The SessionStorage namespace that we're assigned to has an ID, and that ID
  // is passed to us upon creation.  WebKit asks for this ID upon first use and
  // uses it whenever asking the browser process to allocate new storage areas.
  int64 session_storage_namespace_id_;

  // Stores edit commands associated to the next key event.
  // Shall be cleared as soon as the next key event is processed.
  EditCommands edit_commands_;

  // The external popup for the currently showing select popup.
  scoped_ptr<ExternalPopupMenu> external_popup_menu_;

  // The node that the context menu was pressed over.
  blink::WebNode context_menu_node_;

  // All the registered observers.  We expect this list to be small, so vector
  // is fine.
  ObserverList<RenderViewObserver> observers_;

  // Used to inform didChangeSelection() when it is called in the context
  // of handling a InputMsg_SelectRange IPC.
  bool handling_select_range_;

  // Wraps the |webwidget_| as a MouseLockDispatcher::LockTarget interface.
  scoped_ptr<MouseLockDispatcher::LockTarget> webwidget_mouse_lock_target_;

  // State associated with the GetWindowSnapshot function.
  int next_snapshot_id_;
  typedef std::map<int, WindowSnapshotCallback> PendingSnapshotMap;
  PendingSnapshotMap pending_snapshots_;

  // Allows to selectively disable partial buffer swap for this renderer's
  // compositor.
  bool allow_partial_swap_;

  // Allows JS to access DOM automation. The JS object is only exposed when the
  // DOM automation bindings are enabled.
  scoped_ptr<DomAutomationController> dom_automation_controller_;

  // Allows JS to read out a variety of internal various metrics. The JS object
  // is only exposed when the stats collection bindings are enabled.
  scoped_ptr<StatsCollectionController> stats_collection_controller_;

  // This field stores drag/drop related info for the event that is currently
  // being handled. If the current event results in starting a drag/drop
  // session, this info is sent to the browser along with other drag/drop info.
  DragEventSourceInfo possible_drag_event_info_;

  // NOTE: stats_collection_observer_ should be the last members because their
  // constructors call the AddObservers method of RenderViewImpl.
  scoped_ptr<StatsCollectionObserver> stats_collection_observer_;

  ui::MenuSourceType context_menu_source_type_;
  gfx::Point touch_editing_context_menu_location_;

  // ---------------------------------------------------------------------------
  // ADDING NEW DATA? Please see if it fits appropriately in one of the above
  // sections rather than throwing it randomly at the end. If you're adding a
  // bunch of stuff, you should probably create a helper class and put your
  // data and methods on that to avoid bloating RenderView more.  You can
  // use the Observer interface to filter IPC messages and receive frame change
  // notifications.
  // ---------------------------------------------------------------------------

  DISALLOW_COPY_AND_ASSIGN(RenderViewImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_VIEW_IMPL_H_
