// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDER_VIEW_H_
#define CHROME_RENDERER_RENDER_VIEW_H_

#include <map>
#include <set>
#include <string>
#include <queue>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/gfx/point.h"
#include "base/gfx/rect.h"
#include "base/id_map.h"
#include "base/linked_ptr.h"
#include "base/shared_memory.h"
#include "base/timer.h"
#include "base/values.h"
#include "base/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/common/edit_command.h"
#include "chrome/common/navigation_gesture.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/page_zoom.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/renderer_preferences.h"
#include "chrome/common/view_types.h"
#include "chrome/renderer/automation/dom_automation_controller.h"
#include "chrome/renderer/dom_ui_bindings.h"
#include "chrome/renderer/extensions/extension_process_bindings.h"
#include "chrome/renderer/external_host_bindings.h"
#include "chrome/renderer/notification_provider.h"
#include "chrome/renderer/render_widget.h"
#include "chrome/renderer/render_view_visitor.h"
#include "chrome/renderer/translate/page_translator.h"
#include "chrome/renderer/translate/text_translator_impl.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "testing/gtest/include/gtest/gtest_prod.h"
#include "third_party/WebKit/WebKit/chromium/public/WebConsoleMessage.h"
#include "third_party/WebKit/WebKit/chromium/public/WebContextMenuData.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrameClient.h"
#include "third_party/WebKit/WebKit/chromium/public/WebMediaPlayerAction.h"
#include "third_party/WebKit/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPageSerializerClient.h"
#include "third_party/WebKit/WebKit/chromium/public/WebTextDirection.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"
#include "third_party/WebKit/WebKit/chromium/public/WebViewClient.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/image_resource_fetcher.h"
#include "webkit/glue/password_form_dom_manager.h"
#include "webkit/glue/webaccessibility.h"
#include "webkit/glue/webplugin_page_delegate.h"
#include "webkit/glue/webpreferences.h"

#if defined(OS_WIN)
// RenderView is a diamond-shaped hierarchy, with WebWidgetClient at the root.
// VS warns when we inherit the WebWidgetClient method implementations from
// RenderWidget.  It's safe to ignore that warning.
#pragma warning(disable: 4250)
#endif

class AudioMessageFilter;
class DictionaryValue;
class DevToolsAgent;
class DevToolsClient;
class FilePath;
class GURL;
class ListValue;
class NavigationState;
class PrintWebViewHelper;
class WebPluginDelegateProxy;
struct ContextMenuMediaParams;
struct ThumbnailScore;
struct ViewMsg_ClosePage_Params;
struct ViewMsg_Navigate_Params;
struct WebDropData;

namespace base {
class WaitableEvent;
}

namespace webkit_glue {
struct FileUploadData;
}

namespace WebKit {
class WebAccessibilityCache;
class WebDataSource;
class WebDragData;
class WebMediaPlayer;
class WebMediaPlayerClient;
class WebStorageNamespace;
class WebURLRequest;
struct WebFileChooserParams;
struct WebFindOptions;
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
  // Visit all RenderViews with a live WebView (i.e., RenderViews that have
  // been closed but not yet destroyed are excluded).
  static void ForEach(RenderViewVisitor* visitor);

