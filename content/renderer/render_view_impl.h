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
#include "base/process.h"
#include "base/timer.h"
#include "base/values.h"
#include "build/build_config.h"
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
#include "content/public/renderer/render_view.h"
#include "content/renderer/mouse_lock_dispatcher.h"
#include "content/renderer/render_view_pepper_helper.h"
#include "content/renderer/render_widget.h"
#include "content/renderer/renderer_webcookiejar_impl.h"
#include "ipc/ipc_platform_file.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFileSystem.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebConsoleMessage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrameClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebHistoryItem.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIconURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNavigationType.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPageSerializerClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPageVisibilityState.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebViewClient.h"
#include "ui/surface/transport_dib.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/media/webmediaplayer_delegate.h"
#include "webkit/plugins/npapi/webplugin_page_delegate.h"

#if defined(OS_ANDROID)
#include "content/renderer/android/content_detector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebContentDetectionResult.h"
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
struct ViewMsg_SwapOut_Params;
struct WebDropData;

namespace ui {
struct SelectedFileInfo;
}  // namespace ui

namespace webkit {

namespace ppapi {
class PluginInstance;
}  // namespace ppapi

}  // namespace webkit

namespace webkit_glue {
class ImageResourceFetcher;
class ResourceFetcher;
}

#if defined(OS_ANDROID)
namespace webkit_media {
class MediaPlayerBridgeManagerImpl;
class WebMediaPlayerManagerAndroid;
}
#endif

