// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDER_VIEW_H_
#define CHROME_RENDERER_RENDER_VIEW_H_
#pragma once

#include <deque>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "app/surface/transport_dib.h"
#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/id_map.h"
#include "base/linked_ptr.h"
#include "base/timer.h"
#include "base/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/edit_command.h"
#include "chrome/common/navigation_gesture.h"
#include "chrome/common/page_zoom.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/renderer_preferences.h"
#include "chrome/common/view_types.h"
#include "chrome/renderer/pepper_plugin_delegate_impl.h"
#include "chrome/renderer/render_widget.h"
#include "chrome/renderer/renderer_webcookiejar_impl.h"
#include "chrome/renderer/translate_helper.h"
#include "third_party/WebKit/WebKit/chromium/public/WebConsoleMessage.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileSystem.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrameClient.h"
#include "third_party/WebKit/WebKit/chromium/public/WebMediaPlayerAction.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPageSerializerClient.h"
#include "third_party/WebKit/WebKit/chromium/public/WebTextDirection.h"
#include "third_party/WebKit/WebKit/chromium/public/WebViewClient.h"
#include "third_party/WebKit/WebKit/chromium/public/WebNavigationType.h"
#include "webkit/glue/plugins/webplugin_page_delegate.h"
#include "webkit/glue/webpreferences.h"

#if defined(OS_WIN)
// RenderView is a diamond-shaped hierarchy, with WebWidgetClient at the root.
// VS warns when we inherit the WebWidgetClient method implementations from
// RenderWidget.  It's safe to ignore that warning.
#pragma warning(disable: 4250)
#endif

class AudioMessageFilter;
class AutoFillHelper;
class DictionaryValue;
class DeviceOrientationDispatcher;
class DevToolsAgent;
class DevToolsClient;
class DomAutomationController;
class DOMUIBindings;
class ExternalHostBindings;
class FilePath;
class GeolocationDispatcher;
class GURL;
class ListValue;
class NavigationState;
class NotificationProvider;
class PageClickTracker;
class PasswordAutocompleteManager;
class PepperDeviceTest;
class PluginGroup;
class PrintWebViewHelper;
class RenderViewVisitor;
class SkBitmap;
class SpeechInputDispatcher;
class WebPluginDelegatePepper;
class WebPluginDelegateProxy;
struct ContextMenuMediaParams;
struct ThumbnailScore;
struct ViewMsg_ClosePage_Params;
struct ViewMsg_Navigate_Params;
struct WebDropData;

namespace base {
class WaitableEvent;
}

namespace gfx {
class Point;
class Rect;
}

namespace pepper {
class PluginInstance;
class FullscreenContainer;
}

namespace webkit_glue {
class ImageResourceFetcher;
struct FileUploadData;
struct FormData;
struct PasswordFormFillData;
}

namespace WebKit {
class WebAccessibilityCache;
class WebAccessibilityObject;
class WebApplicationCacheHost;
class WebApplicationCacheHostClient;
class WebDataSource;
class WebDocument;
class WebDragData;
class WebFrame;
class WebGeolocationServiceInterface;
class WebImage;
class WebInputElement;
class WebKeyboardEvent;
class WebMediaPlayer;
class WebMediaPlayerClient;
class WebMouseEvent;
class WebNode;
class WebPlugin;
class WebSpeechInputController;
class WebSpeechInputListener;
class WebStorageNamespace;
class WebURLRequest;
class WebView;
struct WebContextMenuData;
struct WebFileChooserParams;
struct WebFindOptions;
struct WebPluginParams;
struct WebPoint;
struct WebWindowFeatures;
}