  // Returns the RenderView containing the given WebView.
  static RenderView* FromWebView(WebKit::WebView* webview);

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
      int64 session_storage_namespace_id);

  // Sets the "next page id" counter.
  static void SetNextPageID(int32 next_page_id);

  // May return NULL when the view is closing.
  WebKit::WebView* webview() const {
    return static_cast<WebKit::WebView*>(webwidget());
  }

  gfx::NativeViewId host_window() const {
    return host_window_;
  }

  int browser_window_id() {
    return browser_window_id_;
  }

  ViewType::Type view_type() {
    return view_type_;
  }

  PrintWebViewHelper* print_helper() {
    return print_helper_.get();
  }

  // IPC::Channel::Listener
  virtual void OnMessageReceived(const IPC::Message& msg);

  // WebViewDelegate
  virtual void LoadNavigationErrorPage(
      WebKit::WebFrame* frame,
      const WebKit::WebURLRequest& failed_request,
      const WebKit::WebURLError& error,
      const std::string& html,
      bool replace);
  virtual void OnMissingPluginStatus(
      WebPluginDelegateProxy* delegate,
      int status);
  virtual void UserMetricsRecordAction(const std::string& action);
  virtual void DnsPrefetch(const std::vector<std::string>& host_names);

  // WebKit::WebViewClient
  virtual WebKit::WebView* createView(WebKit::WebFrame* creator);
  virtual WebKit::WebWidget* createPopupMenu(bool activatable);
  virtual WebKit::WebWidget* createPopupMenu(
      const WebKit::WebPopupMenuInfo& info);
  virtual WebKit::WebStorageNamespace* createSessionStorageNamespace();
  virtual void didAddMessageToConsole(
      const WebKit::WebConsoleMessage& message,
      const WebKit::WebString& source_name, unsigned source_line);
  virtual void printPage(WebKit::WebFrame* frame);
  virtual WebKit::WebNotificationPresenter* notificationPresenter() {
    return notification_provider_.get();
  }
  virtual void didStartLoading();
  virtual void didStopLoading();
  virtual bool isSelectTrailingWhitespaceEnabled();
  virtual void setInputMethodEnabled(bool enabled);
  virtual void didChangeSelection(bool is_selection_empty);
  virtual void didExecuteCommand(const WebKit::WebString& command_name);
  virtual bool handleCurrentKeyboardEvent();
  virtual void spellCheck(
      const WebKit::WebString& text, int& offset, int& length);
  virtual WebKit::WebString autoCorrectWord(
      const WebKit::WebString& misspelled_word);
  virtual void showSpellingUI(bool show);
  virtual bool isShowingSpellingUI();
  virtual void updateSpellingUIWithMisspelledWord(
      const WebKit::WebString& word);
  virtual bool runFileChooser(
      const WebKit::WebFileChooserParams& params,
      WebKit::WebFileChooserCompletion* chooser_completion);
  virtual void runModalAlertDialog(
      WebKit::WebFrame* frame, const WebKit::WebString& message);
  virtual bool runModalConfirmDialog(
      WebKit::WebFrame* frame, const WebKit::WebString& message);
  virtual bool runModalPromptDialog(
      WebKit::WebFrame* frame, const WebKit::WebString& message,
      const WebKit::WebString& default_value, WebKit::WebString* actual_value);
  virtual bool runModalBeforeUnloadDialog(
      WebKit::WebFrame* frame, const WebKit::WebString& message);
  virtual void showContextMenu(
      WebKit::WebFrame* frame, const WebKit::WebContextMenuData& data);
  virtual void setStatusText(const WebKit::WebString& text);
  virtual void setMouseOverURL(const WebKit::WebURL& url);
  virtual void setKeyboardFocusURL(const WebKit::WebURL& url);
  virtual void setToolTipText(
      const WebKit::WebString& text, WebKit::WebTextDirection hint);
  virtual void startDragging(
      const WebKit::WebPoint& from, const WebKit::WebDragData& data,
      WebKit::WebDragOperationsMask mask);
  virtual bool acceptsLoadDrops();
  virtual void focusNext();
  virtual void focusPrevious();
  virtual void navigateBackForwardSoon(int offset);
  virtual int historyBackListCount();
  virtual int historyForwardListCount();
  virtual void didAddHistoryItem();
  virtual void focusAccessibilityObject(
      const WebKit::WebAccessibilityObject& acc_obj);
  virtual void didUpdateInspectorSettings();
  virtual void queryAutofillSuggestions(
      const WebKit::WebNode& node, const WebKit::WebString& name,
      const WebKit::WebString& value);
  virtual void removeAutofillSuggestions(
      const WebKit::WebString& name, const WebKit::WebString& value);

  virtual WebKit::WebNotificationPresenter* GetNotificationPresenter() {
    return notification_provider_.get();
  }

  // WebKit::WebWidgetClient
  // Most methods are handled by RenderWidget.
  virtual void show(WebKit::WebNavigationPolicy policy);
  virtual void closeWidgetSoon();
  virtual void runModal();

  // WebKit::WebFrameClient
  virtual WebKit::WebPlugin* createPlugin(
      WebKit::WebFrame* frame, const WebKit::WebPluginParams& params);
  virtual WebKit::WebWorker* createWorker(
      WebKit::WebFrame* frame, WebKit::WebWorkerClient* client);
  virtual WebKit::WebSharedWorker* createSharedWorker(
      WebKit::WebFrame* frame, const WebKit::WebURL& url,
      const WebKit::WebString& name, unsigned long long documentId);
  virtual WebKit::WebMediaPlayer* createMediaPlayer(
      WebKit::WebFrame* frame, WebKit::WebMediaPlayerClient* client);
  virtual void willClose(WebKit::WebFrame* frame);
  virtual void loadURLExternally(
      WebKit::WebFrame* frame, const WebKit::WebURLRequest& request,
      WebKit::WebNavigationPolicy policy);
  virtual WebKit::WebNavigationPolicy decidePolicyForNavigation(
      WebKit::WebFrame* frame, const WebKit::WebURLRequest& request,
      WebKit::WebNavigationType type, const WebKit::WebNode&,
      WebKit::WebNavigationPolicy default_policy, bool is_redirect);
  virtual bool canHandleRequest(
      WebKit::WebFrame* frame, const WebKit::WebURLRequest& request);
  virtual WebKit::WebURLError cannotHandleRequestError(
      WebKit::WebFrame* frame, const WebKit::WebURLRequest& request);
  virtual WebKit::WebURLError cancelledError(
      WebKit::WebFrame* frame, const WebKit::WebURLRequest& request);
  virtual void unableToImplementPolicyWithError(
      WebKit::WebFrame* frame, const WebKit::WebURLError& error);
  virtual void willSubmitForm(WebKit::WebFrame* frame,
      const WebKit::WebFormElement& form);
  virtual void willPerformClientRedirect(
      WebKit::WebFrame* frame, const WebKit::WebURL& from,
      const WebKit::WebURL& to, double interval, double fire_time);
  virtual void didCancelClientRedirect(WebKit::WebFrame* frame);
  virtual void didCompleteClientRedirect(
      WebKit::WebFrame* frame, const WebKit::WebURL& from);
  virtual void didCreateDataSource(
      WebKit::WebFrame* frame, WebKit::WebDataSource* datasource);
  virtual void didStartProvisionalLoad(WebKit::WebFrame* frame);
  virtual void didReceiveServerRedirectForProvisionalLoad(
      WebKit::WebFrame* frame);
  virtual void didFailProvisionalLoad(
      WebKit::WebFrame* frame, const WebKit::WebURLError& error);
  virtual void didReceiveDocumentData(
      WebKit::WebFrame* frame, const char* data, size_t length,
      bool& prevent_default);
  virtual void didCommitProvisionalLoad(
      WebKit::WebFrame* frame, bool is_new_navigation);
  virtual void didClearWindowObject(WebKit::WebFrame* frame);
  virtual void didCreateDocumentElement(WebKit::WebFrame* frame);
  virtual void didReceiveTitle(
      WebKit::WebFrame* frame, const WebKit::WebString& title);
  virtual void didFinishDocumentLoad(WebKit::WebFrame* frame);
  virtual void didHandleOnloadEvents(WebKit::WebFrame* frame);
  virtual void didFailLoad(
      WebKit::WebFrame* frame, const WebKit::WebURLError& error);
  virtual void didFinishLoad(WebKit::WebFrame* frame);
  virtual void didChangeLocationWithinPage(
      WebKit::WebFrame* frame, bool is_new_navigation);
  virtual void didUpdateCurrentHistoryItem(WebKit::WebFrame* frame);
  virtual void assignIdentifierToRequest(
      WebKit::WebFrame* frame, unsigned identifier,
      const WebKit::WebURLRequest& request);
  virtual void willSendRequest(
      WebKit::WebFrame* frame, unsigned identifier,
      WebKit::WebURLRequest& request,
      const WebKit::WebURLResponse& redirect_response);
  virtual void didReceiveResponse(
      WebKit::WebFrame* frame, unsigned identifier,
      const WebKit::WebURLResponse& response);
  virtual void didFinishResourceLoad(
      WebKit::WebFrame* frame, unsigned identifier);
  virtual void didFailResourceLoad(
      WebKit::WebFrame* frame, unsigned identifier,
      const WebKit::WebURLError& error);
  virtual void didLoadResourceFromMemoryCache(
      WebKit::WebFrame* frame, const WebKit::WebURLRequest& request,
      const WebKit::WebURLResponse&);
  virtual void didDisplayInsecureContent(WebKit::WebFrame* frame);
  virtual void didRunInsecureContent(
      WebKit::WebFrame* frame, const WebKit::WebSecurityOrigin& origin);
  virtual bool allowScript(WebKit::WebFrame* frame, bool enabled_per_settings);
  virtual void didExhaustMemoryAvailableForScript(WebKit::WebFrame* frame);
  virtual void didCreateScriptContext(WebKit::WebFrame* frame);
  virtual void didDestroyScriptContext(WebKit::WebFrame* frame);
  virtual void didCreateIsolatedScriptContext(WebKit::WebFrame* frame);
  virtual void didChangeContentsSize(
      WebKit::WebFrame* frame, const WebKit::WebSize& size);
  virtual void didChangeScrollOffset(WebKit::WebFrame* frame);
  virtual void reportFindInPageMatchCount(
      int request_id, int count, bool final_update);
  virtual void reportFindInPageSelection(
      int request_id, int active_match_ordinal, const WebKit::WebRect& sel);

  // webPageSerializerClient
  virtual void didSerializeDataForFrame(const WebKit::WebURL& frame_url,
                                        const WebKit::WebCString& data,
                                        PageSerializationStatus status);

  // webkit_glue::WebPluginPageDelegate
  virtual webkit_glue::WebPluginDelegate* CreatePluginDelegate(
      const GURL& url,
      const std::string& mime_type,
      std::string* actual_mime_type);
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

  // Do not delete directly.  This class is reference counted.
  virtual ~RenderView();

  // Called when a plugin has crashed.
  void PluginCrashed(const FilePath& plugin_path);

  // Called from JavaScript window.external.AddSearchProvider() to add a
  // keyword for a provider described in the given OpenSearch document.
  void AddSearchProvider(const std::string& url);

  // Asks the browser for the CPBrowsingContext associated with this renderer.
  uint32 GetCPBrowsingContext();

  // Dispatches the current navigation state to the browser. Called on a
  // periodic timer so we don't send too many messages.
  void SyncNavigationState();

  // Evaluates a string of JavaScript in a particular frame.
  void EvaluateScript(const std::wstring& frame_xpath,
                      const std::wstring& jscript);

  // Inserts a string of CSS in a particular frame. |id| can be specified to
  // give the CSS style element an id, and (if specified) will replace the
  // element with the same id.
  void InsertCSS(const std::wstring& frame_xpath,
                 const std::string& css,
                 const std::string& id);

  // Whether content state (such as form state and scroll position) should be
  // sent to the browser immediately. This is normally false, but set to true
  // by some tests.
  void set_send_content_state_immediately(bool value) {
    send_content_state_immediately_ = value;
  }

  AudioMessageFilter* audio_message_filter() { return audio_message_filter_; }

  void OnClearFocusedNode();

  void SendExtensionRequest(const std::string& name, const ListValue& args,
                            int request_id, bool has_callback);
  void OnExtensionResponse(int request_id, bool success,
                           const std::string& response,
                           const std::string& error);

  void OnSetExtensionViewMode(const std::string& mode);

  const WebPreferences& webkit_preferences() const {
    return webkit_preferences_;
  }

  // Sends a message and runs a nested message loop.
  bool SendAndRunNestedMessageLoop(IPC::SyncMessage* message);

  // Called when the "idle" user script state has been reached. See
  // UserScript::DOCUMENT_IDLE.
  void OnUserScriptIdleTriggered(WebKit::WebFrame* frame);

  PageTranslator* page_translator() const { return page_translator_.get(); }

  // Returns the ISO 639 language code of the current page (e.g. en, fr, zh).
  // If ISO 639-1 code is not available for the language, ISO 639-2 3-letter code
  // will be returned (e.g. kha for Khasi and und for undtermined). For traditional
  // Chinse, 'zh-TW' will be returned while for simplified Chinse, 'zh' will be
  // returned.
  // TODO(jungshik): Make it return 'he' (the correct ISO 639 code for Hebrew)
  // instead of the obsolete 'iw'. Perhaps, it's also better to return 'zh-Hans'
  // (or 'zh-CN') for Simplified Chinese instead of 'zh' to be aligned with
  // 'zh-TW' for Traditional Chinse.
  std::string DetectLanguage();

 protected:
  // RenderWidget overrides:
  virtual void Close();
  virtual void OnResize(const gfx::Size& new_size,
                        const gfx::Rect& resizer_rect);
  virtual void DidPaint();
  virtual void DidHandleKeyEvent();

 private:
  // For unit tests.
  friend class RenderViewTest;
  FRIEND_TEST(RenderViewTest, OnLoadAlternateHTMLText);
  FRIEND_TEST(RenderViewTest, OnNavStateChanged);
  FRIEND_TEST(RenderViewTest, OnImeStateChanged);
  FRIEND_TEST(RenderViewTest, ImeComposition);
  FRIEND_TEST(RenderViewTest, OnSetTextDirection);
  FRIEND_TEST(RenderViewTest, OnPrintPages);
  FRIEND_TEST(RenderViewTest, PrintWithIframe);
  FRIEND_TEST(RenderViewTest, PrintLayoutTest);
  FRIEND_TEST(RenderViewTest, OnHandleKeyboardEvent);
  FRIEND_TEST(RenderViewTest, InsertCharacters);