namespace WebKit {
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

namespace content {
class BrowserPluginManager;
class DeviceOrientationDispatcher;
class DevToolsAgent;
class DocumentState;
class DomAutomationController;
class ExternalPopupMenu;
class FaviconHelper;
class GeolocationDispatcher;
class InputTagSpeechDispatcher;
class JavaBridgeDispatcher;
class LoadProgressTracker;
class MediaStreamDispatcher;
class MediaStreamImpl;
class MouseLockDispatcher;
class NavigationState;
class NotificationProvider;
class RenderViewObserver;
class RenderViewTest;
class RendererAccessibility;
class RendererDateTimePicker;
class RendererPpapiHost;
class RendererWebColorChooserImpl;
class RenderWidgetFullscreenPepper;
class SpeechRecognitionDispatcher;
class WebPluginDelegateProxy;
struct CustomContextMenuContext;
struct FileChooserParams;
struct RenderViewImplParams;

#if defined(OS_ANDROID)
class WebMediaPlayerProxyImplAndroid;
#endif

// We need to prevent a page from trying to create infinite popups. It is not
// as simple as keeping a count of the number of immediate children
// popups. Having an html file that window.open()s itself would create
// an unlimited chain of RenderViews who only have one RenderView child.
//
// Therefore, each new top level RenderView creates a new counter and shares it
// with all its children and grandchildren popup RenderViewImpls created with
// createView() to have a sort of global limit for the page so no more than
// kMaximumNumberOfPopups popups are created.
//
// This is a RefCounted holder of an int because I can't say
// scoped_refptr<int>.
typedef base::RefCountedData<int> SharedRenderViewCounter;

//
// RenderView is an object that manages a WebView object, and provides a
// communication interface with an embedding application process
//
class CONTENT_EXPORT RenderViewImpl
    : public RenderWidget,
      NON_EXPORTED_BASE(public WebKit::WebViewClient),
      NON_EXPORTED_BASE(public WebKit::WebFrameClient),
      NON_EXPORTED_BASE(public WebKit::WebPageSerializerClient),
      public RenderView,
      NON_EXPORTED_BASE(public webkit::npapi::WebPluginPageDelegate),
      NON_EXPORTED_BASE(public webkit_media::WebMediaPlayerDelegate),
      public base::SupportsWeakPtr<RenderViewImpl> {
 public:
  // Creates a new RenderView. If this is a blocked popup or as a new tab,
  // opener_id is the routing ID of the RenderView responsible for creating this
  // RenderView. |counter| is either a currently initialized counter, or NULL
  // (in which case we treat this RenderView as a top level window).
  static RenderViewImpl* Create(
      int32 opener_id,
      const RendererPreferences& renderer_prefs,
      const webkit_glue::WebPreferences& webkit_prefs,
      SharedRenderViewCounter* counter,
      int32 routing_id,
      int32 surface_id,
      int64 session_storage_namespace_id,
      const string16& frame_name,
      bool is_renderer_created,
      bool swapped_out,
      int32 next_page_id,
      const WebKit::WebScreenInfo& screen_info,
      AccessibilityMode accessibility_mode,
      bool allow_partial_swap);

  // Used by content_layouttest_support to hook into the creation of
  // RenderViewImpls.
  static void InstallCreateHook(
      RenderViewImpl* (*create_render_view_impl)(RenderViewImplParams*));

  // Returns the RenderViewImpl containing the given WebView.
  static RenderViewImpl* FromWebView(WebKit::WebView* webview);

  // Returns the RenderViewImpl for the given routing ID.
  static RenderViewImpl* FromRoutingID(int routing_id);

  // May return NULL when the view is closing.
  WebKit::WebView* webview() const;

  int history_list_offset() const { return history_list_offset_; }

  const webkit_glue::WebPreferences& webkit_preferences() const {
    return webkit_preferences_;
  }

  bool enable_do_not_track() const {
    return renderer_preferences_.enable_do_not_track;
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

#if defined(OS_ANDROID)
  webkit_media::WebMediaPlayerManagerAndroid* media_player_manager() {
    return media_player_manager_.get();
  }
#endif

  // Lazily initialize this view's BrowserPluginManager and return it.
  BrowserPluginManager* browser_plugin_manager();

  // Functions to add and remove observers for this object.
  void AddObserver(RenderViewObserver* observer);
  void RemoveObserver(RenderViewObserver* observer);

  // Adds the given file chooser request to the file_chooser_completion_ queue
  // (see that var for more) and requests the chooser be displayed if there are
  // no other waiting items in the queue.
  //
  // Returns true if the chooser was successfully scheduled. False means we
  // didn't schedule anything.
  bool ScheduleFileChooser(const FileChooserParams& params,
                           WebKit::WebFileChooserCompletion* completion);

  void LoadNavigationErrorPage(
      WebKit::WebFrame* frame,
      const WebKit::WebURLRequest& failed_request,
      const WebKit::WebURLError& error,
      const std::string& html,
      bool replace);

  // Plugin-related functions --------------------------------------------------
  // (See also WebPluginPageDelegate implementation.)

  // Notification that the given plugin has crashed.
  void PluginCrashed(const base::FilePath& plugin_path,
                     base::ProcessId plugin_pid);

  // Creates a fullscreen container for a pepper plugin instance.
  RenderWidgetFullscreenPepper* CreatePepperFullscreenContainer(
      webkit::ppapi::PluginInstance* plugin);

  // Informs the render view that a PPAPI plugin has gained or lost focus.
  void PpapiPluginFocusChanged();

  // Informs the render view that a PPAPI plugin has changed text input status.
  void PpapiPluginTextInputTypeChanged();
  void PpapiPluginCaretPositionChanged();

  // Cancels current composition.
  void PpapiPluginCancelComposition();

  // Informs the render view that a PPAPI plugin has changed selection.
  void PpapiPluginSelectionChanged();

  // Notification that a PPAPI plugin has been created.
  void PpapiPluginCreated(RendererPpapiHost* host);

  // Retrieves the current caret position if a PPAPI plugin has focus.
  bool GetPpapiPluginCaretBounds(gfx::Rect* rect);

  // Simulates IME events for testing purpose.
  void SimulateImeSetComposition(
      const string16& text,
      const std::vector<WebKit::WebCompositionUnderline>& underlines,
      int selection_start,
      int selection_end);
  void SimulateImeConfirmComposition(const string16& text,
                                     const ui::Range& replacement_range);

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
                     webkit::WebPluginInfo* plugin_info,
                     std::string* actual_mime_type);

  void TransferActiveWheelFlingAnimation(
      const WebKit::WebActiveWheelFlingParameters& params);

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

  // IPC::Listener implementation ----------------------------------------------

  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  // WebKit::WebWidgetClient implementation ------------------------------------

  // Most methods are handled by RenderWidget.
  virtual void didFocus();
  virtual void didBlur();
  virtual void show(WebKit::WebNavigationPolicy policy);
  virtual void runModal();
  virtual bool enterFullScreen();
  virtual void exitFullScreen();
  virtual bool requestPointerLock();
  virtual void requestPointerUnlock();
  virtual bool isPointerLocked();
  virtual void didActivateCompositor(int input_handler_identifier);
  virtual void didHandleGestureEvent(const WebKit::WebGestureEvent& event,
                                     bool event_cancelled) OVERRIDE;

  // WebKit::WebViewClient implementation --------------------------------------

  virtual WebKit::WebView* createView(
      WebKit::WebFrame* creator,
      const WebKit::WebURLRequest& request,
      const WebKit::WebWindowFeatures& features,
      const WebKit::WebString& frame_name,
      WebKit::WebNavigationPolicy policy);
  virtual WebKit::WebWidget* createPopupMenu(WebKit::WebPopupType popup_type);
  virtual WebKit::WebExternalPopupMenu* createExternalPopupMenu(
      const WebKit::WebPopupMenuInfo& popup_menu_info,
      WebKit::WebExternalPopupMenuClient* popup_menu_client);
  virtual WebKit::WebStorageNamespace* createSessionStorageNamespace(
      unsigned quota);
  virtual void didAddMessageToConsole(
      const WebKit::WebConsoleMessage& message,
      const WebKit::WebString& source_name,
      unsigned source_line);
  virtual void printPage(WebKit::WebFrame* frame);
  virtual WebKit::WebNotificationPresenter* notificationPresenter();
  virtual bool enumerateChosenDirectory(
      const WebKit::WebString& path,
      WebKit::WebFileChooserCompletion* chooser_completion);
  virtual void initializeHelperPluginWebFrame(WebKit::WebHelperPlugin*);
  virtual void didStartLoading();
  virtual void didStopLoading();
  virtual void didChangeLoadProgress(WebKit::WebFrame* frame,
                                     double load_progress);
  virtual void didCancelCompositionOnSelectionChange();
  virtual void didChangeSelection(bool is_selection_empty);
  virtual void didExecuteCommand(const WebKit::WebString& command_name);
  virtual bool handleCurrentKeyboardEvent();
  virtual WebKit::WebColorChooser* createColorChooser(
      WebKit::WebColorChooserClient*, const WebKit::WebColor& initial_color);
  virtual bool runFileChooser(
      const WebKit::WebFileChooserParams& params,
      WebKit::WebFileChooserCompletion* chooser_completion);
  virtual void runModalAlertDialog(WebKit::WebFrame* frame,
                                   const WebKit::WebString& message);
  virtual bool runModalConfirmDialog(WebKit::WebFrame* frame,
                                     const WebKit::WebString& message);
  virtual bool runModalPromptDialog(WebKit::WebFrame* frame,
                                    const WebKit::WebString& message,
                                    const WebKit::WebString& default_value,
                                    WebKit::WebString* actual_value);
  virtual bool runModalBeforeUnloadDialog(WebKit::WebFrame* frame,
                                          const WebKit::WebString& message);
  virtual void showContextMenu(WebKit::WebFrame* frame,
                               const WebKit::WebContextMenuData& data);
  virtual void setStatusText(const WebKit::WebString& text);
  virtual void setMouseOverURL(const WebKit::WebURL& url);
  virtual void setKeyboardFocusURL(const WebKit::WebURL& url);
  virtual void startDragging(WebKit::WebFrame* frame,
                             const WebKit::WebDragData& data,
                             WebKit::WebDragOperationsMask mask,
                             const WebKit::WebImage& image,
                             const WebKit::WebPoint& imageOffset);
  virtual bool acceptsLoadDrops();
  virtual void focusNext();
  virtual void focusPrevious();
  virtual void focusedNodeChanged(const WebKit::WebNode& node);
  virtual void numberOfWheelEventHandlersChanged(unsigned num_handlers);
  virtual void hasTouchEventHandlers(bool has_handlers);
  virtual void didUpdateLayout();
#if defined(OS_ANDROID)
  virtual bool didTapMultipleTargets(
      const WebKit::WebGestureEvent& event,
      const WebKit::WebVector<WebKit::WebRect>& target_rects);
#endif
  virtual void navigateBackForwardSoon(int offset);
  virtual int historyBackListCount();
  virtual int historyForwardListCount();
  virtual void postAccessibilityNotification(
      const WebKit::WebAccessibilityObject& obj,
      WebKit::WebAccessibilityNotification notification);
  virtual void didUpdateInspectorSetting(const WebKit::WebString& key,
                                         const WebKit::WebString& value);
  virtual WebKit::WebGeolocationClient* geolocationClient();
  virtual WebKit::WebSpeechInputController* speechInputController(
      WebKit::WebSpeechInputListener* listener);
  virtual WebKit::WebSpeechRecognizer* speechRecognizer();
  virtual WebKit::WebDeviceOrientationClient* deviceOrientationClient();
  virtual void zoomLimitsChanged(double minimum_level, double maximum_level);
  virtual void zoomLevelChanged();
  virtual void registerProtocolHandler(const WebKit::WebString& scheme,
                                       const WebKit::WebString& base_url,
                                       const WebKit::WebString& url,
                                       const WebKit::WebString& title);
  virtual WebKit::WebPageVisibilityState visibilityState() const;
  virtual WebKit::WebUserMediaClient* userMediaClient();
  virtual void draggableRegionsChanged();

#if defined(OS_ANDROID)
  virtual void scheduleContentIntent(const WebKit::WebURL& intent);
  virtual void cancelScheduledContentIntents();
  virtual WebKit::WebContentDetectionResult detectContentAround(
      const WebKit::WebHitTestResult& touch_hit);

  // Only used on Android since all other platforms implement
  // date and time input fields using MULTIPLE_FIELDS_UI
  virtual bool openDateTimeChooser(const WebKit::WebDateTimeChooserParams&,
                                   WebKit::WebDateTimeChooserCompletion*);
#endif

  // WebKit::WebFrameClient implementation -------------------------------------

  virtual WebKit::WebPlugin* createPlugin(
      WebKit::WebFrame* frame,
      const WebKit::WebPluginParams& params);
  virtual WebKit::WebSharedWorker* createSharedWorker(
      WebKit::WebFrame* frame, const WebKit::WebURL& url,
      const WebKit::WebString& name, unsigned long long documentId);
  virtual WebKit::WebMediaPlayer* createMediaPlayer(
      WebKit::WebFrame* frame,
      const WebKit::WebURL& url,
      WebKit::WebMediaPlayerClient* client);
  virtual WebKit::WebApplicationCacheHost* createApplicationCacheHost(
      WebKit::WebFrame* frame,
      WebKit::WebApplicationCacheHostClient* client);
  virtual WebKit::WebCookieJar* cookieJar(WebKit::WebFrame* frame);
  virtual void didCreateFrame(WebKit::WebFrame* parent,
                              WebKit::WebFrame* child);
  virtual void didDisownOpener(WebKit::WebFrame* frame);
  virtual void frameDetached(WebKit::WebFrame* frame);
  virtual void willClose(WebKit::WebFrame* frame);
  virtual void didChangeName(WebKit::WebFrame* frame,
                             const WebKit::WebString& name);
  virtual void loadURLExternally(WebKit::WebFrame* frame,
                                 const WebKit::WebURLRequest& request,
                                 WebKit::WebNavigationPolicy policy);
  virtual void loadURLExternally(WebKit::WebFrame* frame,
                                 const WebKit::WebURLRequest& request,
                                 WebKit::WebNavigationPolicy policy,
                                 const WebKit::WebString& suggested_name);
  virtual WebKit::WebNavigationPolicy decidePolicyForNavigation(
      WebKit::WebFrame* frame,
      const WebKit::WebURLRequest& request,
      WebKit::WebNavigationType type,
      const WebKit::WebNode&,
      WebKit::WebNavigationPolicy default_policy,
      bool is_redirect);
  virtual bool canHandleRequest(WebKit::WebFrame* frame,
                                const WebKit::WebURLRequest& request);
  virtual WebKit::WebURLError cannotHandleRequestError(
      WebKit::WebFrame* frame,
      const WebKit::WebURLRequest& request);
  virtual WebKit::WebURLError cancelledError(
      WebKit::WebFrame* frame,
      const WebKit::WebURLRequest& request);
  virtual void unableToImplementPolicyWithError(
      WebKit::WebFrame* frame,
      const WebKit::WebURLError& error);
  virtual void willSendSubmitEvent(WebKit::WebFrame* frame,
                                   const WebKit::WebFormElement& form);
  virtual void willSubmitForm(WebKit::WebFrame* frame,
                              const WebKit::WebFormElement& form);
  virtual void willPerformClientRedirect(WebKit::WebFrame* frame,
                                         const WebKit::WebURL& from,
                                         const WebKit::WebURL& to,
                                         double interval,
                                         double fire_time);
  virtual void didCancelClientRedirect(WebKit::WebFrame* frame);
  virtual void didCompleteClientRedirect(WebKit::WebFrame* frame,
                                         const WebKit::WebURL& from);
  virtual void didCreateDataSource(WebKit::WebFrame* frame,
                                   WebKit::WebDataSource* datasource);
  virtual void didStartProvisionalLoad(WebKit::WebFrame* frame);
  virtual void didReceiveServerRedirectForProvisionalLoad(
      WebKit::WebFrame* frame);
  virtual void didFailProvisionalLoad(WebKit::WebFrame* frame,
                                      const WebKit::WebURLError& error);
  virtual void didReceiveDocumentData(WebKit::WebFrame* frame,
                                      const char* data, size_t length,
                                      bool& prevent_default);
  virtual void didCommitProvisionalLoad(WebKit::WebFrame* frame,
                                        bool is_new_navigation);
  virtual void didClearWindowObject(WebKit::WebFrame* frame);
  virtual void didCreateDocumentElement(WebKit::WebFrame* frame);
  virtual void didReceiveTitle(WebKit::WebFrame* frame,
                               const WebKit::WebString& title,
                               WebKit::WebTextDirection direction);
  virtual void didChangeIcon(WebKit::WebFrame*,
                             WebKit::WebIconURL::Type) OVERRIDE;
  virtual void didFinishDocumentLoad(WebKit::WebFrame* frame);
  virtual void didHandleOnloadEvents(WebKit::WebFrame* frame);
  virtual void didFailLoad(WebKit::WebFrame* frame,
                           const WebKit::WebURLError& error);
  virtual void didFinishLoad(WebKit::WebFrame* frame);
  virtual void didNavigateWithinPage(WebKit::WebFrame* frame,
                                     bool is_new_navigation);
  virtual void didUpdateCurrentHistoryItem(WebKit::WebFrame* frame);
  virtual void assignIdentifierToRequest(WebKit::WebFrame* frame,
                                         unsigned identifier,
                                         const WebKit::WebURLRequest& request);
  virtual void willSendRequest(WebKit::WebFrame* frame,
                               unsigned identifier,
                               WebKit::WebURLRequest& request,
                               const WebKit::WebURLResponse& redirect_response);
  virtual void didReceiveResponse(WebKit::WebFrame* frame,
                                  unsigned identifier,
                                  const WebKit::WebURLResponse& response);
  virtual void didFinishResourceLoad(WebKit::WebFrame* frame,
                                     unsigned identifier);
  virtual void didFailResourceLoad(WebKit::WebFrame* frame,
                                   unsigned identifier,
                                   const WebKit::WebURLError& error);
  virtual void didLoadResourceFromMemoryCache(
      WebKit::WebFrame* frame,
      const WebKit::WebURLRequest& request,
      const WebKit::WebURLResponse&);
  virtual void didDisplayInsecureContent(WebKit::WebFrame* frame);
  virtual void didRunInsecureContent(
      WebKit::WebFrame* frame,
      const WebKit::WebSecurityOrigin& origin,
      const WebKit::WebURL& target);
  virtual void didExhaustMemoryAvailableForScript(WebKit::WebFrame* frame);
  virtual void didCreateScriptContext(WebKit::WebFrame* frame,
                                      v8::Handle<v8::Context>,
                                      int extension_group,
                                      int world_id);
  virtual void willReleaseScriptContext(WebKit::WebFrame* frame,
                                        v8::Handle<v8::Context>,
                                        int world_id);
  virtual void didChangeScrollOffset(WebKit::WebFrame* frame);
  virtual void willInsertBody(WebKit::WebFrame* frame) OVERRIDE;
#if defined(OS_ANDROID)
  virtual void didFirstVisuallyNonEmptyLayout(WebKit::WebFrame*) OVERRIDE;
#endif
  virtual void didChangeContentsSize(WebKit::WebFrame* frame,
                                     const WebKit::WebSize& size);
  virtual void reportFindInPageMatchCount(int request_id,
                                          int count,
                                          bool final_update);
  virtual void reportFindInPageSelection(int request_id,
                                         int active_match_ordinal,
                                         const WebKit::WebRect& sel);
  virtual void openFileSystem(WebKit::WebFrame* frame,
                              WebKit::WebFileSystemType type,
                              long long size,
                              bool create,
                              WebKit::WebFileSystemCallbacks* callbacks);
  virtual void deleteFileSystem(WebKit::WebFrame* frame,
                                WebKit::WebFileSystemType type,
                                WebKit::WebFileSystemCallbacks* callbacks);
  virtual void queryStorageUsageAndQuota(
      WebKit::WebFrame* frame,
      WebKit::WebStorageQuotaType type,
      WebKit::WebStorageQuotaCallbacks* callbacks);
  virtual void requestStorageQuota(
      WebKit::WebFrame* frame,
      WebKit::WebStorageQuotaType type,
      unsigned long long requested_size,
      WebKit::WebStorageQuotaCallbacks* callbacks);
  virtual void willOpenSocketStream(
      WebKit::WebSocketStreamHandle* handle);
  virtual void willStartUsingPeerConnectionHandler(WebKit::WebFrame* frame,
      WebKit::WebRTCPeerConnectionHandler* handler) OVERRIDE;
  virtual bool willCheckAndDispatchMessageEvent(
      WebKit::WebFrame* sourceFrame,
      WebKit::WebFrame* targetFrame,
      WebKit::WebSecurityOrigin targetOrigin,
      WebKit::WebDOMMessageEvent event) OVERRIDE;
  virtual WebKit::WebString userAgentOverride(
      WebKit::WebFrame* frame,
      const WebKit::WebURL& url) OVERRIDE;
  virtual bool allowWebGL(WebKit::WebFrame* frame, bool default_value) OVERRIDE;
  virtual void didLoseWebGLContext(
      WebKit::WebFrame* frame,
      int arb_robustness_status_code) OVERRIDE;

  // WebKit::WebPageSerializerClient implementation ----------------------------

  virtual void didSerializeDataForFrame(
      const WebKit::WebURL& frame_url,
      const WebKit::WebCString& data,
      PageSerializationStatus status) OVERRIDE;

  // RenderView implementation -------------------------------------------------

  virtual bool Send(IPC::Message* message) OVERRIDE;
  virtual int GetRoutingID() const OVERRIDE;
  virtual int GetPageId() const OVERRIDE;
  virtual gfx::Size GetSize() const OVERRIDE;
  virtual webkit_glue::WebPreferences& GetWebkitPreferences() OVERRIDE;
  virtual void SetWebkitPreferences(
      const webkit_glue::WebPreferences& preferences) OVERRIDE;
  virtual WebKit::WebView* GetWebView() OVERRIDE;
  virtual WebKit::WebNode GetFocusedNode() const OVERRIDE;
  virtual WebKit::WebNode GetContextMenuNode() const OVERRIDE;
  virtual bool IsEditableNode(const WebKit::WebNode& node) const OVERRIDE;
  virtual WebKit::WebPlugin* CreatePlugin(
      WebKit::WebFrame* frame,
      const webkit::WebPluginInfo& info,
      const WebKit::WebPluginParams& params) OVERRIDE;
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
  virtual WebKit::WebPageVisibilityState GetVisibilityState() const OVERRIDE;
  virtual void RunModalAlertDialog(WebKit::WebFrame* frame,
                                   const WebKit::WebString& message) OVERRIDE;
  virtual void LoadURLExternally(
      WebKit::WebFrame* frame,
      const WebKit::WebURLRequest& request,
      WebKit::WebNavigationPolicy policy) OVERRIDE;
  virtual void Repaint(const gfx::Size& size) OVERRIDE;
  virtual void SetEditCommandForNextKeyEvent(const std::string& name,
                                             const std::string& value) OVERRIDE;
  virtual void ClearEditCommands() OVERRIDE;
  virtual SSLStatus GetSSLStatusOfFrame(WebKit::WebFrame* frame) const OVERRIDE;
#if defined(OS_ANDROID)
  virtual skia::RefPtr<SkPicture> CapturePicture() OVERRIDE;
#endif

  // webkit_glue::WebPluginPageDelegate implementation -------------------------

  virtual webkit::npapi::WebPluginDelegate* CreatePluginDelegate(
      const base::FilePath& file_path,
      const std::string& mime_type) OVERRIDE;
  virtual WebKit::WebPlugin* CreatePluginReplacement(
      const base::FilePath& file_path) OVERRIDE;
  virtual void CreatedPluginWindow(gfx::PluginWindowHandle handle) OVERRIDE;
  virtual void WillDestroyPluginWindow(gfx::PluginWindowHandle handle) OVERRIDE;
  virtual void DidMovePlugin(
      const webkit::npapi::WebPluginGeometry& move) OVERRIDE;
  virtual void DidStartLoadingForPlugin() OVERRIDE;
  virtual void DidStopLoadingForPlugin() OVERRIDE;
  virtual WebKit::WebCookieJar* GetCookieJar() OVERRIDE;

  // webkit_media::WebMediaPlayerDelegate implementation -----------------------

  virtual void DidPlay(WebKit::WebMediaPlayer* player) OVERRIDE;
  virtual void DidPause(WebKit::WebMediaPlayer* player) OVERRIDE;
  virtual void PlayerGone(WebKit::WebMediaPlayer* player) OVERRIDE;

  // Please do not add your stuff randomly to the end here. If there is an
  // appropriate section, add it there. If not, there are some random functions
  // nearer to the top you can add it to.

  // Cannot use std::set unfortunately since linked_ptr<> does not support
  // operator<.
  typedef std::vector<linked_ptr<webkit_glue::ImageResourceFetcher> >
      ImageResourceFetcherList;

 protected:
  // RenderWidget overrides:
  virtual void Close() OVERRIDE;
  virtual void OnResize(const gfx::Size& new_size,
                        const gfx::Size& physical_backing_size,
                        const gfx::Rect& resizer_rect,
                        bool is_fullscreen) OVERRIDE;
  virtual void WillInitiatePaint() OVERRIDE;
  virtual void DidInitiatePaint() OVERRIDE;
  virtual void DidFlushPaint() OVERRIDE;
  virtual webkit::ppapi::PluginInstance* GetBitmapForOptimizedPluginPaint(
      const gfx::Rect& paint_bounds,
      TransportDIB** dib,
      gfx::Rect* location,
      gfx::Rect* clip,
      float* scale_factor) OVERRIDE;
  virtual gfx::Vector2d GetScrollOffset() OVERRIDE;
  virtual void DidHandleKeyEvent() OVERRIDE;
  virtual bool WillHandleMouseEvent(
      const WebKit::WebMouseEvent& event) OVERRIDE;
  virtual bool WillHandleGestureEvent(
      const WebKit::WebGestureEvent& event) OVERRIDE;
  virtual void DidHandleMouseEvent(const WebKit::WebMouseEvent& event) OVERRIDE;
  virtual void DidHandleTouchEvent(const WebKit::WebTouchEvent& event) OVERRIDE;
  virtual bool HasTouchEventHandlersAt(const gfx::Point& point) const OVERRIDE;
  virtual void OnSetFocus(bool enable) OVERRIDE;
  virtual void OnWasHidden() OVERRIDE;
  virtual void OnWasShown(bool needs_repainting) OVERRIDE;
  virtual GURL GetURLForGraphicsContext3D() OVERRIDE;
  virtual bool ForceCompositingModeEnabled() OVERRIDE;
  virtual void OnImeSetComposition(
      const string16& text,
      const std::vector<WebKit::WebCompositionUnderline>& underlines,
      int selection_start,
      int selection_end) OVERRIDE;
  virtual void OnImeConfirmComposition(
      const string16& text, const ui::Range& replacement_range) OVERRIDE;
  virtual void SetDeviceScaleFactor(float device_scale_factor) OVERRIDE;
  virtual ui::TextInputType GetTextInputType() OVERRIDE;
  virtual void GetSelectionBounds(gfx::Rect* start, gfx::Rect* end) OVERRIDE;
  virtual void GetCompositionCharacterBounds(
      std::vector<gfx::Rect>* character_bounds) OVERRIDE;
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

  // Do not delete directly.  This class is reference counted.
  virtual ~RenderViewImpl();

 private:
  // For unit tests.
  friend class ExternalPopupMenuTest;
  friend class PepperDeviceTest;
  friend class RendererAccessibilityTest;
  friend class RenderViewTest;

  FRIEND_TEST_ALL_PREFIXES(ExternalPopupMenuRemoveTest, RemoveOnChange);
  FRIEND_TEST_ALL_PREFIXES(ExternalPopupMenuTest, NormalCase);
  FRIEND_TEST_ALL_PREFIXES(ExternalPopupMenuTest, ShowPopupThenNavigate);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, DecideNavigationPolicyForWebUI);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest,
                           DontIgnoreBackAfterNavEntryLimit);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, ImeComposition);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, InsertCharacters);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, JSBlockSentAfterPageLoad);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, LastCommittedUpdateState);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, OnExtendSelectionAndDelete);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, OnHandleKeyboardEvent);
  FRIEND_TEST_ALL_PREFIXES(RenderViewImplTest, OnImeStateChanged);
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

  typedef std::map<GURL, double> HostZoomLevels;

  enum ErrorPageType {
    DNS_ERROR,
    HTTP_404,
    CONNECTION_ERROR,
  };

  void UpdateURL(WebKit::WebFrame* frame);
  void UpdateTitle(WebKit::WebFrame* frame, const string16& title,
                   WebKit::WebTextDirection title_direction);
  void UpdateSessionHistory(WebKit::WebFrame* frame);
  void SendUpdateState(const WebKit::WebHistoryItem& item);

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
  void UpdateEncoding(WebKit::WebFrame* frame,
                      const std::string& encoding_name);

  void OpenURL(WebKit::WebFrame* frame,
               const GURL& url,
               const Referrer& referrer,
               WebKit::WebNavigationPolicy policy);

  bool RunJavaScriptMessage(JavaScriptMessageType type,
                            const string16& message,
                            const string16& default_value,
                            const GURL& frame_url,
                            string16* result);

  // Sends a message and runs a nested message loop.
  bool SendAndRunNestedMessageLoop(IPC::SyncMessage* message);

  // Called when the "pinned to left/right edge" state needs to be updated.
  void UpdateScrollState(WebKit::WebFrame* frame);

  // IPC message handlers ------------------------------------------------------
  //
  // The documentation for these functions should be in
  // render_messages_internal.h for the message that the function is handling.

  void OnAllowBindings(int enabled_bindings_flags);
  void OnAllowScriptToClose(bool script_can_close);
  void OnAsyncFileOpened(base::PlatformFileError error_code,
                         IPC::PlatformFileForTransit file_for_transit,
                         int message_id);
  void OnPpapiBrokerChannelCreated(int request_id,
                                   base::ProcessId broker_pid,
                                   const IPC::ChannelHandle& handle);
  void OnPpapiBrokerPermissionResult(int request_id, bool result);
  void OnCancelDownload(int32 download_id);
  void OnClearFocusedNode();
  void OnClosePage();
  void OnContextMenuClosed(const CustomContextMenuContext& custom_context);
  void OnCopy();
  void OnCopyImageAt(int x, int y);
  void OnCut();
  void OnCSSInsertRequest(const string16& frame_xpath,
                          const std::string& css);
  void OnCustomContextMenuAction(const CustomContextMenuContext& custom_context,
      unsigned action);
  void OnDelete();
  void OnSetName(const std::string& name);
  void OnDeterminePageLanguage();
  void OnDisableScrollbarsForSmallWindows(
      const gfx::Size& disable_scrollbars_size_limit);
  void OnDisassociateFromPopupCount();
  void OnDragSourceEndedOrMoved(const gfx::Point& client_point,
                                const gfx::Point& screen_point,
                                bool ended,
                                WebKit::WebDragOperation drag_operation);
  void OnDragSourceSystemDragEnded();
  void OnDragTargetDrop(const gfx::Point& client_pt,
                        const gfx::Point& screen_pt,
                        int key_modifiers);
  void OnDragTargetDragEnter(const WebDropData& drop_data,
                             const gfx::Point& client_pt,
                             const gfx::Point& screen_pt,
                             WebKit::WebDragOperationsMask operations_allowed,
                             int key_modifiers);
  void OnDragTargetDragLeave();
  void OnDragTargetDragOver(const gfx::Point& client_pt,
                            const gfx::Point& screen_pt,
                            WebKit::WebDragOperationsMask operations_allowed,
                            int key_modifiers);
  void OnEnablePreferredSizeChangedMode();
  void OnEnableAutoResize(const gfx::Size& min_size, const gfx::Size& max_size);
  void OnDisableAutoResize(const gfx::Size& new_size);
  void OnEnumerateDirectoryResponse(int id,
                                    const std::vector<base::FilePath>& paths);
  void OnExecuteEditCommand(const std::string& name, const std::string& value);
  void OnExtendSelectionAndDelete(int before, int after);
  void OnFileChooserResponse(
      const std::vector<ui::SelectedFileInfo>& files);
  void OnFind(int request_id, const string16&, const WebKit::WebFindOptions&);
  void OnGetAllSavableResourceLinksForCurrentPage(const GURL& page_url);
  void OnGetSerializedHtmlDataForCurrentPageWithLocalLinks(
      const std::vector<GURL>& links,
      const std::vector<base::FilePath>& local_paths,
      const base::FilePath& local_directory_name);
  void OnMediaPlayerActionAt(const gfx::Point& location,
                             const WebKit::WebMediaPlayerAction& action);

  // Screen has rotated. 0 = default (portrait), 90 = one turn right, and so on.
  void OnOrientationChangeEvent(int orientation);

  void OnPluginActionAt(const gfx::Point& location,
                        const WebKit::WebPluginAction& action);
  void OnMoveOrResizeStarted();
  void OnNavigate(const ViewMsg_Navigate_Params& params);
  void OnPaste();
  void OnPasteAndMatchStyle();
  void OnPostMessageEvent(const ViewMsg_PostMessage_Params& params);
  void OnRedo();
  void OnReleaseDisambiguationPopupDIB(TransportDIB::Handle dib_handle);
  void OnReloadFrame();
  void OnReplace(const string16& text);
  void OnReplaceMisspelling(const string16& text);
  void OnResetPageEncodingToDefault();
  void OnScriptEvalRequest(const string16& frame_xpath,
                           const string16& jscript,
                           int id,
                           bool notify_result);
  void OnSelectAll();
  void OnSelectRange(const gfx::Point& start, const gfx::Point& end);
  void OnMoveCaret(const gfx::Point& point);
  void OnSetAccessibilityMode(AccessibilityMode new_mode);
  void OnSetActive(bool active);
  void OnSetAltErrorPageURL(const GURL& gurl);
  void OnSetBackground(const SkBitmap& background);
  void OnSetCompositionFromExistingText(
      int start, int end,
      const std::vector<WebKit::WebCompositionUnderline>& underlines);
  void OnSetEditableSelectionOffsets(int start, int end);
  void OnSetNavigationStartTime(
      const base::TimeTicks& browser_navigation_start);
  void OnSetWebUIProperty(const std::string& name, const std::string& value);
  void OnSetEditCommandsForNextKeyEvent(const EditCommands& edit_commands);
  void OnSetHistoryLengthAndPrune(int history_length, int32 minimum_page_id);
  void OnSetInitialFocus(bool reverse);
  void OnScrollFocusedEditableNodeIntoRect(const gfx::Rect& rect);
  void OnSetPageEncoding(const std::string& encoding_name);
  void OnSetRendererPrefs(const RendererPreferences& renderer_prefs);
  void OnSetZoomLevel(double zoom_level);
  void OnSetZoomLevelForLoadingURL(const GURL& url, double zoom_level);
  void OnExitFullscreen();
  void OnShouldClose();
  void OnStop();
  void OnStopFinding(StopFindAction action);
  void OnSwapOut(const ViewMsg_SwapOut_Params& params);
  void OnThemeChanged();
  void OnUndo();
  void OnUpdateTargetURLAck();
  void OnUpdateTimezone();
  void OnUpdateWebPreferences(const webkit_glue::WebPreferences& prefs);
  void OnUnselect();

  void OnZoom(PageZoom zoom);
  void OnZoomFactor(PageZoom zoom, int zoom_center_x, int zoom_center_y);

  void OnEnableViewSourceMode();

  void OnJavaBridgeInit();

  void OnDisownOpener();
  void OnUpdatedFrameTree(int process_id,
                          int route_id,
                          const std::string& frame_tree);