// We need to prevent a page from trying to create infinite popups. It is not
// as simple as keeping a count of the number of immediate children
// popups. Having an html file that window.open()s itself would create
// an unlimited chain of RenderViews who only have one RenderView child.
//
// Therefore, each new top level RenderView creates a new counter and shares it
// with all its children and grandchildren popup RenderViews created with
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
class RenderView : public RenderWidget,
                   public WebKit::WebViewClient,
                   public WebKit::WebFrameClient,
                   public WebKit::WebPageSerializerClient,
                   public webkit_glue::WebPluginPageDelegate,
                   public base::SupportsWeakPtr<RenderView> {
 public:
  // Creates a new RenderView.  The parent_hwnd specifies a HWND to use as the
  // parent of the WebView HWND that will be created.  If this is a constrained
  // popup or as a new tab, opener_id is the routing ID of the RenderView
  // responsible for creating this RenderView (corresponding to parent_hwnd).
  // |counter| is either a currently initialized counter, or NULL (in which case
  // we treat this RenderView as a top level window).
  static RenderView* Create(
      RenderThreadBase* render_thread,
      gfx::NativeViewId parent_hwnd,
      int32 opener_id,
      const RendererPreferences& renderer_prefs,
      const WebPreferences& webkit_prefs,
      SharedRenderViewCounter* counter,
      int32 routing_id,
      int64 session_storage_namespace_id,
      const string16& frame_name);

  // Visit all RenderViews with a live WebView (i.e., RenderViews that have
  // been closed but not yet destroyed are excluded).
  static void ForEach(RenderViewVisitor* visitor);

  // Returns the RenderView containing the given WebView.
  static RenderView* FromWebView(WebKit::WebView* webview);

  // Sets the "next page id" counter.
  static void SetNextPageID(int32 next_page_id);

  // May return NULL when the view is closing.
  WebKit::WebView* webview() const;

  int browser_window_id() const {
    return browser_window_id_;
  }

  ViewType::Type view_type() const {
    return view_type_;
  }

  PrintWebViewHelper* print_helper() const {
    return print_helper_.get();
  }

  int page_id() const {
    return page_id_;
  }

  AudioMessageFilter* audio_message_filter() {
    return audio_message_filter_;
  }

  const WebPreferences& webkit_preferences() const {
    return webkit_preferences_;
  }

  void set_send_content_state_immediately(bool value) {
    send_content_state_immediately_ = value;
  }

  PageClickTracker* page_click_tracker() const {
    return page_click_tracker_.get();
  }

  // Called from JavaScript window.external.AddSearchProvider() to add a
  // keyword for a provider described in the given OpenSearch document.
  void AddSearchProvider(const std::string& url);

  // Returns the install state for the given search provider url.
  ViewHostMsg_GetSearchProviderInstallState_Params
      GetSearchProviderInstallState(WebKit::WebFrame* frame,
                                    const std::string& url);

  // Sends ViewHostMsg_SetSuggestResult to the browser.
  void SetSuggestResult(const std::string& suggest);

  // Evaluates a string of JavaScript in a particular frame.
  void EvaluateScript(const std::wstring& frame_xpath,
                      const std::wstring& jscript);

  // Adds the given file chooser request to the file_chooser_completion_ queue
  // (see that var for more) and requests the chooser be displayed if there are
  // no other waiting items in the queue.
  //
  // Returns true if the chooser was successfully scheduled. False means we
  // didn't schedule anything.
  bool ScheduleFileChooser(const ViewHostMsg_RunFileChooser_Params& params,
                           WebKit::WebFileChooserCompletion* completion);

  // Called when the translate helper has finished translating the page.  We
  // use this signal to re-scan the page for forms.
  void OnPageTranslated();

  // Sets the content settings that back allowScripts(), allowImages(), and
  // allowPlugins().
  void SetContentSettings(const ContentSettings& settings);

  // Notifies the browser that the given action has been performed. This is
  // aggregated to the user metrics service.
  void UserMetricsRecordAction(const std::string& action);

  // Extensions ----------------------------------------------------------------

  void SendExtensionRequest(const ViewHostMsg_DomMessage_Params& params);

  void OnExtensionResponse(int request_id, bool success,
                           const std::string& response,
                           const std::string& error);

  void OnSetExtensionViewMode(const std::string& mode);

  // Called when the "idle" user script state has been reached. See
  // UserScript::DOCUMENT_IDLE.
  void OnUserScriptIdleTriggered(WebKit::WebFrame* frame);

  // Plugin-related functions --------------------------------------------------
  // (See also WebPluginPageDelegate implementation.)

  // Notification that the given plugin has crashed.
  void PluginCrashed(const FilePath& plugin_path);

  // Notification that the default plugin has done something about a missing
  // plugin. See default_plugin_shared.h for possible values of |status|.
  void OnMissingPluginStatus(WebPluginDelegateProxy* delegate,
                             int status);

  // Notification that the given pepper plugin we created is being deleted the
  // pointer must not be dereferenced as this is called from the destructor of
  // the plugin.
  void OnPepperPluginDestroy(WebPluginDelegatePepper* pepper_plugin);

  // Creates a fullscreen container for a pepper plugin instance.
  pepper::FullscreenContainer* CreatePepperFullscreenContainer(
      pepper::PluginInstance* plugin);

  // Create a new plugin without checking the content settings.
  WebKit::WebPlugin* CreatePluginNoCheck(WebKit::WebFrame* frame,
                                         const WebKit::WebPluginParams& params);

  // Asks the browser for the CPBrowsingContext associated with this renderer.
  // This is an opaque identifier associated with the renderer for sending
  // messages for the given "Chrome Plugin." The Chrome Plugin API is used
  // only by gears and this function can be deleted when we remove gears.
  uint32 GetCPBrowsingContext();

#if defined(OS_MACOSX)
  // Helper routines for GPU plugin support. Used by the
  // WebPluginDelegateProxy, which has a pointer to the RenderView.
  gfx::PluginWindowHandle AllocateFakePluginWindowHandle(bool opaque,
                                                         bool root);
  void DestroyFakePluginWindowHandle(gfx::PluginWindowHandle window);
  void AcceleratedSurfaceSetIOSurface(gfx::PluginWindowHandle window,
                                      int32 width,
                                      int32 height,
                                      uint64 io_surface_identifier);
  TransportDIB::Handle AcceleratedSurfaceAllocTransportDIB(size_t size);
  void AcceleratedSurfaceFreeTransportDIB(TransportDIB::Id dib_id);
  void AcceleratedSurfaceSetTransportDIB(gfx::PluginWindowHandle window,
                                         int32 width,
                                         int32 height,
                                         TransportDIB::Handle transport_dib);
  void AcceleratedSurfaceBuffersSwapped(gfx::PluginWindowHandle window);
#endif

  void RegisterPluginDelegate(WebPluginDelegateProxy* delegate);
  void UnregisterPluginDelegate(WebPluginDelegateProxy* delegate);

  // IPC::Channel::Listener implementation -------------------------------------

  virtual void OnMessageReceived(const IPC::Message& msg);

  // WebKit::WebWidgetClient implementation ------------------------------------

  // Most methods are handled by RenderWidget.
  virtual void didFocus();
  virtual void didBlur();
  virtual void show(WebKit::WebNavigationPolicy policy);
  virtual void closeWidgetSoon();
  virtual void runModal();

  // WebKit::WebViewClient implementation --------------------------------------

  virtual WebKit::WebView* createView(
      WebKit::WebFrame* creator,
      const WebKit::WebWindowFeatures& features,
      const WebKit::WebString& frame_name);
  virtual WebKit::WebWidget* createPopupMenu(WebKit::WebPopupType popup_type);
  virtual WebKit::WebWidget* createPopupMenu(
      const WebKit::WebPopupMenuInfo& info);
  virtual WebKit::WebWidget* createFullscreenWindow(
      WebKit::WebPopupType popup_type);
  virtual WebKit::WebStorageNamespace* createSessionStorageNamespace(
      unsigned quota);
  virtual void didAddMessageToConsole(
      const WebKit::WebConsoleMessage& message,
      const WebKit::WebString& source_name,
      unsigned source_line);
  virtual void printPage(WebKit::WebFrame* frame);
  virtual WebKit::WebNotificationPresenter* notificationPresenter();
  virtual void didStartLoading();
  virtual void didStopLoading();
  virtual bool isSmartInsertDeleteEnabled();
  virtual bool isSelectTrailingWhitespaceEnabled();
  virtual void didChangeSelection(bool is_selection_empty);
  virtual void didExecuteCommand(const WebKit::WebString& command_name);
  virtual void textFieldDidEndEditing(const WebKit::WebInputElement& element);
  virtual void textFieldDidChange(const WebKit::WebInputElement& element);
  virtual void textFieldDidReceiveKeyDown(
      const WebKit::WebInputElement& element,
      const WebKit::WebKeyboardEvent& event);
  virtual bool handleCurrentKeyboardEvent();
  virtual void inputElementClicked(const WebKit::WebInputElement& element,
                                   bool already_focused);
  virtual void spellCheck(const WebKit::WebString& text,
                          int& offset,
                          int& length);
  virtual WebKit::WebString autoCorrectWord(
      const WebKit::WebString& misspelled_word);
  virtual void showSpellingUI(bool show);
  virtual bool isShowingSpellingUI();
  virtual void updateSpellingUIWithMisspelledWord(
      const WebKit::WebString& word);
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
  virtual bool supportsFullscreen();
  virtual void enterFullscreenForNode(const WebKit::WebNode&);
  virtual void exitFullscreenForNode(const WebKit::WebNode&);
  virtual void setStatusText(const WebKit::WebString& text);
  virtual void setMouseOverURL(const WebKit::WebURL& url);
  virtual void setKeyboardFocusURL(const WebKit::WebURL& url);
  virtual void setToolTipText(const WebKit::WebString& text,
                              WebKit::WebTextDirection hint);
  virtual void startDragging(const WebKit::WebDragData& data,
                             WebKit::WebDragOperationsMask mask,
                             const WebKit::WebImage& image,
                             const WebKit::WebPoint& imageOffset);
  virtual bool acceptsLoadDrops();
  virtual void focusNext();
  virtual void focusPrevious();
  virtual void focusedNodeChanged(const WebKit::WebNode& node);
  virtual void navigateBackForwardSoon(int offset);
  virtual int historyBackListCount();
  virtual int historyForwardListCount();
  virtual void focusAccessibilityObject(
      const WebKit::WebAccessibilityObject& acc_obj);
  virtual void postAccessibilityNotification(
      const WebKit::WebAccessibilityObject& obj,
      WebKit::WebAccessibilityNotification notification);
  virtual void didUpdateInspectorSetting(const WebKit::WebString& key,
                                         const WebKit::WebString& value);
  virtual void queryAutofillSuggestions(const WebKit::WebNode& node,
                                        const WebKit::WebString& name,
                                        const WebKit::WebString& value);
  virtual void removeAutofillSuggestions(const WebKit::WebString& name,
                                         const WebKit::WebString& value);
  virtual void didAcceptAutoFillSuggestion(const WebKit::WebNode& node,
                                           const WebKit::WebString& value,
                                           const WebKit::WebString& label,
                                           int unique_id,
                                           unsigned index);
  virtual void didSelectAutoFillSuggestion(const WebKit::WebNode& node,
                                           const WebKit::WebString& value,
                                           const WebKit::WebString& label,
                                           int unique_id);
  virtual void didClearAutoFillSelection(const WebKit::WebNode& node);
  virtual void didAcceptAutocompleteSuggestion(
      const WebKit::WebInputElement& element);
  virtual WebKit::WebGeolocationService* geolocationService();
  virtual WebKit::WebSpeechInputController* speechInputController(
      WebKit::WebSpeechInputListener* listener);
  virtual WebKit::WebDeviceOrientationClient* deviceOrientationClient();

  // WebKit::WebFrameClient implementation -------------------------------------

  virtual WebKit::WebPlugin* createPlugin(
      WebKit::WebFrame* frame,
      const WebKit::WebPluginParams& params);
  virtual WebKit::WebWorker* createWorker(WebKit::WebFrame* frame,
                                          WebKit::WebWorkerClient* client);
  virtual WebKit::WebSharedWorker* createSharedWorker(
      WebKit::WebFrame* frame, const WebKit::WebURL& url,
      const WebKit::WebString& name, unsigned long long documentId);
  virtual WebKit::WebMediaPlayer* createMediaPlayer(
      WebKit::WebFrame* frame,
      WebKit::WebMediaPlayerClient* client);
  virtual WebKit::WebApplicationCacheHost* createApplicationCacheHost(
      WebKit::WebFrame* frame,
      WebKit::WebApplicationCacheHostClient* client);
  virtual WebKit::WebCookieJar* cookieJar();
  virtual void frameDetached(WebKit::WebFrame* frame);
  virtual void willClose(WebKit::WebFrame* frame);
  virtual bool allowImages(WebKit::WebFrame* frame, bool enabled_per_settings);
  virtual bool allowPlugins(WebKit::WebFrame* frame, bool enabled_per_settings);
  virtual void loadURLExternally(WebKit::WebFrame* frame,
                                 const WebKit::WebURLRequest& request,
                                 WebKit::WebNavigationPolicy policy);
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
                               const WebKit::WebString& title);
  virtual void didChangeIcons(WebKit::WebFrame*);
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
  virtual void didRunInsecureContent(WebKit::WebFrame* frame,
                                     const WebKit::WebSecurityOrigin& origin);
  virtual bool allowScript(WebKit::WebFrame* frame, bool enabled_per_settings);
  virtual bool allowDatabase(WebKit::WebFrame* frame,
                             const WebKit::WebString& name,
                             const WebKit::WebString& display_name,
                             unsigned long estimated_size);
  virtual void didNotAllowScript(WebKit::WebFrame* frame);
  virtual void didNotAllowPlugins(WebKit::WebFrame* frame);
  virtual void didExhaustMemoryAvailableForScript(WebKit::WebFrame* frame);
  virtual void didCreateScriptContext(WebKit::WebFrame* frame);
  virtual void didDestroyScriptContext(WebKit::WebFrame* frame);
  virtual void didCreateIsolatedScriptContext(WebKit::WebFrame* frame);
  virtual void logCrossFramePropertyAccess(
      WebKit::WebFrame* frame,
      WebKit::WebFrame* target,
      bool cross_origin,
      const WebKit::WebString& property_name,
      unsigned long long event_id);
  virtual void didChangeContentsSize(WebKit::WebFrame* frame,
                                     const WebKit::WebSize& size);
  virtual void didChangeScrollOffset(WebKit::WebFrame* frame);
  virtual void reportFindInPageMatchCount(int request_id,
                                          int count,
                                          bool final_update);
  virtual void reportFindInPageSelection(int request_id,
                                         int active_match_ordinal,
                                         const WebKit::WebRect& sel);

  virtual void openFileSystem(WebKit::WebFrame* frame,
                              WebKit::WebFileSystem::Type type,
                              long long size,
                              WebKit::WebFileSystemCallbacks* callbacks);

  // WebKit::WebPageSerializerClient implementation ----------------------------

  virtual void didSerializeDataForFrame(const WebKit::WebURL& frame_url,
                                        const WebKit::WebCString& data,
                                        PageSerializationStatus status);

  // webkit_glue::WebPluginPageDelegate implementation -------------------------

  virtual webkit_glue::WebPluginDelegate* CreatePluginDelegate(
      const FilePath& file_path,
      const std::string& mime_type);
  virtual void CreatedPluginWindow(gfx::PluginWindowHandle handle);
  virtual void WillDestroyPluginWindow(gfx::PluginWindowHandle handle);
  virtual void DidMovePlugin(const webkit_glue::WebPluginGeometry& move);
  virtual void DidStartLoadingForPlugin();
  virtual void DidStopLoadingForPlugin();
  virtual void ShowModalHTMLDialogForPlugin(
      const GURL& url,
      const gfx::Size& size,
      const std::string& json_arguments,
      std::string* json_retval);
  virtual WebKit::WebCookieJar* GetCookieJar();

  // Please do not add your stuff randomly to the end here. If there is an
  // appropriate section, add it there. If not, there are some random functions
  // nearer to the top you can add it to.

 protected:
  // RenderWidget overrides:
  virtual void Close();
  virtual void OnResize(const gfx::Size& new_size,
                        const gfx::Rect& resizer_rect);
  virtual void DidInitiatePaint();
  virtual void DidFlushPaint();
  virtual bool GetBitmapForOptimizedPluginPaint(gfx::Rect* bounds,
                                                TransportDIB** dib);
  virtual void DidHandleKeyEvent();
  virtual void DidHandleMouseEvent(const WebKit::WebMouseEvent& event);
  virtual void OnSetFocus(bool enable);