#if defined(OS_MACOSX)
  FRIEND_TEST(RenderViewTest, MacTestCmdUp);
#endif

  typedef std::map<std::string, int> HostZoomLevels;

  explicit RenderView(RenderThreadBase* render_thread,
                      const WebPreferences& webkit_preferences,
                      int64 session_storage_namespace_id);

  // Initializes this view with the given parent and ID. The |routing_id| can be
  // set to 'MSG_ROUTING_NONE' if the true ID is not yet known. In this case,
  // CompleteInit must be called later with the true ID.
  void Init(gfx::NativeViewId parent,
            int32 opener_id,
            const RendererPreferences& renderer_prefs,
            SharedRenderViewCounter* counter,
            int32 routing_id);

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

  // Called to retrieve the text from the given frame contents, the page text
  // up to the maximum amount will be placed into the given buffer
  void CaptureText(WebKit::WebFrame* frame, std::wstring* contents);

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

  // Adds search provider from the given OpenSearch description URL as a
  // keyword search.
  void AddGURLSearchProvider(const GURL& osd_url, bool autodetected);

  // RenderView IPC message handlers
  void SendThumbnail();
  void SendSnapshot();
  void OnPrintPages();
  void OnPrintingDone(int document_cookie, bool success);
  void OnNavigate(const ViewMsg_Navigate_Params& params);
  void OnStop();
  void OnLoadAlternateHTMLText(const std::string& html_contents,
                               bool new_navigation,
                               const GURL& display_url,
                               const std::string& security_info);
  void OnStopFinding(bool clear_selection);
  void OnFindReplyAck();
  void OnUpdateTargetURLAck();
  void OnUndo();
  void OnRedo();
  void OnCut();
  void OnCopy();