#if defined(OS_ANDROID)
  void OnActivateNearestFindResult(int request_id, float x, float y);
  void OnFindMatchRects(int current_version);
  void OnSelectPopupMenuItems(bool canceled,
                              const std::vector<int>& selected_indices);
  void OnUndoScrollFocusedEditableNodeIntoRect();
  void OnEnableHidingTopControls(bool enable);
#elif defined(OS_MACOSX)
  void OnCopyToFindPboard();
  void OnPluginImeCompositionCompleted(const string16& text, int plugin_id);
  void OnSelectPopupMenuItem(int selected_index);
  void OnSetInLiveResize(bool in_live_resize);
  void OnSetWindowVisibility(bool visible);
  void OnWindowFrameChanged(const gfx::Rect& window_frame,
                            const gfx::Rect& view_frame);
#endif

  void OnWindowSnapshotCompleted(const int snapshot_id,
      const gfx::Size& size, const std::vector<unsigned char>& png);


  // Adding a new message handler? Please add it in alphabetical order above
  // and put it in the same position in the .cc file.

  // Misc private functions ----------------------------------------------------
  void ZoomFactorHelper(PageZoom zoom, int zoom_center_x, int zoom_center_y,
                        float scaling_increment);

  void AltErrorPageFinished(WebKit::WebFrame* frame,
                            const WebKit::WebURLError& original_error,
                            const std::string& html);

  // Check whether the preferred size has changed.
  void CheckPreferredSize();

  // This method walks the entire frame tree for this RenderView and sends an
  // update to the browser process as described in the
  // ViewHostMsg_FrameTreeUpdated comments. If |exclude_frame_subtree|
  // frame is non-NULL, the subtree starting at that frame not included in the
  // serialized form.
  // This is used when a frame is going to be removed from the tree.
  void SendUpdatedFrameTree(WebKit::WebFrame* exclude_frame_subtree);

  // Recursively creates a DOM frame tree starting with |frame|, based on
  // |frame_tree|. For each node, the frame is navigated to the swapped out URL,
  // the name (if present) is set on it, and all the subframes are created
  // and added to the DOM.
  void CreateFrameTree(WebKit::WebFrame* frame,
                       base::DictionaryValue* frame_tree);

  // If this is a swapped out RenderView, which maintains a copy of the frame
  // tree of an active RenderView, we keep a map from frame ids in this view to
  // the frame ids of the active view for each corresponding frame.
  // This method returns the frame in this RenderView that corresponds to the
  // frame in the active RenderView specified by |remote_frame_id|.
  WebKit::WebFrame* GetFrameByRemoteID(int remote_frame_id);

  void EnsureMediaStreamImpl();

  // This callback is triggered when DownloadFavicon completes, either
  // succesfully or with a failure. See DownloadFavicon for more
  // details.
  void DidDownloadFavicon(webkit_glue::ImageResourceFetcher* fetcher,
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
  WebKit::WebFrame* GetChildFrame(const string16& frame_xpath) const;

  // Returns the URL being loaded by the given frame's request.
  GURL GetLoadingUrl(WebKit::WebFrame* frame) const;

  // Should only be called if this object wraps a PluginDocument.
  WebKit::WebPlugin* GetWebPluginFromPluginDocument();

  // Returns true if the |params| navigation is to an entry that has been
  // cropped due to a recent navigation the browser did not know about.
  bool IsBackForwardToStaleEntry(const ViewMsg_Navigate_Params& params,
                                 bool is_reload);

  bool MaybeLoadAlternateErrorPage(WebKit::WebFrame* frame,
                                   const WebKit::WebURLError& error,
                                   bool replace);

  // Make this RenderView show an empty, unscriptable page.
  void NavigateToSwappedOutURL(WebKit::WebFrame* frame);

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
#endif

  // Sends a reply to the current find operation handling if it was a
  // synchronous find request.
  void SendFindReply(int request_id,
                     int match_count,
                     int ordinal,
                     const WebKit::WebRect& selection_rect,
                     bool final_status_update);

  // Returns whether |params.selection_text| should be synchronized to the
  // browser before bringing up the context menu. Static for testing.
  static bool ShouldUpdateSelectionTextFromContextMenuParams(
      const string16& selection_text,
      size_t selection_text_offset,
      const ui::Range& selection_range,
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

  // Coordinate conversion -----------------------------------------------------

  gfx::RectF ClientRectToPhysicalWindowRect(const gfx::RectF& rect) const;

  // ---------------------------------------------------------------------------
  // ADDING NEW FUNCTIONS? Please keep private functions alphabetized and put
  // it in the same order in the .cc file as it was in the header.
  // ---------------------------------------------------------------------------

  // Settings ------------------------------------------------------------------

  webkit_glue::WebPreferences webkit_preferences_;
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

  // If we are handling a top-level client-side redirect, this tracks the URL
  // of the page that initiated it. Specifically, when a load is committed this
  // is used to determine if that load originated from a client-side redirect.
  // It is empty if there is no top-level client-side redirect.
  Referrer completed_client_redirect_src_;

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
  ui::Range selection_range_;

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

  // MediaStreamImpl attached to this view; lazily initialized.
  MediaStreamImpl* media_stream_impl_;

  DevToolsAgent* devtools_agent_;

  // The current accessibility mode.
  AccessibilityMode accessibility_mode_;

  // Only valid if |accessibility_mode_| is anything other than
  // AccessibilityModeOff.
  RendererAccessibility* renderer_accessibility_;

  // Java Bridge dispatcher attached to this view; lazily initialized.
  JavaBridgeDispatcher* java_bridge_dispatcher_;

  // Mouse Lock dispatcher attached to this view.
  MouseLockDispatcher* mouse_lock_dispatcher_;

  // Helper class to handle favicon changes.
  FaviconHelper* favicon_helper_;

#if defined(OS_ANDROID)
  // Android Specific ---------------------------------------------------------

  // The background color of the document body element. This is used as the
  // default background color for filling the screen areas for which we don't
  // have the actual content.
  SkColor body_background_color_;

  // True if SendUpdateFrameInfo is pending.
  bool update_frame_info_scheduled_;

  // Expected id of the next content intent launched. Used to prevent scheduled
  // intents to be launched if aborted.
  size_t expected_content_intent_id_;

  // List of click-based content detectors.
  typedef std::vector< linked_ptr<ContentDetector> > ContentDetectorList;
  ContentDetectorList content_detectors_;

  // Proxy class for WebMediaPlayer to communicate with the real media player
  // objects in browser process.
  WebMediaPlayerProxyImplAndroid* media_player_proxy_;

  // The media player manager for managing all the media players on this view.
  scoped_ptr<webkit_media::WebMediaPlayerManagerAndroid> media_player_manager_;

  // Resource manager for all the android media player objects if they are
  // created in the renderer process.
  scoped_ptr<webkit_media::MediaPlayerBridgeManagerImpl> media_bridge_manager_;

  // A date/time picker object for date and time related input elements.
  scoped_ptr<RendererDateTimePicker> date_time_picker_client_;
#endif

  // Misc ----------------------------------------------------------------------

  // The current and pending file chooser completion objects. If the queue is
  // nonempty, the first item represents the currently running file chooser
  // callback, and the remaining elements are the other file chooser completion
  // still waiting to be run (in order).
  struct PendingFileChooser;
  std::deque< linked_ptr<PendingFileChooser> > file_chooser_completions_;

  // The current directory enumeration callback
  std::map<int, WebKit::WebFileChooserCompletion*> enumeration_completions_;
  int enumeration_completion_id_;

  // Reports load progress to the browser.
  scoped_ptr<LoadProgressTracker> load_progress_tracker_;

  // The SessionStorage namespace that we're assigned to has an ID, and that ID
  // is passed to us upon creation.  WebKit asks for this ID upon first use and
  // uses it whenever asking the browser process to allocate new storage areas.
  int64 session_storage_namespace_id_;

  // The total number of unrequested popups that exist and can be followed back
  // to a common opener. This count is shared among all RenderViews created with
  // createView(). All popups are treated as unrequested until specifically
  // instructed otherwise by the Browser process.
  scoped_refptr<SharedRenderViewCounter> shared_popup_counter_;

  // Whether this is a top level window (instead of a popup). Top level windows
  // shouldn't count against their own |shared_popup_counter_|.
  bool decrement_shared_popup_at_destruction_;

  // Stores edit commands associated to the next key event.
  // Shall be cleared as soon as the next key event is processed.
  EditCommands edit_commands_;

  // The external popup for the currently showing select popup.
  scoped_ptr<ExternalPopupMenu> external_popup_menu_;

  // The node that the context menu was pressed over.
  WebKit::WebNode context_menu_node_;

  // All the registered observers.  We expect this list to be small, so vector
  // is fine.
  ObserverList<RenderViewObserver> observers_;

  // Used to inform didChangeSelection() when it is called in the context
  // of handling a ViewMsg_SelectRange IPC.
  bool handling_select_range_;

  // Wraps the |webwidget_| as a MouseLockDispatcher::LockTarget interface.
  scoped_ptr<MouseLockDispatcher::LockTarget> webwidget_mouse_lock_target_;

  // State associated with the GetWindowSnapshot function.
  int next_snapshot_id_;
  typedef std::map<int, WindowSnapshotCallback>
      PendingSnapshotMap;
  PendingSnapshotMap pending_snapshots_;

  bool allow_partial_swap_;

  // Plugins -------------------------------------------------------------------

  // All the currently active plugin delegates for this RenderView; kept so
  // that we can enumerate them to send updates about things like window
  // location or tab focus and visibily. These are non-owning references.
  std::set<WebPluginDelegateProxy*> plugin_delegates_;

#if defined(OS_WIN)
  // The ID of the focused NPAPI plug-in.
  int focused_plugin_id_;
#endif

  // Allows JS to access DOM automation. The JS object is only exposed when the
  // DOM automation bindings are enabled.
  scoped_ptr<DomAutomationController> dom_automation_controller_;

  // Boolean indicating whether we are in the process of creating the frame
  // tree for this renderer in response to ViewMsg_UpdateFrameTree.  If true,
  // we won't be sending ViewHostMsg_FrameTreeUpdated messages back to the
  // browser, as those will be redundant.
  bool updating_frame_tree_;

  // Boolean indicating that the frame tree has changed, but a message has not
  // been sent to the browser because a page has been loading. This helps
  // avoid extra messages being sent to the browser when navigating away from a
  // page with subframes, which will be destroyed. Instead, a single message
  // is sent when the load is stopped with the final state of the frame tree.
  //
  // TODO(nasko): Relying on the is_loading_ means that frame tree updates will
  // not be sent until *all* subframes have completed loading. This can cause
  // JavaScript calls to fail, if they occur prior to the first update message
  // being sent. This will be fixed by bug http://crbug.com/145014.
  bool pending_frame_tree_update_;

  // If this render view is a swapped out "mirror" of an active render view in a
  // different process, we record the process id and route id for the active RV.
  // For further details, see the comments on ViewHostMsg_FrameTreeUpdated.
  int target_process_id_;
  int target_routing_id_;

  // A map of the current process's frame ids to ids in the remote active render
  // view, if this is a swapped out render view.
  std::map<int, int> active_frame_id_map_;

  // This field stores drag/drop related info for the event that is currently
  // being handled. If the current event results in starting a drag/drop
  // session, this info is sent to the browser along with other drag/drop info.
  DragEventSourceInfo possible_drag_event_info_;

  // NOTE: pepper_helper_ should be last member because its constructor calls
  // AddObservers method of RenderViewImpl from c-tor.
  scoped_ptr<RenderViewPepperHelper> pepper_helper_;

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