#if OS_MACOSX
  virtual void OnWasHidden();
  virtual void OnWasRestored(bool needs_repainting);
#endif

 private:
  // For unit tests.
  friend class RenderViewTest;
  friend class PepperDeviceTest;
  FRIEND_TEST_ALL_PREFIXES(RenderViewTest, OnNavStateChanged);
  FRIEND_TEST_ALL_PREFIXES(RenderViewTest, OnImeStateChanged);
  FRIEND_TEST_ALL_PREFIXES(RenderViewTest, ImeComposition);
  FRIEND_TEST_ALL_PREFIXES(RenderViewTest, OnSetTextDirection);
  FRIEND_TEST_ALL_PREFIXES(RenderViewTest, OnPrintPages);
  FRIEND_TEST_ALL_PREFIXES(RenderViewTest, PrintWithIframe);
  FRIEND_TEST_ALL_PREFIXES(RenderViewTest, PrintLayoutTest);
  FRIEND_TEST_ALL_PREFIXES(RenderViewTest, OnHandleKeyboardEvent);
  FRIEND_TEST_ALL_PREFIXES(RenderViewTest, InsertCharacters);
#if defined(OS_MACOSX)
  FRIEND_TEST_ALL_PREFIXES(RenderViewTest, MacTestCmdUp);