#if defined(OS_MACOSX)
  void OnCopyToFindPboard();
#endif
  void OnPaste();
  void OnReplace(const string16& text);
  void OnAdvanceToNextMisspelling();
  void OnToggleSpellPanel(bool is_currently_visible);
  void OnToggleSpellCheck();
  void OnDelete();
  void OnSelectAll();
  void OnCopyImageAt(int x, int y);
  void OnExecuteEditCommand(const std::string& name, const std::string& value);
  void OnSetEditCommandsForNextKeyEvent(const EditCommands& edit_commands);
  void OnSetupDevToolsClient();
  void OnCancelDownload(int32 download_id);
  void OnFind(int request_id, const string16&, const WebKit::WebFindOptions&);
  void OnDeterminePageLanguage();
  void OnZoom(PageZoom::Function function);
  void OnSetZoomLevelForLoadingHost(std::string host, int zoom_level);
  void OnSetPageEncoding(const std::string& encoding_name);
  void OnResetPageEncodingToDefault();
  void OnGetAllSavableResourceLinksForCurrentPage(const GURL& page_url);
  void OnGetSerializedHtmlDataForCurrentPageWithLocalLinks(
      const std::vector<GURL>& links,
      const std::vector<FilePath>& local_paths,
      const FilePath& local_directory_name);
  void OnFormFill(const FormData& form);
  void OnFillPasswordForm(
      const webkit_glue::PasswordFormDomManager::FillData& form_data);
  void OnDragTargetDragEnter(const WebDropData& drop_data,
                             const gfx::Point& client_pt,
                             const gfx::Point& screen_pt,
                             WebKit::WebDragOperationsMask operations_allowed);
  void OnDragTargetDragOver(const gfx::Point& client_pt,
                            const gfx::Point& screen_pt,
                            WebKit::WebDragOperationsMask operations_allowed);
  void OnDragTargetDragLeave();
  void OnDragTargetDrop(const gfx::Point& client_pt,
                        const gfx::Point& screen_pt);
  void OnAllowBindings(int enabled_bindings_flags);
  void OnSetDOMUIProperty(const std::string& name, const std::string& value);
  void OnSetInitialFocus(bool reverse);
  void OnUpdateWebPreferences(const WebPreferences& prefs);
  void OnSetAltErrorPageURL(const GURL& gurl);

  void OnDownloadFavIcon(int id, const GURL& image_url, int image_size);

  void OnGetApplicationInfo(int page_id);

  void OnScriptEvalRequest(const std::wstring& frame_xpath,
                           const std::wstring& jscript);
  void OnCSSInsertRequest(const std::wstring& frame_xpath,
                          const std::string& css,
                          const std::string& id);
  void OnAddMessageToConsole(const string16& frame_xpath,
                             const string16& message,
                             const WebKit::WebConsoleMessage::Level&);
  void OnReservePageIDRange(int size_of_range);

  void OnDragSourceEndedOrMoved(const gfx::Point& client_point,
                                const gfx::Point& screen_point,
                                bool ended,
                                WebKit::WebDragOperation drag_operation);
  void OnDragSourceSystemDragEnded();
  void OnInstallMissingPlugin();
  void OnFileChooserResponse(const std::vector<FilePath>& file_names);
  void OnEnableViewSourceMode();
  void OnEnablePreferredSizeChangedMode();
  void OnSetRendererPrefs(const RendererPreferences& renderer_prefs);
  void OnMediaPlayerActionAt(const gfx::Point& location,
                             const WebKit::WebMediaPlayerAction& action);
  void OnNotifyRendererViewType(ViewType::Type view_type);
  void OnUpdateBrowserWindowId(int window_id);
  void OnExecuteCode(const ViewMsg_ExecuteCode_Params& params);
  void ExecuteCodeImpl(WebKit::WebFrame* frame,
                       const ViewMsg_ExecuteCode_Params& params);
  void OnUpdateBackForwardListCount(int back_list_count,
                                    int forward_list_count);
  void OnGetAccessibilityInfo(
      const webkit_glue::WebAccessibility::InParams& in_params,
      webkit_glue::WebAccessibility::OutParams* out_params);
  void OnClearAccessibilityInfo(int acc_obj_id, bool clear_all);

  void OnExtensionMessageInvoke(const std::string& function_name,
                                const ListValue& args);

  void OnMoveOrResizeStarted();

  // Checks if the RenderView should close, runs the beforeunload handler and
  // sends ViewMsg_ShouldClose to the browser.
  void OnMsgShouldClose();

  // Runs the onunload handler and closes the page, replying with ClosePage_ACK
  // (with the given RPH and request IDs, to help track the request).
  void OnClosePage(const ViewMsg_ClosePage_Params& params);

  // Notification about ui theme changes.
  void OnThemeChanged();

  // Notification that we have received autofill suggestion.
  void OnQueryFormFieldAutofillAck(
      int query_id,
      const std::vector<string16>& suggestions,
      int default_suggestions_index);

  // Message that the popup notification has been shown or hidden.
  void OnPopupNotificationVisibilityChanged(bool visible);

  // Handles messages posted from automation.
  void OnMessageFromExternalHost(const std::string& message,
                                 const std::string& origin,
                                 const std::string& target);

  // Message that we should no longer be part of the current popup window
  // grouping, and should form our own grouping.
  void OnDisassociateFromPopupCount();

  // Sends the selection text to the browser.
  void OnRequestSelectionText();

  // Handle message to make the RenderView transparent and render it on top of
  // a custom background.
  void OnSetBackground(const SkBitmap& background);

  // Activate/deactivate the RenderView (i.e., set its controls' tint
  // accordingly, etc.).
  void OnSetActive(bool active);

  // Execute custom context menu action.
  void OnCustomContextMenuAction(unsigned action);

  // Message that provides the translated text for a request.
  void OnTranslateTextResponse(int work_id,
                               int error_id,
                               const std::vector<string16>& text_chunks);

  // Exposes the DOMAutomationController object that allows JS to send
  // information to the browser process.
  void BindDOMAutomationController(WebKit::WebFrame* webframe);

  // Creates DevToolsClient and sets up JavaScript bindings for developer tools
  // UI that is going to be hosted by this RenderView.
  void CreateDevToolsClient();

  // Locates a sub frame with given xpath
  WebKit::WebFrame* GetChildFrame(const std::wstring& frame_xpath) const;

  // Get all child frames of parent_frame, returned by frames_vector.
  bool GetAllChildFrames(WebKit::WebFrame* parent_frame,
                         std::vector<WebKit::WebFrame* >* frames_vector) const;

  // Requests to download an image. When done, the RenderView is
  // notified by way of DidDownloadImage. Returns true if the request was
  // successfully started, false otherwise. id is used to uniquely identify the
  // request and passed back to the DidDownloadImage method. If the image has
  // multiple frames, the frame whose size is image_size is returned. If the
  // image doesn't have a frame at the specified size, the first is returned.
  bool DownloadImage(int id, const GURL& image_url, int image_size);

  // This callback is triggered when DownloadImage completes, either
  // succesfully or with a failure. See DownloadImage for more details.
  void DidDownloadImage(webkit_glue::ImageResourceFetcher* fetcher,
                        const SkBitmap& image);

  enum ErrorPageType {
    DNS_ERROR,
    HTTP_404,
    CONNECTION_ERROR,
  };

  // Alternate error page helpers.
  GURL GetAlternateErrorPageURL(
      const GURL& failed_url, ErrorPageType error_type);
  bool MaybeLoadAlternateErrorPage(
      WebKit::WebFrame* frame, const WebKit::WebURLError& error, bool replace);
  std::string GetAltHTMLForTemplate(
      const DictionaryValue& error_strings, int template_resource_id) const;
  void AltErrorPageFinished(
      WebKit::WebFrame* frame, const WebKit::WebURLError& original_error,
      const std::string& html);

  // Decodes a data: URL image or returns an empty image in case of failure.
  SkBitmap ImageFromDataUrl(const GURL&) const;

  void DumpLoadHistograms() const;

  // Logs the navigation state to the console.
  void LogNavigationState(const NavigationState* state,
                          const WebKit::WebDataSource* ds) const;

  // Scan the given frame for password forms and send them up to the browser.
  void SendPasswordForms(WebKit::WebFrame* frame);

  void Print(WebKit::WebFrame* frame, bool script_initiated);