#endif
  FRIEND_TEST_ALL_PREFIXES(RenderViewTest, JSBlockSentAfterPageLoad);
  FRIEND_TEST_ALL_PREFIXES(RenderViewTest, UpdateTargetURLWithInvalidURL);

  typedef std::map<GURL, ContentSettings> HostContentSettings;
  typedef std::map<GURL, int> HostZoomLevels;

  enum ErrorPageType {
    DNS_ERROR,
    HTTP_404,
    CONNECTION_ERROR,
  };

  RenderView(RenderThreadBase* render_thread,
             const WebPreferences& webkit_preferences,
             int64 session_storage_namespace_id);

  // Do not delete directly.  This class is reference counted.
  virtual ~RenderView();

  // Initializes this view with the given parent and ID. The |routing_id| can be
  // set to 'MSG_ROUTING_NONE' if the true ID is not yet known. In this case,
  // CompleteInit must be called later with the true ID.
  void Init(gfx::NativeViewId parent,
            int32 opener_id,
            const RendererPreferences& renderer_prefs,
            SharedRenderViewCounter* counter,
            int32 routing_id,
            const string16& frame_name);

  void UpdateURL(WebKit::WebFrame* frame);
  void UpdateTitle(WebKit::WebFrame* frame, const string16& title);
  void UpdateSessionHistory(WebKit::WebFrame* frame);

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

  void OpenURL(const GURL& url, const GURL& referrer,
               WebKit::WebNavigationPolicy policy);

  // Captures the thumbnail and text contents for indexing for the given load
  // ID. If the view's load ID is different than the parameter, this call is
  // a NOP. Typically called on a timer, so the load ID may have changed in the
  // meantime.
  void CapturePageInfo(int load_id, bool preliminary_capture);

  // Retrieves the text from the given frame contents, the page text up to the
  // maximum amount kMaxIndexChars will be placed into the given buffer.
  void CaptureText(WebKit::WebFrame* frame, string16* contents);

  // Creates a thumbnail of |frame|'s contents resized to (|w|, |h|)
  // and puts that in |thumbnail|. Thumbnail metadata goes in |score|.
  bool CaptureThumbnail(WebKit::WebView* view, int w, int h,
                        SkBitmap* thumbnail,
                        ThumbnailScore* score);

  // Capture a snapshot of a view.  This is used to allow an extension
  // to get a snapshot of a tab using chrome.tabs.captureVisibleTab().
  bool CaptureSnapshot(WebKit::WebView* view, SkBitmap* snapshot);

  bool RunJavaScriptMessage(int type,
                            const std::wstring& message,
                            const std::wstring& default_value,
                            const GURL& frame_url,
                            std::wstring* result);

  // Sends a message and runs a nested message loop.
  bool SendAndRunNestedMessageLoop(IPC::SyncMessage* message);

  // Adds search provider from the given OpenSearch description URL as a
  // keyword search.
  void AddGURLSearchProvider(const GURL& osd_url, bool autodetected);

  // Called in a posted task by textFieldDidChange() to work-around a WebKit bug
  // http://bugs.webkit.org/show_bug.cgi?id=16976
  void TextFieldDidChangeImpl(const WebKit::WebInputElement& element);

  // IPC message handlers ------------------------------------------------------
  //
  // The documentation for these functions should be in
  // render_messages_internal.h for the message that the function is handling.

  void OnAccessibilityDoDefaultAction(int acc_obj_id);
  void OnAccessibilityNotificationsAck();
  void OnAllowBindings(int enabled_bindings_flags);
  void OnAddMessageToConsole(const string16& frame_xpath,
                             const string16& message,
                             const WebKit::WebConsoleMessage::Level&);
  void OnAdvanceToNextMisspelling();
  void OnAllowScriptToClose(bool script_can_close);
  void OnAutocompleteSuggestionsReturned(
      int query_id,
      const std::vector<string16>& suggestions,
      int default_suggestions_index);
  void OnAutoFillFormDataFilled(int query_id,
                                const webkit_glue::FormData& form);
  void OnAutoFillSuggestionsReturned(
      int query_id,
      const std::vector<string16>& values,
      const std::vector<string16>& labels,
      const std::vector<string16>& icons,
      const std::vector<int>& unique_ids);
  void OnCancelDownload(int32 download_id);
  void OnClearFocusedNode();
  void OnClosePage(const ViewMsg_ClosePage_Params& params);
  void OnCopy();
  void OnCopyImageAt(int x, int y);