#if defined(OS_LINUX)
  void UpdateFontRenderingFromRendererPrefs();
#else
  void UpdateFontRenderingFromRendererPrefs() { }
#endif

  // Inject toolstrip CSS for extension moles and toolstrips.
  void InjectToolstripCSS();

  // Initializes the document_tag_ member if necessary.
  void EnsureDocumentTag();

  // Update the target url and tell the browser that the target URL has changed.
  // If |url| is empty, show |fallback_url|.
  void UpdateTargetURL(const GURL& url, const GURL& fallback_url);

  // Starts nav_state_sync_timer_ if it isn't already running.
  void StartNavStateSyncTimerIfNecessary();

  // Returns the ISO 639_1 language code of the specified |text|, or 'unknown'
  // if it failed.
  // Note this only works on Windows at this time.  It always returns 'unknown'
  // on other platforms.
  static std::string DetermineTextLanguage(const std::wstring& text);

  // Bitwise-ORed set of extra bindings that have been enabled.  See
  // BindingsPolicy for details.
  int enabled_bindings_;

  // DOM Automation Controller CppBoundClass.
  DomAutomationController dom_automation_controller_;

  // Chrome page<->browser messaging CppBoundClass.
  DOMUIBindings dom_ui_bindings_;

  // External host exposed through automation controller.
  ExternalHostBindings external_host_bindings_;

  // The last gotten main frame's encoding.
  std::string last_encoding_name_;

  // The URL we show the user in the status bar. We use this to
  // determine if we want to send a new one (we do not need to send
  // duplicates). It will be equal to either |mouse_over_url_| or |focus_url_|,
  // depending on which was updated last.
  GURL target_url_;
  // The URL the user's mouse is hovering over.
  GURL mouse_over_url_;
  // The URL that has keyboard focus.
  GURL focus_url_;

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

  // The next target URL we want to send to the browser.
  GURL pending_target_url_;

  // Are we loading our top level frame
  bool is_loading_;

  // If we are handling a top-level client-side redirect, this tracks the URL
  // of the page that initiated it. Specifically, when a load is committed this
  // is used to determine if that load originated from a client-side redirect.
  // It is empty if there is no top-level client-side redirect.
  GURL completed_client_redirect_src_;

  // The gesture that initiated the current navigation.
  NavigationGesture navigation_gesture_;

  // Unique id to identify the current page between browser and renderer.
  //
  // Note that this is NOT updated for every main frame navigation, only for
  // "regular" navigations that go into session history. In particular, client
  // redirects, like the page cycler uses (document.location.href="foo") do not
  // count as regular navigations and do not increment the page id.
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

  // Used for popups.
  bool opened_by_user_gesture_;
  GURL creator_url_;

  // The alternate error page URL, if one exists.
  GURL alternate_error_page_url_;

  // Whether this RenderView was created by a frame that was suppressing its
  // opener. If so, we may want to load pages in a separate process.  See
  // decidePolicyForNavigation for details.
  bool opener_suppressed_;

  ScopedRunnableMethodFactory<RenderView> method_factory_;

  // Timer used to delay the updating of nav state (see SyncNavigationState).
  base::OneShotTimer<RenderView> nav_state_sync_timer_;

  // Remember the first uninstalled plugin, so that we can ask the plugin
  // to install itself when user clicks on the info bar.
  base::WeakPtr<webkit_glue::WebPluginDelegate> first_default_plugin_;

  // If the browser hasn't sent us an ACK for the last FindReply we sent
  // to it, then we need to queue up the message (keeping only the most
  // recent message if new ones come in).
  scoped_ptr<IPC::Message> queued_find_reply_message_;

  // Provides access to this renderer from the remote Inspector UI.
  scoped_ptr<DevToolsAgent> devtools_agent_;

  // DevToolsClient for renderer hosting developer tools UI. It's NULL for other
  // render views.
  scoped_ptr<DevToolsClient> devtools_client_;

  // A pointer to a file chooser completion object. When not empty, file
  // choosing operation is underway.
  WebKit::WebFileChooserCompletion* file_chooser_completion_;

  int history_back_list_count_;
  int history_forward_list_count_;

  // True if the page has any frame-level unload or beforeunload listeners.
  bool has_unload_listener_;

  // The total number of unrequested popups that exist and can be followed back
  // to a common opener. This count is shared among all RenderViews created
  // with createView(). All popups are treated as unrequested until
  // specifically instructed otherwise by the Browser process.
  scoped_refptr<SharedRenderViewCounter> shared_popup_counter_;

  // Whether this is a top level window (instead of a popup). Top level windows
  // shouldn't count against their own |shared_popup_counter_|.
  bool decrement_shared_popup_at_destruction_;

  // TODO(port): revisit once we have accessibility