#if defined(OS_MACOSX)
  void OnCopyToFindPboard();
#endif
  void OnCut();
  void OnCaptureThumbnail();
  void OnCaptureSnapshot();
  void OnCSSInsertRequest(const std::wstring& frame_xpath,
                          const std::string& css,
                          const std::string& id);
  void OnCustomContextMenuAction(unsigned action);
  void OnDelete();
  void OnDeterminePageLanguage();
  void OnDisableScrollbarsForSmallWindows(
      const gfx::Size& disable_scrollbars_size_limit);
  void OnDisassociateFromPopupCount();
  void OnDownloadFavIcon(int id, const GURL& image_url, int image_size);
  void OnDragSourceEndedOrMoved(const gfx::Point& client_point,
                                const gfx::Point& screen_point,
                                bool ended,
                                WebKit::WebDragOperation drag_operation);
  void OnDragSourceSystemDragEnded();
  void OnDragTargetDrop(const gfx::Point& client_pt,
                        const gfx::Point& screen_pt);
  void OnDragTargetDragEnter(const WebDropData& drop_data,
                             const gfx::Point& client_pt,
                             const gfx::Point& screen_pt,
                             WebKit::WebDragOperationsMask operations_allowed);
  void OnDragTargetDragLeave();
  void OnDragTargetDragOver(const gfx::Point& client_pt,
                            const gfx::Point& screen_pt,
                            WebKit::WebDragOperationsMask operations_allowed);
  void OnEnablePreferredSizeChangedMode(int flags);
  void OnEnableViewSourceMode();
  void OnExecuteCode(const ViewMsg_ExecuteCode_Params& params);
  void OnExecuteEditCommand(const std::string& name, const std::string& value);
  void OnExtensionMessageInvoke(const std::string& function_name,
                                const ListValue& args,
                                bool requires_incognito_access,
                                const GURL& event_url);
  void OnFileChooserResponse(const std::vector<FilePath>& paths);
  void OnFind(int request_id, const string16&, const WebKit::WebFindOptions&);
  void OnFindReplyAck();
  void OnGetAccessibilityTree();
  void OnGetAllSavableResourceLinksForCurrentPage(const GURL& page_url);
  void OnGetApplicationInfo(int page_id);
  void OnGetSerializedHtmlDataForCurrentPageWithLocalLinks(
      const std::vector<GURL>& links,
      const std::vector<FilePath>& local_paths,
      const FilePath& local_directory_name);
  void OnHandleMessageFromExternalHost(const std::string& message,
                                       const std::string& origin,
                                       const std::string& target);
  void OnInstallMissingPlugin();
  void OnLoadBlockedPlugins();
  void OnMediaPlayerActionAt(const gfx::Point& location,
                             const WebKit::WebMediaPlayerAction& action);
  void OnMoveOrResizeStarted();
  void OnNavigate(const ViewMsg_Navigate_Params& params);
  void OnNotifyRendererViewType(ViewType::Type view_type);
  void OnFillPasswordForm(
      const webkit_glue::PasswordFormFillData& form_data);
  void OnOpenFileSystemRequestComplete(int request_id,
                                       bool accepted,
                                       const string16& name,
                                       const string16& root_path);
  void OnPaste();
  void OnPrintingDone(int document_cookie, bool success);
  void OnPrintPages();
  void OnRedo();
  void OnReloadFrame();
  void OnReplace(const string16& text);
  void OnReservePageIDRange(int size_of_range);
  void OnResetPageEncodingToDefault();
  void OnRevertTranslation(int page_id);
  void OnScriptEvalRequest(const std::wstring& frame_xpath,
                           const std::wstring& jscript);
  void OnSelectAll();
  void OnSetAccessibilityFocus(int acc_obj_id);
  void OnSetActive(bool active);
  void OnSetAltErrorPageURL(const GURL& gurl);
  void OnSetBackground(const SkBitmap& background);
  void OnSetContentSettingsForLoadingURL(
      const GURL& url,
      const ContentSettings& content_settings);
  void OnSetDOMUIProperty(const std::string& name, const std::string& value);
  void OnSetEditCommandsForNextKeyEvent(const EditCommands& edit_commands);
  void OnSetInitialFocus(bool reverse);
  void OnSetPageEncoding(const std::string& encoding_name);
  void OnSetRendererPrefs(const RendererPreferences& renderer_prefs);
  void OnSetupDevToolsClient();
#if defined(OS_MACOSX)
  void OnSetWindowVisibility(bool visible);
#endif
  void OnSetZoomLevelForLoadingURL(const GURL& url, int zoom_level);
  void OnShouldClose();
  void OnStop();
  void OnStopFinding(const ViewMsg_StopFinding_Params& params);
  void OnThemeChanged();
  void OnToggleSpellCheck();
  void OnToggleSpellPanel(bool is_currently_visible);
  void OnTranslatePage(int page_id,
                       const std::string& translate_script,
                       const std::string& source_lang,
                       const std::string& target_lang);
  void OnUndo();
  void OnUpdateBrowserWindowId(int window_id);
  void OnUpdateTargetURLAck();
  void OnUpdateWebPreferences(const WebPreferences& prefs);
#if defined(OS_MACOSX)
  void OnWindowFrameChanged(const gfx::Rect& window_frame,
                            const gfx::Rect& view_frame);
#endif
  void OnZoom(PageZoom::Function function);

  void OnAsyncFileOpened(base::PlatformFileError error_code,
                         IPC::PlatformFileForTransit file_for_transit,
                         int message_id);

  // Adding a new message handler? Please add it in alphabetical order above
  // and put it in the same position in the .cc file.

  // Misc private functions ----------------------------------------------------

  // Helper method that returns if the user wants to block content of type
  // |content_type|.
  bool AllowContentType(ContentSettingsType settings_type);

  void AltErrorPageFinished(WebKit::WebFrame* frame,
                            const WebKit::WebURLError& original_error,
                            const std::string& html);

  // Exposes the DOMAutomationController object that allows JS to send
  // information to the browser process.
  void BindDOMAutomationController(WebKit::WebFrame* webframe);

  // Check whether the preferred size has changed. This is called periodically
  // by preferred_size_change_timer_.
  void CheckPreferredSize();

  // Resets the |content_blocked_| array.
  void ClearBlockedContentSettings();

  // Creates DevToolsClient and sets up JavaScript bindings for developer tools
  // UI that is going to be hosted by this RenderView.
  void CreateDevToolsClient();

  // Create a new NPAPI plugin.
  WebKit::WebPlugin* CreateNPAPIPlugin(WebKit::WebFrame* frame,
                                       const WebKit::WebPluginParams& params,
                                       const FilePath& path,
                                       const std::string& mime_type);

  // Create a new Pepper plugin.
  WebKit::WebPlugin* CreatePepperPlugin(WebKit::WebFrame* frame,
                                        const WebKit::WebPluginParams& params,
                                        const FilePath& path,
                                        pepper::PluginModule* pepper_module);

  // Create a new placeholder for a blocked plugin.
  WebKit::WebPlugin* CreatePluginPlaceholder(
      WebKit::WebFrame* frame,
      const WebKit::WebPluginParams& params,
      PluginGroup* group);

  // Sends an IPC notification that the specified content type was blocked.
  // If the content type requires it, |resource_identifier| names the specific
  // resource that was blocked (the plugin path in the case of plugins),
  // otherwise it's the empty string.
  void DidBlockContentType(ContentSettingsType settings_type,
                           const std::string& resource_identifier);

  // This callback is triggered when DownloadImage completes, either
  // succesfully or with a failure. See DownloadImage for more details.
  void DidDownloadImage(webkit_glue::ImageResourceFetcher* fetcher,
                        const SkBitmap& image);

  // Requests to download an image. When done, the RenderView is
  // notified by way of DidDownloadImage. Returns true if the request was
  // successfully started, false otherwise. id is used to uniquely identify the
  // request and passed back to the DidDownloadImage method. If the image has
  // multiple frames, the frame whose size is image_size is returned. If the
  // image doesn't have a frame at the specified size, the first is returned.
  bool DownloadImage(int id, const GURL& image_url, int image_size);

  void DumpLoadHistograms() const;

  // Initializes the document_tag_ member if necessary.
  void EnsureDocumentTag();

  // Backend for the IPC Message ExecuteCode in addition to being used
  // internally by other RenderView functions.
  void ExecuteCodeImpl(WebKit::WebFrame* frame,
                       const ViewMsg_ExecuteCode_Params& params);

  // Get all child frames of parent_frame, returned by frames_vector.
  bool GetAllChildFrames(WebKit::WebFrame* parent_frame,
                         std::vector<WebKit::WebFrame* >* frames_vector) const;

  GURL GetAlternateErrorPageURL(const GURL& failed_url,
                                ErrorPageType error_type);

  std::string GetAltHTMLForTemplate(const DictionaryValue& error_strings,
                                    int template_resource_id) const;

  // Locates a sub frame with given xpath
  WebKit::WebFrame* GetChildFrame(const std::wstring& frame_xpath) const;

  DOMUIBindings* GetDOMUIBindings();

  ExternalHostBindings* GetExternalHostBindings();

  // Should only be called if this object wraps a PluginDocument.
  WebKit::WebPlugin* GetWebPluginFromPluginDocument();

  // Decodes a data: URL image or returns an empty image in case of failure.
  SkBitmap ImageFromDataUrl(const GURL&) const;

  // Inserts a string of CSS in a particular frame. |id| can be specified to
  // give the CSS style element an id, and (if specified) will replace the
  // element with the same id.
  void InsertCSS(const std::wstring& frame_xpath,
                 const std::string& css,
                 const std::string& id);

  // Returns false unless this is a top-level navigation that crosses origins.
  bool IsNonLocalTopLevelNavigation(const GURL& url,
                                    WebKit::WebFrame* frame,
                                    WebKit::WebNavigationType type);

  void LoadNavigationErrorPage(WebKit::WebFrame* frame,
                               const WebKit::WebURLRequest& failed_request,
                               const WebKit::WebURLError& error,
                               const std::string& html,
                               bool replace);

  // Logs the navigation state to the console.
  void LogNavigationState(const NavigationState* state,
                          const WebKit::WebDataSource* ds) const;

  bool MaybeLoadAlternateErrorPage(WebKit::WebFrame* frame,
                                   const WebKit::WebURLError& error,
                                   bool replace);

  void Print(WebKit::WebFrame* frame, bool script_initiated);

  // Returns whether the page associated with |document| is a candidate for
  // translation.  Some pages can explictly specify (via a meta-tag) that they
  // should not be translated.
  bool IsPageTranslatable(WebKit::WebDocument* document);

  // Starts nav_state_sync_timer_ if it isn't already running.
  void StartNavStateSyncTimerIfNecessary();

  // Dispatches the current navigation state to the browser. Called on a
  // periodic timer so we don't send too many messages.
  void SyncNavigationState();