#if defined(OS_WIN)
  // Handles accessibility requests into the renderer side, as well as
  // maintains the cache and other features of the accessibility tree.
  scoped_ptr<WebKit::WebAccessibilityCache> accessibility_;
#endif

  // Resource message queue. Used to queue up resource IPCs if we need
  // to wait for an ACK from the browser before proceeding.
  std::queue<IPC::Message*> queued_resource_messages_;

  // The id of the last request sent for form field autofill.  Used to ignore
  // out of date responses.
  int autofill_query_id_;

  // The id of the node corresponding to the last request sent for form field
  // autofill.
  WebKit::WebNode autofill_query_node_;

  // We need to prevent windows from closing themselves with a window.close()
  // call while a blocked popup notification is being displayed. We cannot
  // synchronously query the Browser process. We cannot wait for the Browser
  // process to send a message to us saying that a blocked popup notification
  // is being displayed. We instead assume that when we create a window off
  // this RenderView, that it is going to be blocked until we get a message
  // from the Browser process telling us otherwise.
  bool popup_notification_visible_;

  // True if the browser is showing the spelling panel for us.
  bool spelling_panel_visible_;

  // See description above setter.
  bool send_content_state_immediately_;

  scoped_refptr<AudioMessageFilter> audio_message_filter_;

  // The currently selected text. This is currently only updated on Linux, where
  // it's for the selection clipboard.
  std::string selection_text_;

  // Cache the preferred size of the page in order to prevent sending the IPC
  // when layout() recomputes but doesn't actually change sizes.
  gfx::Size preferred_size_;

  // If true, we send IPC messages when |preferred_size_| changes.
  bool send_preferred_size_changes_;

  // The text selection the last time DidChangeSelection got called.
  std::string last_selection_;

  // Hopds a reference to the service which provides desktop notifications.
  scoped_ptr<NotificationProvider> notification_provider_;

  // Holds state pertaining to a navigation that we initiated.  This is held by
  // the WebDataSource::ExtraData attribute.  We use pending_navigation_state_
  // as a temporary holder for the state until the WebDataSource corresponding
  // to the new navigation is created.  See DidCreateDataSource.
  scoped_ptr<NavigationState> pending_navigation_state_;

  // PrintWebViewHelper handles printing.  Note that this object is constructed
  // when printing for the first time but only destroyed with the RenderView.
  scoped_ptr<PrintWebViewHelper> print_helper_;

  RendererPreferences renderer_preferences_;

  // Type of view attached with RenderView, it could be INVALID, TAB_CONTENTS,
  // EXTENSION_TOOLSTRIP, EXTENSION_BACKGROUND_PAGE, DEV_TOOLS_UI.
  ViewType::Type view_type_;

  // Id number of browser window which RenderView is attached to.
  int browser_window_id_;

  std::queue<linked_ptr<ViewMsg_ExecuteCode_Params> >
      pending_code_execution_queue_;

  // page id for the last navigation sent to the browser.
  int32 last_top_level_navigation_page_id_;

#if defined(OS_MACOSX)
  // True if the current RenderView has been assigned a document tag.
  bool has_document_tag_;
#endif

  // Document tag for this RenderView.
  int document_tag_;

  // The settings this render view initialized WebKit with.
  WebPreferences webkit_preferences_;

  // Stores edit commands associated to the next key event.
  // Shall be cleared as soon as the next key event is processed.
  EditCommands edit_commands_;

  // ImageResourceFetchers schedule via DownloadImage.
  typedef std::set<webkit_glue::ImageResourceFetcher*> ImageResourceFetcherSet;
  ImageResourceFetcherSet image_fetchers_;

  typedef std::map<WebKit::WebView*, RenderView*> ViewMap;

  HostZoomLevels host_zoom_levels_;

  // The SessionStorage namespace that we're assigned to has an ID, and that ID
  // is passed to us upon creation.  WebKit asks for this ID upon first use and
  // uses it whenever asking the browser process to allocate new storage areas.
  int64 session_storage_namespace_id_;

  // Page translation related objects.
  TextTranslatorImpl text_translator_;
  scoped_ptr<PageTranslator> page_translator_;

  DISALLOW_COPY_AND_ASSIGN(RenderView);
};

#endif  // CHROME_RENDERER_RENDER_VIEW_H_