#if defined(OS_LINUX)
  void UpdateFontRenderingFromRendererPrefs();
#else
  void UpdateFontRenderingFromRendererPrefs() {}
#endif

  // Update the target url and tell the browser that the target URL has changed.
  // If |url| is empty, show |fallback_url|.
  void UpdateTargetURL(const GURL& url, const GURL& fallback_url);

  // ---------------------------------------------------------------------------
  // ADDING NEW FUNCTIONS? Please keep private functions alphabetized and put
  // it in the same order in the .cc file as it was in the header.
  // ---------------------------------------------------------------------------

  // Settings ------------------------------------------------------------------

  WebPreferences webkit_preferences_;
  RendererPreferences renderer_preferences_;

  HostContentSettings host_content_settings_;
  HostZoomLevels host_zoom_levels_;

  // Whether content state (such as form state, scroll position and page
  // contents) should be sent to the browser immediately. This is normally
  // false, but set to true by some tests.
  bool send_content_state_immediately_;

  // Stores if loading of images, scripts, and plugins is allowed.
  ContentSettings current_content_settings_;

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

  // We need to prevent windows from closing themselves with a window.close()
  // call while a blocked popup notification is being displayed. We cannot
  // synchronously query the Browser process. We cannot wait for the Browser
  // process to send a message to us saying that a blocked popup notification
  // is being displayed. We instead assume that when we create a window off
  // this RenderView, that it is going to be blocked until we get a message
  // from the Browser process telling us otherwise.
  bool script_can_close_;

  // Loading state -------------------------------------------------------------

  // True if the top level frame is currently being loaded.
  bool is_loading_;

  // The gesture that initiated the current navigation.
  NavigationGesture navigation_gesture_;

  // Used for popups.
  bool opened_by_user_gesture_;
  GURL creator_url_;

  // Whether this RenderView was created by a frame that was suppressing its
  // opener. If so, we may want to load pages in a separate process.  See
  // decidePolicyForNavigation for details.
  bool opener_suppressed_;

  // If we are handling a top-level client-side redirect, this tracks the URL
  // of the page that initiated it. Specifically, when a load is committed this
  // is used to determine if that load originated from a client-side redirect.
  // It is empty if there is no top-level client-side redirect.
  GURL completed_client_redirect_src_;

  // Stores if images, scripts, and plugins have actually been blocked.
  bool content_blocked_[CONTENT_SETTINGS_NUM_TYPES];

  // Holds state pertaining to a navigation that we initiated.  This is held by
  // the WebDataSource::ExtraData attribute.  We use pending_navigation_state_
  // as a temporary holder for the state until the WebDataSource corresponding
  // to the new navigation is created.  See DidCreateDataSource.
  scoped_ptr<NavigationState> pending_navigation_state_;

  // Timer used to delay the updating of nav state (see SyncNavigationState).
  base::OneShotTimer<RenderView> nav_state_sync_timer_;

  // Page IDs ------------------------------------------------------------------
  //
  // Page IDs allow the browser to identify pages in each renderer process for
  // keeping back/forward history in sync.

  // ID of the current page.  Note that this is NOT updated for every main
  // frame navigation, only for "regular" navigations that go into session
  // history. In particular, client redirects, like the page cycler uses
  // (document.location.href="foo") do not count as regular navigations and do
  // not increment the page id.
  int32 page_id_;

  // Indicates the ID of the last page that we sent a FrameNavigate to the
  // browser for. This is used to determine if the most recent transition
  // generated a history entry (less than page_id_), or not (equal to or
  // greater than). Note that this will be greater than page_id_ if the user
  // goes back.
  int32 last_page_id_sent_to_browser_;

  // Page_id from the last page we indexed. This prevents us from indexing the
  // same page twice in a row.
  int32 last_indexed_page_id_;

  // The next available page ID to use. This ensures that the page IDs are
  // globally unique in the renderer.
  static int32 next_page_id_;

  // Page info -----------------------------------------------------------------

  // The last gotten main frame's encoding.
  std::string last_encoding_name_;

  int history_list_offset_;
  int history_list_length_;

  // True if the page has any frame-level unload or beforeunload listeners.
  bool has_unload_listener_;

#if defined(OS_MACOSX)
  // True if the current RenderView has been assigned a document tag.
  bool has_document_tag_;
#endif

  int document_tag_;

  // Site isolation metrics flags. These are per-page-load counts, reset to 0
  // in OnClosePage.
  int cross_origin_access_count_;
  int same_origin_access_count_;

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

  // True if the browser is showing the spelling panel for us.
  bool spelling_panel_visible_;

  // The text selection the last time DidChangeSelection got called.
  std::string last_selection_;

  // View ----------------------------------------------------------------------

  // Type of view attached with RenderView.  See view_types.h
  ViewType::Type view_type_;

  // Id number of browser window which RenderView is attached to. This is used
  // for extensions.
  int browser_window_id_;

  // Cache the preferred size of the page in order to prevent sending the IPC
  // when layout() recomputes but doesn't actually change sizes.
  gfx::Size preferred_size_;

  // Nasty hack. WebKit does not send us events when the preferred size changes,
  // so we must poll it. See also:
  // https://bugs.webkit.org/show_bug.cgi?id=32807.
  base::RepeatingTimer<RenderView> preferred_size_change_timer_;

  // Plugins -------------------------------------------------------------------

  // Remember the first uninstalled plugin, so that we can ask the plugin
  // to install itself when user clicks on the info bar.
  base::WeakPtr<webkit_glue::WebPluginDelegate> first_default_plugin_;

  PepperPluginDelegateImpl pepper_delegate_;

  // All the currently active plugin delegates for this RenderView; kept so that
  // we can enumerate them to send updates about things like window location
  // or tab focus and visibily. These are non-owning references.
  std::set<WebPluginDelegateProxy*> plugin_delegates_;

  // A list of all Pepper v1 plugins that we've created that haven't been
  // destroyed yet. Pepper v2 plugins are tracked by the pepper_delegate_.
  std::set<WebPluginDelegatePepper*> current_oldstyle_pepper_plugins_;

  // Helper objects ------------------------------------------------------------

  ScopedRunnableMethodFactory<RenderView> page_info_method_factory_;
  ScopedRunnableMethodFactory<RenderView> autofill_method_factory_;

  // Responsible for translating the page contents to other languages.
  TranslateHelper translate_helper_;

  // Responsible for automatically filling login and password textfields.
  scoped_ptr<PasswordAutocompleteManager> password_autocomplete_manager_;

  // Responsible for filling forms (AutoFill) and single text entries
  // (Autocomplete).
  scoped_ptr<AutoFillHelper> autofill_helper_;

  // Tracks when text input controls get clicked.
  // IMPORTANT: this should be declared after autofill_helper_ and
  // password_autocomplete_manager_ so the tracker is deleted first (so we won't
  // run the risk of notifying deleted objects).
  scoped_ptr<PageClickTracker> page_click_tracker_;

  RendererWebCookieJarImpl cookie_jar_;

  // Provides access to this renderer from the remote Inspector UI.
  scoped_ptr<DevToolsAgent> devtools_agent_;

  // DevToolsClient for renderer hosting developer tools UI. It's NULL for other
  // render views.
  scoped_ptr<DevToolsClient> devtools_client_;

  // Holds a reference to the service which provides desktop notifications.
  scoped_ptr<NotificationProvider> notification_provider_;

  // PrintWebViewHelper handles printing.  Note that this object is constructed
  // when printing for the first time but only destroyed with the RenderView.
  scoped_ptr<PrintWebViewHelper> print_helper_;

  scoped_refptr<AudioMessageFilter> audio_message_filter_;

  // The geolocation dispatcher attached to this view, lazily initialized.
  scoped_ptr<GeolocationDispatcher> geolocation_dispatcher_;

  // Handles accessibility requests into the renderer side, as well as
  // maintains the cache and other features of the accessibility tree.
  scoped_ptr<WebKit::WebAccessibilityCache> accessibility_;

  // Collect renderer accessibility notifications until they are ready to be
  // sent to the browser.
  std::vector<ViewHostMsg_AccessibilityNotification_Params>
      pending_accessibility_notifications_;

  // The speech dispatcher attached to this view, lazily initialized.
  scoped_ptr<SpeechInputDispatcher> speech_input_dispatcher_;

  // Device orientation dispatcher attached to this view; lazily initialized.
  scoped_ptr<DeviceOrientationDispatcher> device_orientation_dispatcher_;

  // Misc ----------------------------------------------------------------------

  // The current and pending file chooser completion objects. If the queue is
  // nonempty, the first item represents the currently running file chooser
  // callback, and the remaining elements are the other file chooser completion
  // still waiting to be run (in order).
  struct PendingFileChooser;
  std::deque< linked_ptr<PendingFileChooser> > file_chooser_completions_;

  // Resource message queue. Used to queue up resource IPCs if we need
  // to wait for an ACK from the browser before proceeding.
  std::queue<IPC::Message*> queued_resource_messages_;

  std::queue<linked_ptr<ViewMsg_ExecuteCode_Params> >
      pending_code_execution_queue_;

  // ImageResourceFetchers schedule via DownloadImage.
  typedef std::set<webkit_glue::ImageResourceFetcher*> ImageResourceFetcherSet;
  ImageResourceFetcherSet image_fetchers_;

  // The SessionStorage namespace that we're assigned to has an ID, and that ID
  // is passed to us upon creation.  WebKit asks for this ID upon first use and
  // uses it whenever asking the browser process to allocate new storage areas.
  int64 session_storage_namespace_id_;

  // The total number of unrequested popups that exist and can be followed back
  // to a common opener. This count is shared among all RenderViews created
  // with createView(). All popups are treated as unrequested until
  // specifically instructed otherwise by the Browser process.
  scoped_refptr<SharedRenderViewCounter> shared_popup_counter_;

  // Whether this is a top level window (instead of a popup). Top level windows
  // shouldn't count against their own |shared_popup_counter_|.
  bool decrement_shared_popup_at_destruction_;

  // If the browser hasn't sent us an ACK for the last FindReply we sent
  // to it, then we need to queue up the message (keeping only the most
  // recent message if new ones come in).
  scoped_ptr<IPC::Message> queued_find_reply_message_;

  // Stores edit commands associated to the next key event.
  // Shall be cleared as soon as the next key event is processed.
  EditCommands edit_commands_;

  // Allows JS to access DOM automation. The JS object is only exposed when the
  // DOM automation bindings are enabled.
  scoped_ptr<DomAutomationController> dom_automation_controller_;

  // Allows DOM UI pages (new tab page, etc.) to talk to the browser. The JS
  // object is only exposed when DOM UI bindings are enabled.
  scoped_ptr<DOMUIBindings> dom_ui_bindings_;

  // External host exposed through automation controller.
  scoped_ptr<ExternalHostBindings> external_host_bindings_;

  // Pending openFileSystem completion objects.
  struct PendingOpenFileSystem;
  IDMap<PendingOpenFileSystem, IDMapOwnPointer>
      pending_file_system_requests_;

  // ---------------------------------------------------------------------------
  // ADDING NEW DATA? Please see if it fits appropriately in one of the above
  // sections rather than throwing it randomly at the end. If you're adding a
  // bunch of stuff, you should probably create a helper class and put your
  // data and methods on that to avoid bloating RenderView more.
  // ---------------------------------------------------------------------------

  DISALLOW_COPY_AND_ASSIGN(RenderView);
};

#endif  // CHROME_RENDERER_RENDER_VIEW_H_
