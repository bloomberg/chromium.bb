// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(ENABLE_PEPPER)
#define PEPPER_APIS_ENABLED
#endif

#include "chrome/renderer/render_view.h"

#include <algorithm>
#include <string>
#include <vector>

#include "app/gfx/color_utils.h"
#include "app/gfx/favicon_size.h"
#include "app/gfx/native_widget_types.h"
#include "app/l10n_util.h"
#include "app/message_box_flags.h"
#include "app/resource_bundle.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/field_trial.h"
#include "base/process_util.h"
#include "base/singleton.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "build/build_config.h"
#include "chrome/common/bindings_policy.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/page_zoom.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/renderer_preferences.h"
#include "chrome/common/thumbnail_score.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/about_handler.h"
#include "chrome/renderer/audio_message_filter.h"
#include "chrome/renderer/devtools_agent.h"
#include "chrome/renderer/devtools_client.h"
#include "chrome/renderer/extension_groups.h"
#include "chrome/renderer/extensions/event_bindings.h"
#include "chrome/renderer/extensions/extension_process_bindings.h"
#include "chrome/renderer/extensions/renderer_extension_bindings.h"
#include "chrome/renderer/localized_error.h"
#include "chrome/renderer/media/audio_renderer_impl.h"
#include "chrome/renderer/navigation_state.h"
#include "chrome/renderer/notification_provider.h"
#include "chrome/renderer/plugin_channel_host.h"
#include "chrome/renderer/print_web_view_helper.h"
#include "chrome/renderer/render_process.h"
#include "chrome/renderer/user_script_slave.h"
#include "chrome/renderer/visitedlink_slave.h"
#include "chrome/renderer/webplugin_delegate_pepper.h"
#include "chrome/renderer/webplugin_delegate_proxy.h"
#include "chrome/renderer/webworker_proxy.h"
#include "grit/generated_resources.h"
#include "grit/renderer_resources.h"
#include "net/base/data_url.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "skia/ext/bitmap_platform_device.h"
#include "skia/ext/image_operations.h"
#include "webkit/api/public/WebAccessibilityCache.h"
#include "webkit/api/public/WebAccessibilityObject.h"
#include "webkit/api/public/WebDataSource.h"
#include "webkit/api/public/WebDevToolsAgent.h"
#include "webkit/api/public/WebDragData.h"
#include "webkit/api/public/WebForm.h"
#include "webkit/api/public/WebFrame.h"
#include "webkit/api/public/WebHistoryItem.h"
#include "webkit/api/public/WebNode.h"
#include "webkit/api/public/WebPoint.h"
#include "webkit/api/public/WebRect.h"
#include "webkit/api/public/WebScriptSource.h"
#include "webkit/api/public/WebSearchableFormData.h"
#include "webkit/api/public/WebSecurityOrigin.h"
#include "webkit/api/public/WebSize.h"
#include "webkit/api/public/WebString.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/api/public/WebURLError.h"
#include "webkit/api/public/WebURLRequest.h"
#include "webkit/api/public/WebURLResponse.h"
#include "webkit/api/public/WebVector.h"
#include "webkit/appcache/appcache_interfaces.h"
#include "webkit/default_plugin/default_plugin_shared.h"
#include "webkit/glue/glue_serialize.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/dom_operations.h"
#include "webkit/glue/dom_serializer.h"
#include "webkit/glue/image_decoder.h"
#include "webkit/glue/media/buffered_data_source.h"
#include "webkit/glue/media/simple_data_source.h"
#include "webkit/glue/password_form.h"
#include "webkit/glue/plugins/plugin_list.h"
#include "webkit/glue/plugins/webplugin_delegate_impl.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webmediaplayer_impl.h"
#include "webkit/glue/webplugin_impl.h"

#if defined(OS_WIN)
// TODO(port): these files are currently Windows only because they concern:
//   * theming
#include "app/gfx/native_theme_win.h"
#endif

using base::Time;
using base::TimeDelta;
using webkit_glue::AltErrorPageResourceFetcher;
using webkit_glue::FormFieldValues;
using webkit_glue::ImageResourceFetcher;
using webkit_glue::PasswordForm;
using webkit_glue::PasswordFormDomManager;
using WebKit::WebAccessibilityCache;
using WebKit::WebAccessibilityObject;
using WebKit::WebColor;
using WebKit::WebColorName;
using WebKit::WebConsoleMessage;
using WebKit::WebContextMenuData;
using WebKit::WebData;
using WebKit::WebDataSource;
using WebKit::WebDevToolsAgent;
using WebKit::WebDragData;
using WebKit::WebDragOperation;
using WebKit::WebDragOperationsMask;
using WebKit::WebEditingAction;
using WebKit::WebFindOptions;
using WebKit::WebForm;
using WebKit::WebFrame;
using WebKit::WebHistoryItem;
using WebKit::WebMediaPlayer;
using WebKit::WebMediaPlayerAction;
using WebKit::WebMediaPlayerClient;
using WebKit::WebNavigationPolicy;
using WebKit::WebNavigationType;
using WebKit::WebNode;
using WebKit::WebPlugin;
using WebKit::WebPluginParams;
using WebKit::WebPoint;
using WebKit::WebPopupMenuInfo;
using WebKit::WebRange;
using WebKit::WebRect;
using WebKit::WebScriptSource;
using WebKit::WebSearchableFormData;
using WebKit::WebSecurityOrigin;
using WebKit::WebSettings;
using WebKit::WebSize;
using WebKit::WebString;
using WebKit::WebTextAffinity;
using WebKit::WebTextDirection;
using WebKit::WebURL;
using WebKit::WebURLError;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;
using WebKit::WebVector;
using WebKit::WebView;
using WebKit::WebWidget;
using WebKit::WebWorker;
using WebKit::WebWorkerClient;

//-----------------------------------------------------------------------------

// define to write the time necessary for thumbnail/DOM text retrieval,
// respectively, into the system debug log
// #define TIME_BITMAP_RETRIEVAL
// #define TIME_TEXT_RETRIEVAL

// maximum number of characters in the document to index, any text beyond this
// point will be clipped
static const size_t kMaxIndexChars = 65535;

// Size of the thumbnails that we'll generate
static const int kThumbnailWidth = 212;
static const int kThumbnailHeight = 132;

// Delay in milliseconds that we'll wait before capturing the page contents
// and thumbnail.
static const int kDelayForCaptureMs = 500;

// Typically, we capture the page data once the page is loaded.
// Sometimes, the page never finishes to load, preventing the page capture
// To workaround this problem, we always perform a capture after the following
// delay.
static const int kDelayForForcedCaptureMs = 6000;

// Time, in seconds, we delay before sending content state changes (such as form
// state and scroll position) to the browser. We delay sending changes to avoid
// spamming the browser.
// To avoid having tab/session restore require sending a message to get the
// current content state during tab closing we use a shorter timeout for the
// foreground renderer. This means there is a small window of time from which
// content state is modified and not sent to session restore, but this is
// better than having to wake up all renderers during shutdown.
static const int kDelaySecondsForContentStateSyncHidden = 5;
static const int kDelaySecondsForContentStateSync = 1;

// The maximum number of popups that can be spawned from one page.
static const int kMaximumNumberOfUnacknowledgedPopups = 25;

static const char* const kUnreachableWebDataURL =
    "chrome://chromewebdata/";

static const char* const kBackForwardNavigationScheme = "history";

static void GetRedirectChain(WebDataSource* ds, std::vector<GURL>* result) {
  WebVector<WebURL> urls;
  ds->redirectChain(urls);
  result->reserve(urls.size());
  for (size_t i = 0; i < urls.size(); ++i)
    result->push_back(urls[i]);
}

///////////////////////////////////////////////////////////////////////////////

int32 RenderView::next_page_id_ = 1;

RenderView::RenderView(RenderThreadBase* render_thread,
                       const WebPreferences& webkit_preferences)
    : RenderWidget(render_thread, true),
      enabled_bindings_(0),
      target_url_status_(TARGET_NONE),
      is_loading_(false),
      navigation_gesture_(NavigationGestureUnknown),
      page_id_(-1),
      last_page_id_sent_to_browser_(-1),
      last_indexed_page_id_(-1),
      opened_by_user_gesture_(true),
      opener_suppressed_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      devtools_agent_(NULL),
      devtools_client_(NULL),
      file_chooser_completion_(NULL),
      history_back_list_count_(0),
      history_forward_list_count_(0),
      has_unload_listener_(false),
      decrement_shared_popup_at_destruction_(false),
      autofill_query_id_(0),
      popup_notification_visible_(false),
      spelling_panel_visible_(false),
      send_content_state_immediately_(false),
      send_preferred_size_changes_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          notification_provider_(new NotificationProvider(this))),
      determine_page_text_after_loading_stops_(false),
      view_type_(ViewType::INVALID),
      browser_window_id_(-1),
      last_top_level_navigation_page_id_(-1),
#if defined(OS_MACOSX)
      has_document_tag_(false),
#endif
      document_tag_(0),
      webkit_preferences_(webkit_preferences) {
}

RenderView::~RenderView() {
  if (decrement_shared_popup_at_destruction_)
    shared_popup_counter_->data--;

  // Dispose of un-disposed image fetchers.
  for (ImageResourceFetcherSet::iterator i = image_fetchers_.begin();
       i != image_fetchers_.end(); ++i) {
    delete *i;
  }

  // If file chooser is still waiting for answer, dispatch empty answer.
  if (file_chooser_completion_)
    file_chooser_completion_->didChooseFile(WebVector<WebString>());

#if defined(OS_MACOSX)
  // Tell the spellchecker that the document is closed.
  if (has_document_tag_)
    Send(new ViewHostMsg_DocumentWithTagClosed(routing_id_, document_tag_));
#endif

  render_thread_->RemoveFilter(audio_message_filter_);

#ifndef NDEBUG
  // Make sure we are no longer referenced by the ViewMap.
  ViewMap* views = Singleton<ViewMap>::get();
  for (ViewMap::iterator it = views->begin(); it != views->end(); ++it)
    DCHECK_NE(this, it->second) << "Failed to call Close?";
#endif
}

/*static*/
void RenderView::ForEach(RenderViewVisitor* visitor) {
  ViewMap* views = Singleton<ViewMap>::get();
  for (ViewMap::iterator it = views->begin(); it != views->end(); ++it) {
    if (!visitor->Visit(it->second))
      return;
  }
}

/*static*/
RenderView* RenderView::FromWebView(WebView* webview) {
  ViewMap* views = Singleton<ViewMap>::get();
  ViewMap::iterator it = views->find(webview);
  return it == views->end() ? NULL : it->second;
}

/*static*/
RenderView* RenderView::Create(
    RenderThreadBase* render_thread,
    gfx::NativeViewId parent_hwnd,
    int32 opener_id,
    const RendererPreferences& renderer_prefs,
    const WebPreferences& webkit_prefs,
    SharedRenderViewCounter* counter,
    int32 routing_id) {
  DCHECK(routing_id != MSG_ROUTING_NONE);
  scoped_refptr<RenderView> view = new RenderView(render_thread, webkit_prefs);
  view->Init(parent_hwnd,
             opener_id,
             renderer_prefs,
             counter,
             routing_id);  // adds reference
  return view;
}

/*static*/
void RenderView::SetNextPageID(int32 next_page_id) {
  // This method should only be called during process startup, and the given
  // page id had better not exceed our current next page id!
  DCHECK_EQ(next_page_id_, 1);
  DCHECK(next_page_id >= next_page_id_);
  next_page_id_ = next_page_id;
}

void RenderView::UserMetricsRecordAction(const std::wstring& action) {
  Send(new ViewHostMsg_UserMetricsRecordAction(routing_id_, action));
}

void RenderView::PluginCrashed(const FilePath& plugin_path) {
  Send(new ViewHostMsg_CrashedPlugin(routing_id_, plugin_path));
}

void RenderView::Init(gfx::NativeViewId parent_hwnd,
                      int32 opener_id,
                      const RendererPreferences& renderer_prefs,
                      SharedRenderViewCounter* counter,
                      int32 routing_id) {
  DCHECK(!webview());

  if (opener_id != MSG_ROUTING_NONE)
    opener_id_ = opener_id;

  if (counter) {
    shared_popup_counter_ = counter;
    shared_popup_counter_->data++;
    decrement_shared_popup_at_destruction_ = true;
  } else {
    shared_popup_counter_ = new SharedRenderViewCounter(0);
    decrement_shared_popup_at_destruction_ = false;
  }

  devtools_agent_.reset(new DevToolsAgent(routing_id, this));

  webwidget_ = WebView::create(this);
  Singleton<ViewMap>::get()->insert(std::make_pair(webview(), this));
  webkit_preferences_.Apply(webview());
  webview()->initializeMainFrame(this);
  webview()->setDevToolsAgent(
      WebDevToolsAgent::create(webview(), devtools_agent_.get()));

  OnSetRendererPrefs(renderer_prefs);

  routing_id_ = routing_id;
  render_thread_->AddRoute(routing_id_, this);
  // Take a reference on behalf of the RenderThread.  This will be balanced
  // when we receive ViewMsg_Close.
  AddRef();

  // If this is a popup, we must wait for the CreatingNew_ACK message before
  // completing initialization.  Otherwise, we can finish it now.
  if (opener_id == MSG_ROUTING_NONE) {
    did_show_ = true;
    CompleteInit(parent_hwnd);
  }

  host_window_ = parent_hwnd;

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kDomAutomationController))
    enabled_bindings_ |= BindingsPolicy::DOM_AUTOMATION;

  audio_message_filter_ = new AudioMessageFilter(routing_id_);
  render_thread_->AddFilter(audio_message_filter_);
}

void RenderView::OnMessageReceived(const IPC::Message& message) {
  WebFrame* main_frame = webview() ? webview()->mainFrame() : NULL;
  if (main_frame)
    child_process_logging::SetActiveURL(main_frame->url());

  // If this is developer tools renderer intercept tools messages first.
  if (devtools_client_.get() && devtools_client_->OnMessageReceived(message))
    return;
  if (devtools_agent_.get() && devtools_agent_->OnMessageReceived(message))
    return;
  if (notification_provider_->OnMessageReceived(message))
    return;

  IPC_BEGIN_MESSAGE_MAP(RenderView, message)
    IPC_MESSAGE_HANDLER(ViewMsg_CaptureThumbnail, SendThumbnail)
    IPC_MESSAGE_HANDLER(ViewMsg_PrintPages, OnPrintPages)
    IPC_MESSAGE_HANDLER(ViewMsg_PrintingDone, OnPrintingDone)
    IPC_MESSAGE_HANDLER(ViewMsg_Navigate, OnNavigate)
    IPC_MESSAGE_HANDLER(ViewMsg_Stop, OnStop)
    IPC_MESSAGE_HANDLER(ViewMsg_LoadAlternateHTMLText, OnLoadAlternateHTMLText)
    IPC_MESSAGE_HANDLER(ViewMsg_StopFinding, OnStopFinding)
    IPC_MESSAGE_HANDLER(ViewMsg_Undo, OnUndo)
    IPC_MESSAGE_HANDLER(ViewMsg_Redo, OnRedo)
    IPC_MESSAGE_HANDLER(ViewMsg_Cut, OnCut)
    IPC_MESSAGE_HANDLER(ViewMsg_Copy, OnCopy)
#if defined(OS_MACOSX)
    IPC_MESSAGE_HANDLER(ViewMsg_CopyToFindPboard, OnCopyToFindPboard)
#endif
    IPC_MESSAGE_HANDLER(ViewMsg_Paste, OnPaste)
    IPC_MESSAGE_HANDLER(ViewMsg_Replace, OnReplace)
    IPC_MESSAGE_HANDLER(ViewMsg_ToggleSpellPanel, OnToggleSpellPanel)
    IPC_MESSAGE_HANDLER(ViewMsg_AdvanceToNextMisspelling,
                        OnAdvanceToNextMisspelling)
    IPC_MESSAGE_HANDLER(ViewMsg_ToggleSpellCheck, OnToggleSpellCheck)
    IPC_MESSAGE_HANDLER(ViewMsg_Delete, OnDelete)
    IPC_MESSAGE_HANDLER(ViewMsg_SelectAll, OnSelectAll)
    IPC_MESSAGE_HANDLER(ViewMsg_CopyImageAt, OnCopyImageAt)
    IPC_MESSAGE_HANDLER(ViewMsg_ExecuteEditCommand, OnExecuteEditCommand)
    IPC_MESSAGE_HANDLER(ViewMsg_Find, OnFind)
    IPC_MESSAGE_HANDLER(ViewMsg_DeterminePageText, OnDeterminePageText)
    IPC_MESSAGE_HANDLER(ViewMsg_Zoom, OnZoom)
    IPC_MESSAGE_HANDLER(ViewMsg_SetPageEncoding, OnSetPageEncoding)
    IPC_MESSAGE_HANDLER(ViewMsg_ResetPageEncodingToDefault,
                        OnResetPageEncodingToDefault)
    IPC_MESSAGE_HANDLER(ViewMsg_SetupDevToolsClient, OnSetupDevToolsClient)
    IPC_MESSAGE_HANDLER(ViewMsg_DownloadFavIcon, OnDownloadFavIcon)
    IPC_MESSAGE_HANDLER(ViewMsg_ScriptEvalRequest, OnScriptEvalRequest)
    IPC_MESSAGE_HANDLER(ViewMsg_CSSInsertRequest, OnCSSInsertRequest)
    IPC_MESSAGE_HANDLER(ViewMsg_AddMessageToConsole, OnAddMessageToConsole)
    IPC_MESSAGE_HANDLER(ViewMsg_ReservePageIDRange, OnReservePageIDRange)
    IPC_MESSAGE_HANDLER(ViewMsg_FormFill, OnFormFill)
    IPC_MESSAGE_HANDLER(ViewMsg_FillPasswordForm, OnFillPasswordForm)
    IPC_MESSAGE_HANDLER(ViewMsg_DragTargetDragEnter, OnDragTargetDragEnter)
    IPC_MESSAGE_HANDLER(ViewMsg_DragTargetDragOver, OnDragTargetDragOver)
    IPC_MESSAGE_HANDLER(ViewMsg_DragTargetDragLeave, OnDragTargetDragLeave)
    IPC_MESSAGE_HANDLER(ViewMsg_DragTargetDrop, OnDragTargetDrop)
    IPC_MESSAGE_HANDLER(ViewMsg_AllowBindings, OnAllowBindings)
    IPC_MESSAGE_HANDLER(ViewMsg_SetDOMUIProperty, OnSetDOMUIProperty)
    IPC_MESSAGE_HANDLER(ViewMsg_DragSourceEndedOrMoved,
                        OnDragSourceEndedOrMoved)
    IPC_MESSAGE_HANDLER(ViewMsg_DragSourceSystemDragEnded,
                        OnDragSourceSystemDragEnded)
    IPC_MESSAGE_HANDLER(ViewMsg_SetInitialFocus, OnSetInitialFocus)
    IPC_MESSAGE_HANDLER(ViewMsg_FindReplyACK, OnFindReplyAck)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateTargetURL_ACK, OnUpdateTargetURLAck)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateWebPreferences, OnUpdateWebPreferences)
    IPC_MESSAGE_HANDLER(ViewMsg_SetAltErrorPageURL, OnSetAltErrorPageURL)
    IPC_MESSAGE_HANDLER(ViewMsg_InstallMissingPlugin, OnInstallMissingPlugin)
    IPC_MESSAGE_HANDLER(ViewMsg_RunFileChooserResponse, OnFileChooserResponse)
    IPC_MESSAGE_HANDLER(ViewMsg_EnableViewSourceMode, OnEnableViewSourceMode)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateBackForwardListCount,
                        OnUpdateBackForwardListCount)
    IPC_MESSAGE_HANDLER(ViewMsg_GetAllSavableResourceLinksForCurrentPage,
                        OnGetAllSavableResourceLinksForCurrentPage)
    IPC_MESSAGE_HANDLER(
        ViewMsg_GetSerializedHtmlDataForCurrentPageWithLocalLinks,
        OnGetSerializedHtmlDataForCurrentPageWithLocalLinks)
    IPC_MESSAGE_HANDLER(ViewMsg_GetApplicationInfo, OnGetApplicationInfo)
    IPC_MESSAGE_HANDLER(ViewMsg_GetAccessibilityInfo, OnGetAccessibilityInfo)
    IPC_MESSAGE_HANDLER(ViewMsg_ClearAccessibilityInfo,
                        OnClearAccessibilityInfo)
    IPC_MESSAGE_HANDLER(ViewMsg_ShouldClose, OnMsgShouldClose)
    IPC_MESSAGE_HANDLER(ViewMsg_ClosePage, OnClosePage)
    IPC_MESSAGE_HANDLER(ViewMsg_ThemeChanged, OnThemeChanged)
    IPC_MESSAGE_HANDLER(ViewMsg_HandleMessageFromExternalHost,
                        OnMessageFromExternalHost)
    IPC_MESSAGE_HANDLER(ViewMsg_DisassociateFromPopupCount,
                        OnDisassociateFromPopupCount)
    IPC_MESSAGE_HANDLER(ViewMsg_QueryFormFieldAutofill_ACK,
                        OnQueryFormFieldAutofillAck)
    IPC_MESSAGE_HANDLER(ViewMsg_PopupNotificationVisibilityChanged,
                        OnPopupNotificationVisibilityChanged)
    IPC_MESSAGE_HANDLER(ViewMsg_MoveOrResizeStarted, OnMoveOrResizeStarted)
    IPC_MESSAGE_HANDLER(ViewMsg_ExtensionResponse, OnExtensionResponse)
    IPC_MESSAGE_HANDLER(ViewMsg_ExtensionMessageInvoke,
                        OnExtensionMessageInvoke)
    IPC_MESSAGE_HANDLER(ViewMsg_ClearFocusedNode, OnClearFocusedNode)
    IPC_MESSAGE_HANDLER(ViewMsg_SetBackground, OnSetBackground)
    IPC_MESSAGE_HANDLER(ViewMsg_EnablePreferredSizeChangedMode,
                        OnEnablePreferredSizeChangedMode)
    IPC_MESSAGE_HANDLER(ViewMsg_SetRendererPrefs, OnSetRendererPrefs)
    IPC_MESSAGE_HANDLER(ViewMsg_UpdateBrowserWindowId,
                        OnUpdateBrowserWindowId)
    IPC_MESSAGE_HANDLER(ViewMsg_NotifyRenderViewType,
                        OnNotifyRendererViewType)
    IPC_MESSAGE_HANDLER(ViewMsg_MediaPlayerActionAt, OnMediaPlayerActionAt)
    IPC_MESSAGE_HANDLER(ViewMsg_SetActive, OnSetActive)
    IPC_MESSAGE_HANDLER(ViewMsg_SetEditCommandsForNextKeyEvent,
                        OnSetEditCommandsForNextKeyEvent);
    IPC_MESSAGE_HANDLER(ViewMsg_ExecuteCode,
                        OnExecuteCode)

    // Have the super handle all other messages.
    IPC_MESSAGE_UNHANDLED(RenderWidget::OnMessageReceived(message))
  IPC_END_MESSAGE_MAP()
}

void RenderView::SendThumbnail() {
  WebFrame* main_frame = webview()->mainFrame();
  if (!main_frame)
    return;

  // get the URL for this page
  GURL url(main_frame->url());
  if (url.is_empty())
    return;

  if (size_.IsEmpty())
    return;  // Don't create an empty thumbnail!

  ThumbnailScore score;
  SkBitmap thumbnail;
  if (!CaptureThumbnail(webview(), kThumbnailWidth, kThumbnailHeight,
                        &thumbnail, &score))
    return;

  // send the thumbnail message to the browser process
  Send(new ViewHostMsg_Thumbnail(routing_id_, url, score, thumbnail));
}

void RenderView::OnPrintPages() {
  DCHECK(webview());
  if (webview()) {
    // If the user has selected text in the currently focused frame we print
    // only that frame (this makes print selection work for multiple frames).
    if (webview()->focusedFrame()->hasSelection())
      Print(webview()->focusedFrame(), false);
    else
      Print(webview()->mainFrame(), false);
  }
}

void RenderView::OnPrintingDone(int document_cookie, bool success) {
  // Ignoring document cookie here since only one print job can be outstanding
  // per renderer and document_cookie is 0 when printing is successful.
  DCHECK(print_helper_.get());
  if (print_helper_.get() != NULL) {
    print_helper_->DidFinishPrinting(success);
  }
}

void RenderView::CapturePageInfo(int load_id, bool preliminary_capture) {
  if (load_id != page_id_)
    return;  // this capture call is no longer relevant due to navigation
  if (load_id == last_indexed_page_id_)
    return;  // we already indexed this page

  if (!webview())
    return;

  WebFrame* main_frame = webview()->mainFrame();
  if (!main_frame)
    return;

  // Don't index/capture pages that are in view source mode.
  if (main_frame->isViewSourceModeEnabled())
    return;

  // Don't index/capture pages that failed to load.  This only checks the top
  // level frame so the thumbnail may contain a frame that failed to load.
  WebDataSource* ds = main_frame->dataSource();
  if (ds && ds->hasUnreachableURL())
    return;

  if (!preliminary_capture)
    last_indexed_page_id_ = load_id;

  // get the URL for this page
  GURL url(main_frame->url());
  if (url.is_empty())
    return;

  // full text
  std::wstring contents;
  CaptureText(main_frame, &contents);
  if (contents.size()) {
    // Send the text to the browser for indexing.
    Send(new ViewHostMsg_PageContents(url, load_id, contents));
  }

  // Send over text content of this page to the browser.
  if (determine_page_text_after_loading_stops_) {
    determine_page_text_after_loading_stops_ = false;
    Send(new ViewMsg_DeterminePageText_Reply(routing_id_, contents));
  }

  // thumbnail
  SendThumbnail();
}

void RenderView::CaptureText(WebFrame* frame, std::wstring* contents) {
  contents->clear();
  if (!frame)
    return;

  // Don't index any https pages. People generally don't want their bank
  // accounts, etc. indexed on their computer, especially since some of these
  // things are not marked cachable.
  // TODO(brettw) we may want to consider more elaborate heuristics such as
  // the cachability of the page. We may also want to consider subframes (this
  // test will still index subframes if the subframe is SSL).
  if (GURL(frame->url()).SchemeIsSecure())
    return;

#ifdef TIME_TEXT_RETRIEVAL
  double begin = time_util::GetHighResolutionTimeNow();
#endif

  // get the contents of the frame
  *contents = UTF16ToWideHack(frame->contentAsText(kMaxIndexChars));

#ifdef TIME_TEXT_RETRIEVAL
  double end = time_util::GetHighResolutionTimeNow();
  char buf[128];
  sprintf_s(buf, "%d chars retrieved for indexing in %gms\n",
            contents.size(), (end - begin)*1000);
  OutputDebugStringA(buf);
#endif

  // When the contents are clipped to the maximum, we don't want to have a
  // partial word indexed at the end that might have been clipped. Therefore,
  // terminate the string at the last space to ensure no words are clipped.
  if (contents->size() == kMaxIndexChars) {
    size_t last_space_index = contents->find_last_of(kWhitespaceWide);
    if (last_space_index == std::wstring::npos)
      return;  // don't index if we got a huge block of text with no spaces
    contents->resize(last_space_index);
  }
}

bool RenderView::CaptureThumbnail(WebView* view,
                                  int w,
                                  int h,
                                  SkBitmap* thumbnail,
                                  ThumbnailScore* score) {
#ifdef TIME_BITMAP_RETRIEVAL
  double begin = time_util::GetHighResolutionTimeNow();
#endif

  view->layout();
  const WebSize& size = view->size();

  skia::PlatformCanvas canvas;
  if (!canvas.initialize(size.width, size.height, true))
    return false;
  view->paint(webkit_glue::ToWebCanvas(&canvas),
              WebRect(0, 0, size.width, size.height));

  skia::BitmapPlatformDevice& device =
      static_cast<skia::BitmapPlatformDevice&>(canvas.getTopPlatformDevice());

  const SkBitmap& src_bmp = device.accessBitmap(false);

  SkRect dest_rect = { 0, 0, SkIntToScalar(w), SkIntToScalar(h) };
  float dest_aspect = dest_rect.width() / dest_rect.height();

  // Get the src rect so that we can preserve the aspect ratio while filling
  // the destination.
  SkIRect src_rect;
  if (src_bmp.width() < dest_rect.width() ||
      src_bmp.height() < dest_rect.height()) {
    // Source image is smaller: we clip the part of source image within the
    // dest rect, and then stretch it to fill the dest rect. We don't respect
    // the aspect ratio in this case.
    src_rect.set(0, 0, static_cast<S16CPU>(dest_rect.width()),
                 static_cast<S16CPU>(dest_rect.height()));
    score->good_clipping = false;
  } else {
    float src_aspect = static_cast<float>(src_bmp.width()) / src_bmp.height();
    if (src_aspect > dest_aspect) {
      // Wider than tall, clip horizontally: we center the smaller thumbnail in
      // the wider screen.
      S16CPU new_width = static_cast<S16CPU>(src_bmp.height() * dest_aspect);
      S16CPU x_offset = (src_bmp.width() - new_width) / 2;
      src_rect.set(x_offset, 0, new_width + x_offset, src_bmp.height());
      score->good_clipping = false;
    } else {
      src_rect.set(0, 0, src_bmp.width(),
                   static_cast<S16CPU>(src_bmp.width() / dest_aspect));
      score->good_clipping = true;
    }
  }

  score->at_top = (view->mainFrame()->scrollOffset().height == 0);

  SkBitmap subset;
  device.accessBitmap(false).extractSubset(&subset, src_rect);

  // Resample the subset that we want to get it the right size.
  *thumbnail = skia::ImageOperations::Resize(
      subset, skia::ImageOperations::RESIZE_LANCZOS3, w, h);

  score->boring_score = CalculateBoringScore(thumbnail);

#ifdef TIME_BITMAP_RETRIEVAL
  double end = time_util::GetHighResolutionTimeNow();
  char buf[128];
  sprintf_s(buf, "thumbnail in %gms\n", (end - begin) * 1000);
  OutputDebugStringA(buf);
#endif
  return true;
}

double RenderView::CalculateBoringScore(SkBitmap* bitmap) {
  int histogram[256] = {0};
  color_utils::BuildLumaHistogram(bitmap, histogram);

  int color_count = *std::max_element(histogram, histogram + 256);
  int pixel_count = bitmap->width() * bitmap->height();
  return static_cast<double>(color_count) / pixel_count;
}

void RenderView::OnNavigate(const ViewMsg_Navigate_Params& params) {
  if (!webview())
    return;

  if (devtools_agent_.get())
    devtools_agent_->OnNavigate();

  child_process_logging::SetActiveURL(params.url);

  AboutHandler::MaybeHandle(params.url);

  bool is_reload = (params.navigation_type == ViewMsg_Navigate_Params::RELOAD);

  WebFrame* main_frame = webview()->mainFrame();
  if (is_reload && main_frame->currentHistoryItem().isNull()) {
    // We cannot reload if we do not have any history state.  This happens, for
    // example, when recovering from a crash.  Our workaround here is a bit of
    // a hack since it means that reload after a crashed tab does not cause an
    // end-to-end cache validation.
    is_reload = false;
  }

  // A navigation resulting from loading a javascript URL should not be treated
  // as a browser initiated event.  Instead, we want it to look as if the page
  // initiated any load resulting from JS execution.
  if (!params.url.SchemeIs(chrome::kJavaScriptScheme)) {
    NavigationState* state = NavigationState::CreateBrowserInitiated(
        params.page_id, params.transition, params.request_time);
    if (params.navigation_type == ViewMsg_Navigate_Params::RESTORE) {
      // We're doing a load of a page that was restored from the last session.
      // By default this prefers the cache over loading (LOAD_PREFERRING_CACHE)
      // which can result in stale data for pages that are set to expire. We
      // explicitly override that by setting the policy here so that as
      // necessary we load from the network.
      state->set_cache_policy_override(WebURLRequest::UseProtocolCachePolicy);
    }
    pending_navigation_state_.reset(state);
  }

  // If we are reloading, then WebKit will use the history state of the current
  // page, so we should just ignore any given history state.  Otherwise, if we
  // have history state, then we need to navigate to it, which corresponds to a
  // back/forward navigation event.
  if (is_reload) {
    main_frame->reload();
  } else if (!params.state.empty()) {
    // We must know the page ID of the page we are navigating back to.
    DCHECK_NE(params.page_id, -1);
    main_frame->loadHistoryItem(
        webkit_glue::HistoryItemFromString(params.state));
  } else {
    // Navigate to the given URL.
    WebURLRequest request(params.url);

    // A session history navigation should have been accompanied by state.
    DCHECK_EQ(params.page_id, -1);

    if (main_frame->isViewSourceModeEnabled())
      request.setCachePolicy(WebURLRequest::ReturnCacheDataElseLoad);

    if (params.referrer.is_valid()) {
      request.setHTTPHeaderField(WebString::fromUTF8("Referer"),
                                 WebString::fromUTF8(params.referrer.spec()));
    }

    main_frame->loadRequest(request);
  }

  // In case LoadRequest failed before DidCreateDataSource was called.
  pending_navigation_state_.reset();
}

// Stop loading the current page
void RenderView::OnStop() {
  if (webview())
    webview()->mainFrame()->stopLoading();
}

void RenderView::OnLoadAlternateHTMLText(const std::string& html,
                                         bool new_navigation,
                                         const GURL& display_url,
                                         const std::string& security_info) {
  if (!webview())
    return;

  pending_navigation_state_.reset(NavigationState::CreateBrowserInitiated(
      new_navigation ? -1 : page_id_, PageTransition::LINK, Time::Now()));
  pending_navigation_state_->set_security_info(security_info);

  webview()->mainFrame()->loadHTMLString(
      html, GURL(kUnreachableWebDataURL), display_url, !new_navigation);

  pending_navigation_state_.reset();
}

void RenderView::OnCopyImageAt(int x, int y) {
  webview()->copyImageAt(WebPoint(x, y));
}

void RenderView::OnExecuteEditCommand(const std::string& name,
    const std::string& value) {
  if (!webview() || !webview()->focusedFrame())
    return;

  webview()->focusedFrame()->executeCommand(
      WebString::fromUTF8(name), WebString::fromUTF8(value));
}

void RenderView::OnSetupDevToolsClient() {
  DCHECK(!devtools_client_.get());
  devtools_client_.reset(new DevToolsClient(this));
}

void RenderView::OnStopFinding(bool clear_selection) {
  WebView* view = webview();
  if (!view)
    return;

  if (clear_selection)
    view->focusedFrame()->executeCommand(WebString::fromUTF8("Unselect"));

  WebFrame* frame = view->mainFrame();
  while (frame) {
    frame->stopFinding(clear_selection);
    frame = frame->traverseNext(false);
  }
}

void RenderView::OnFindReplyAck() {
  // Check if there is any queued up request waiting to be sent.
  if (queued_find_reply_message_.get()) {
    // Send the search result over to the browser process.
    Send(queued_find_reply_message_.get());
    queued_find_reply_message_.release();
  }
}

void RenderView::OnUpdateTargetURLAck() {
  // Check if there is a targeturl waiting to be sent.
  if (target_url_status_ == TARGET_PENDING) {
    Send(new ViewHostMsg_UpdateTargetURL(routing_id_, page_id_,
                                         pending_target_url_));
  }

  target_url_status_ = TARGET_NONE;
}

void RenderView::OnUndo() {
  if (!webview())
    return;

  webview()->focusedFrame()->executeCommand(WebString::fromUTF8("Undo"));
  UserMetricsRecordAction(L"Undo");
}

void RenderView::OnRedo() {
  if (!webview())
    return;

  webview()->focusedFrame()->executeCommand(WebString::fromUTF8("Redo"));
  UserMetricsRecordAction(L"Redo");
}

void RenderView::OnCut() {
  if (!webview())
    return;

  webview()->focusedFrame()->executeCommand(WebString::fromUTF8("Cut"));
  UserMetricsRecordAction(L"Cut");
}

void RenderView::OnCopy() {
  if (!webview())
    return;

  webview()->focusedFrame()->executeCommand(WebString::fromUTF8("Copy"));
  UserMetricsRecordAction(L"Copy");
}

#if defined(OS_MACOSX)
void RenderView::OnCopyToFindPboard() {
  if (!webview())
    return;

  // Since the find pasteboard supports only plain text, this can be simpler
  // than the |OnCopy()| case.
  WebFrame* frame = webview()->focusedFrame();
  if (frame->hasSelection()) {
    string16 selection = frame->selectionAsText();
    RenderThread::current()->Send(
        new ViewHostMsg_ClipboardFindPboardWriteStringAsync(selection));
  }

  UserMetricsRecordAction(L"CopyToFindPboard");
}
#endif

void RenderView::OnPaste() {
  if (!webview())
    return;

  webview()->focusedFrame()->executeCommand(WebString::fromUTF8("Paste"));
  UserMetricsRecordAction(L"Paste");
}

void RenderView::OnReplace(const string16& text) {
  if (!webview())
    return;

  webview()->focusedFrame()->replaceSelection(text);
}

void RenderView::OnAdvanceToNextMisspelling() {
  if (!webview())
    return;
  webview()->focusedFrame()->executeCommand(
      WebString::fromUTF8("AdvanceToNextMisspelling"));
}

void RenderView::OnToggleSpellPanel(bool is_currently_visible) {
  if (!webview())
    return;
  // We need to tell the webView whether the spelling panel is visible or not so
  // that it won't need to make ipc calls later.
  spelling_panel_visible_ = is_currently_visible;
  webview()->focusedFrame()->executeCommand(
      WebString::fromUTF8("ToggleSpellPanel"));
}

void RenderView::OnToggleSpellCheck() {
  if (!webview())
    return;

  WebFrame* frame = webview()->focusedFrame();
  frame->enableContinuousSpellChecking(
      !frame->isContinuousSpellCheckingEnabled());
}

void RenderView::OnDelete() {
  if (!webview())
    return;

  webview()->focusedFrame()->executeCommand(WebString::fromUTF8("Delete"));
  UserMetricsRecordAction(L"DeleteSelection");
}

void RenderView::OnSelectAll() {
  if (!webview())
    return;

  webview()->focusedFrame()->executeCommand(
      WebString::fromUTF8("SelectAll"));
  UserMetricsRecordAction(L"SelectAll");
}

void RenderView::OnSetInitialFocus(bool reverse) {
  if (!webview())
    return;
  webview()->setInitialFocus(reverse);
}

///////////////////////////////////////////////////////////////////////////////

// Tell the embedding application that the URL of the active page has changed
void RenderView::UpdateURL(WebFrame* frame) {
  WebDataSource* ds = frame->dataSource();
  DCHECK(ds);

  const WebURLRequest& request = ds->request();
  const WebURLRequest& original_request = ds->originalRequest();
  const WebURLResponse& response = ds->response();

  NavigationState* navigation_state = NavigationState::FromDataSource(ds);
  DCHECK(navigation_state);

  ViewHostMsg_FrameNavigate_Params params;
  params.http_status_code = response.httpStatusCode();
  params.is_post = false;
  params.page_id = page_id_;
  params.is_content_filtered = response.isContentFiltered();
  if (!navigation_state->security_info().empty()) {
    // SSL state specified in the request takes precedence over the one in the
    // response.
    // So far this is only intended for error pages that are not expected to be
    // over ssl, so we should not get any clash.
    DCHECK(response.securityInfo().isEmpty());
    params.security_info = navigation_state->security_info();
  } else {
    params.security_info = response.securityInfo();
  }

  // Set the URL to be displayed in the browser UI to the user.
  if (ds->hasUnreachableURL()) {
    params.url = ds->unreachableURL();
  } else {
    params.url = request.url();
  }

  GetRedirectChain(ds, &params.redirects);
  params.should_update_history = !ds->hasUnreachableURL();

  params.searchable_form_url = navigation_state->searchable_form_url();
  params.searchable_form_encoding =
      navigation_state->searchable_form_encoding();

  const PasswordForm* password_form_data =
      navigation_state->password_form_data();
  if (password_form_data)
    params.password_form = *password_form_data;

  params.gesture = navigation_gesture_;
  navigation_gesture_ = NavigationGestureUnknown;

  if (!frame->parent()) {
    // Top-level navigation.

    // Update contents MIME type for main frame.
    params.contents_mime_type = ds->response().mimeType().utf8();

    params.transition = navigation_state->transition_type();
    if (!PageTransition::IsMainFrame(params.transition)) {
      // If the main frame does a load, it should not be reported as a subframe
      // navigation.  This can occur in the following case:
      // 1. You're on a site with frames.
      // 2. You do a subframe navigation.  This is stored with transition type
      //    MANUAL_SUBFRAME.
      // 3. You navigate to some non-frame site, say, google.com.
      // 4. You navigate back to the page from step 2.  Since it was initially
      //    MANUAL_SUBFRAME, it will be that same transition type here.
      // We don't want that, because any navigation that changes the toplevel
      // frame should be tracked as a toplevel navigation (this allows us to
      // update the URL bar, etc).
      params.transition = PageTransition::LINK;
    }

    // If we have a valid consumed client redirect source,
    // the page contained a client redirect (meta refresh, document.loc...),
    // so we set the referrer and transition to match.
    if (completed_client_redirect_src_.is_valid()) {
      DCHECK(completed_client_redirect_src_ == params.redirects[0]);
      params.referrer = completed_client_redirect_src_;
      params.transition = static_cast<PageTransition::Type>(
          params.transition | PageTransition::CLIENT_REDIRECT);
    } else {
      // Bug 654101: the referrer will be empty on https->http transitions. It
      // would be nice if we could get the real referrer from somewhere.
      params.referrer = GURL(
          original_request.httpHeaderField(WebString::fromUTF8("Referer")));
    }

    string16 method = request.httpMethod();
    if (EqualsASCII(method, "POST"))
      params.is_post = true;

    // Save some histogram data so we can compute the average memory used per
    // page load of the glyphs.
    UMA_HISTOGRAM_COUNTS_10000("Memory.GlyphPagesPerLoad",
                               webkit_glue::GetGlyphPageCount());

    Send(new ViewHostMsg_FrameNavigate(routing_id_, params));
  } else {
    // Subframe navigation: the type depends on whether this navigation
    // generated a new session history entry. When they do generate a session
    // history entry, it means the user initiated the navigation and we should
    // mark it as such. This test checks if this is the first time UpdateURL
    // has been called since WillNavigateToURL was called to initiate the load.
    if (page_id_ > last_page_id_sent_to_browser_)
      params.transition = PageTransition::MANUAL_SUBFRAME;
    else
      params.transition = PageTransition::AUTO_SUBFRAME;

    Send(new ViewHostMsg_FrameNavigate(routing_id_, params));
  }

  last_page_id_sent_to_browser_ =
      std::max(last_page_id_sent_to_browser_, page_id_);

  // If we end up reusing this WebRequest (for example, due to a #ref click),
  // we don't want the transition type to persist.  Just clear it.
  navigation_state->set_transition_type(PageTransition::LINK);

#if defined(OS_WIN)
  if (accessibility_.get()) {
    // Clear accessibility info cache.
    accessibility_->clear();
  }
#else
  // TODO(port): accessibility not yet implemented. See http://crbug.com/8288.
#endif
}

// Tell the embedding application that the title of the active page has changed
void RenderView::UpdateTitle(WebFrame* frame, const string16& title) {
  // Ignore all but top level navigations...
  if (!frame->parent()) {
    Send(new ViewHostMsg_UpdateTitle(
        routing_id_,
        page_id_,
        UTF16ToWideHack(title.length() > chrome::kMaxTitleChars ?
            title.substr(0, chrome::kMaxTitleChars) : title)));
  }
}

void RenderView::UpdateEncoding(WebFrame* frame,
                                const std::string& encoding_name) {
  // Only update main frame's encoding_name.
  if (webview()->mainFrame() == frame &&
      last_encoding_name_ != encoding_name) {
    // Save the encoding name for later comparing.
    last_encoding_name_ = encoding_name;

    Send(new ViewHostMsg_UpdateEncoding(routing_id_, last_encoding_name_));
  }
}

// Sends the previous session history state to the browser so it will be saved
// before we navigate to a new page. This must be called *before* the page ID
// has been updated so we know what it was.
void RenderView::UpdateSessionHistory(WebFrame* frame) {
  // If we have a valid page ID at this point, then it corresponds to the page
  // we are navigating away from.  Otherwise, this is the first navigation, so
  // there is no past session history to record.
  if (page_id_ == -1)
    return;

  const WebHistoryItem& item =
      webview()->mainFrame()->previousHistoryItem();
  if (item.isNull())
    return;

  Send(new ViewHostMsg_UpdateState(
      routing_id_, page_id_, webkit_glue::HistoryItemToString(item)));
}

void RenderView::OpenURL(
    const GURL& url, const GURL& referrer, WebNavigationPolicy policy) {
  Send(new ViewHostMsg_OpenURL(
      routing_id_, url, referrer, NavigationPolicyToDisposition(policy)));
}

// WebViewDelegate ------------------------------------------------------------

void RenderView::DidPaint() {
  WebFrame* main_frame = webview()->mainFrame();

  if (main_frame->provisionalDataSource()) {
    // If we have a provisional frame we are between the start
    // and commit stages of loading...ignore this paint.
    return;
  }

  WebDataSource* ds = main_frame->dataSource();
  NavigationState* navigation_state = NavigationState::FromDataSource(ds);
  DCHECK(navigation_state);

  Time now = Time::Now();
  if (navigation_state->first_paint_time().is_null()) {
    navigation_state->set_first_paint_time(now);
  }
  if (navigation_state->first_paint_after_load_time().is_null() &&
      !navigation_state->finish_load_time().is_null()) {
    navigation_state->set_first_paint_after_load_time(now);
  }
}

void RenderView::LoadNavigationErrorPage(WebFrame* frame,
                                         const WebURLRequest& failed_request,
                                         const WebURLError& error,
                                         const std::string& html,
                                         bool replace) {
  GURL failed_url = error.unreachableURL;

  std::string alt_html;
  if (html.empty()) {
    // Use a local error page.
    int resource_id;
    DictionaryValue error_strings;
    if (error.reason == net::ERR_CACHE_MISS &&
        EqualsASCII(failed_request.httpMethod(), "POST")) {
      GetFormRepostErrorValues(failed_url, &error_strings);
      resource_id = IDR_ERROR_NO_DETAILS_HTML;
    } else {
      GetLocalizedErrorValues(error, &error_strings);
      resource_id = IDR_NET_ERROR_HTML;
    }
    error_strings.SetString(L"textdirection",
      (l10n_util::GetTextDirection() == l10n_util::RIGHT_TO_LEFT) ?
       L"rtl" : L"ltr");

    alt_html = GetAltHTMLForTemplate(error_strings, resource_id);
  } else {
    alt_html = html;
  }

  frame->loadHTMLString(alt_html,
                        GURL(kUnreachableWebDataURL),
                        failed_url,
                        replace);
}

void RenderView::BindDOMAutomationController(WebFrame* frame) {
  dom_automation_controller_.set_message_sender(this);
  dom_automation_controller_.set_routing_id(routing_id_);
  dom_automation_controller_.BindToJavascript(frame,
                                              L"domAutomationController");
}

bool RenderView::RunJavaScriptMessage(int type,
                                      const std::wstring& message,
                                      const std::wstring& default_value,
                                      const GURL& frame_url,
                                      std::wstring* result) {
  bool success = false;
  std::wstring result_temp;
  if (!result)
    result = &result_temp;

  SendAndRunNestedMessageLoop(new ViewHostMsg_RunJavaScriptMessage(
      routing_id_, message, default_value, frame_url, type, &success, result));
  return success;
}

void RenderView::AddGURLSearchProvider(const GURL& osd_url, bool autodetected) {
  if (!osd_url.is_empty())
    Send(new ViewHostMsg_PageHasOSDD(routing_id_, page_id_, osd_url,
                                     autodetected));
}

void RenderView::OnQueryFormFieldAutofillAck(
    int query_id,
    const std::vector<string16>& suggestions,
    int default_suggestion_index) {
  if (webview() && query_id == autofill_query_id_) {
    webview()->applyAutofillSuggestions(
        autofill_query_node_, suggestions, default_suggestion_index);
  }
  autofill_query_node_.reset();
}

void RenderView::OnPopupNotificationVisibilityChanged(bool visible) {
  popup_notification_visible_ = visible;
}

uint32 RenderView::GetCPBrowsingContext() {
  uint32 context = 0;
  Send(new ViewHostMsg_GetCPBrowsingContext(&context));
  return context;
}

void RenderView::AddSearchProvider(const std::string& url) {
  AddGURLSearchProvider(GURL(url),
                        false);  // not autodetected
}

void RenderView::OnMissingPluginStatus(
    WebPluginDelegateProxy* delegate,
    int status) {
#if defined(OS_WIN)
  if (!first_default_plugin_) {
    // Show the InfoBar for the first available plugin.
    if (status == default_plugin::MISSING_PLUGIN_AVAILABLE) {
      first_default_plugin_ = delegate->AsWeakPtr();
      Send(new ViewHostMsg_MissingPluginStatus(routing_id_, status));
    }
  } else {
    // Closes the InfoBar if user clicks on the plugin (instead of the InfoBar)
    // to start the download/install.
    if (status == default_plugin::MISSING_PLUGIN_USER_STARTED_DOWNLOAD) {
      Send(new ViewHostMsg_MissingPluginStatus(routing_id_, status));
    }
  }
#else
  // TODO(port): plugins current not supported
  NOTIMPLEMENTED();
#endif
}

// WebKit::WebViewClient ------------------------------------------------------

WebView* RenderView::createView(WebFrame* creator) {
  // Check to make sure we aren't overloading on popups.
  if (shared_popup_counter_->data > kMaximumNumberOfUnacknowledgedPopups)
    return NULL;

  // This window can't be closed from a window.close() call until we receive a
  // message from the Browser process explicitly allowing it.
  popup_notification_visible_ = true;

  int32 routing_id = MSG_ROUTING_NONE;
  bool user_gesture = creator->isProcessingUserGesture();
  bool opener_suppressed = creator->willSuppressOpenerInNewFrame();

  render_thread_->Send(
      new ViewHostMsg_CreateWindow(routing_id_, user_gesture, &routing_id));
  if (routing_id == MSG_ROUTING_NONE)
    return NULL;

  RenderView* view = RenderView::Create(render_thread_,
                                        0,
                                        routing_id_,
                                        renderer_preferences_,
                                        webkit_preferences_,
                                        shared_popup_counter_,
                                        routing_id);
  view->opened_by_user_gesture_ = user_gesture;

  // Record whether the creator frame is trying to suppress the opener field.
  view->opener_suppressed_ = opener_suppressed;

  // Record the security origin of the creator.
  GURL creator_url(creator->securityOrigin().toString().utf8());
  if (!creator_url.is_valid() || !creator_url.IsStandard())
    creator_url = GURL();
  view->creator_url_ = creator_url;

  // Copy over the alternate error page URL so we can have alt error pages in
  // the new render view (we don't need the browser to send the URL back down).
  view->alternate_error_page_url_ = alternate_error_page_url_;

  return view->webview();
}

WebWidget* RenderView::createPopupMenu(bool activatable) {
  RenderWidget* widget = RenderWidget::Create(routing_id_,
                                              render_thread_,
                                              activatable);
  return widget->webwidget();
}

WebWidget* RenderView::createPopupMenu(const WebPopupMenuInfo& info) {
  RenderWidget* widget = RenderWidget::Create(routing_id_,
                                              render_thread_,
                                              true);
  widget->ConfigureAsExternalPopupMenu(info);
  return widget->webwidget();
}

void RenderView::didAddMessageToConsole(
    const WebConsoleMessage& message, const WebString& source_name,
    unsigned source_line) {
  Send(new ViewHostMsg_AddMessageToConsole(routing_id_,
                                           UTF16ToWideHack(message.text),
                                           static_cast<int32>(source_line),
                                           UTF16ToWideHack(source_name)));
}

void RenderView::printPage(WebFrame* frame) {
  DCHECK(frame);
  Print(frame, true);
}

void RenderView::didStartLoading() {
  if (is_loading_) {
    DLOG(WARNING) << "didStartLoading called while loading";
    return;
  }

  is_loading_ = true;
  // Clear the pointer so that we can assign it only when there is an unknown
  // plugin on a page.
  first_default_plugin_.reset();

  Send(new ViewHostMsg_DidStartLoading(routing_id_));
}

void RenderView::didStopLoading() {
  if (!is_loading_) {
    DLOG(WARNING) << "DidStopLoading called while not loading";
    return;
  }

  is_loading_ = false;

  // NOTE: For now we're doing the safest thing, and sending out notification
  // when done loading. This currently isn't an issue as the favicon is only
  // displayed when done loading. Ideally we would send notification when
  // finished parsing the head, but webkit doesn't support that yet.
  // The feed discovery code would also benefit from access to the head.
  GURL favicon_url(webview()->mainFrame()->favIconURL());
  if (!favicon_url.is_empty())
    Send(new ViewHostMsg_UpdateFavIconURL(routing_id_, page_id_, favicon_url));

  AddGURLSearchProvider(webview()->mainFrame()->openSearchDescriptionURL(),
                        true);  // autodetected

  Send(new ViewHostMsg_DidStopLoading(routing_id_));

  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      method_factory_.NewRunnableMethod(&RenderView::CapturePageInfo, page_id_,
                                        false),
      kDelayForCaptureMs);
}

bool RenderView::isSelectTrailingWhitespaceEnabled() {
#if defined(OS_WIN)
  return true;
#else
  return false;
#endif
}

void RenderView::setInputMethodEnabled(bool enabled) {
  // Save the updated IME status and mark the input focus has been updated.
  // The IME status is to be sent to a browser process next time when
  // the input caret is rendered.
  if (!ime_control_busy_) {
    ime_control_updated_ = true;
    ime_control_new_state_ = enabled;
  }
}

void RenderView::didChangeSelection(bool is_empty_selection) {
#if defined(OS_LINUX)
  if (!handling_input_event_)
      return;
  // TODO(estade): investigate incremental updates to the selection so that we
  // don't send the entire selection over IPC every time.
  if (!is_empty_selection) {
    // Sometimes we get repeated didChangeSelection calls from webkit when
    // the selection hasn't actually changed. We don't want to report these
    // because it will cause us to continually claim the X clipboard.
    const std::string& this_selection =
        webview()->focusedFrame()->selectionAsText().utf8();
    if (this_selection == last_selection_)
      return;

    Send(new ViewHostMsg_SelectionChanged(routing_id_,
         this_selection));
    last_selection_ = this_selection;
  } else {
    last_selection_.clear();
  }
#endif
}

void RenderView::didExecuteCommand(const WebString& command_name) {
  const std::wstring& name = UTF16ToWideHack(command_name);
  if (StartsWith(name, L"Move", true) ||
      StartsWith(name, L"Insert", true) ||
      StartsWith(name, L"Delete", true))
    return;
  UserMetricsRecordAction(name);
}

bool RenderView::handleCurrentKeyboardEvent() {
  if (edit_commands_.empty())
    return false;

  WebFrame* frame = webview()->focusedFrame();
  if (!frame)
    return false;

  EditCommands::iterator it = edit_commands_.begin();
  EditCommands::iterator end = edit_commands_.end();

  bool did_execute_command = false;
  for (; it != end; ++it) {
    // In gtk and cocoa, it's possible to bind multiple edit commands to one
    // key (but it's the exception). Once one edit command is not executed, it
    // seems safest to not execute the rest.
    if (!frame->executeCommand(WebString::fromUTF8(it->name),
                               WebString::fromUTF8(it->value)))
      break;
    did_execute_command = true;
  }

  return did_execute_command;
}

void RenderView::spellCheck(const WebString& text,
                            int& misspelled_offset,
                            int& misspelled_length) {
  EnsureDocumentTag();
  Send(new ViewHostMsg_SpellCheck(routing_id_, text, document_tag_,
                                  &misspelled_offset, &misspelled_length));
}

WebString RenderView::autoCorrectWord(const WebKit::WebString& word) {
  string16 autocorrect_word;
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kExperimentalSpellcheckerFeatures)) {
    EnsureDocumentTag();
    Send(new ViewHostMsg_GetAutoCorrectWord(
        routing_id_, word, document_tag_, &autocorrect_word));
  }
  return autocorrect_word;
}

void RenderView::showSpellingUI(bool show) {
  Send(new ViewHostMsg_ShowSpellingPanel(routing_id_, show));
}

bool RenderView::isShowingSpellingUI() {
  return spelling_panel_visible_;
}

void RenderView::updateSpellingUIWithMisspelledWord(const WebString& word) {
  Send(new ViewHostMsg_UpdateSpellingPanelWithMisspelledWord(routing_id_,
                                                             word));
}

bool RenderView::runFileChooser(
    bool multi_select,
    const WebKit::WebString& title,
    const WebKit::WebString& initial_value,
    WebKit::WebFileChooserCompletion* chooser_completion) {
  if (file_chooser_completion_) {
    // TODO(brettw): bug 1235154: This should be a synchronous message to deal
    // with the fact that web pages can programatically trigger this. With the
    // asnychronous messages, we can get an additional call when one is pending,
    // which this test is for. For now, we just ignore the additional file
    // chooser request. WebKit doesn't do anything to expect the callback, so
    // we can just ignore calling it.
    return false;
  }
  file_chooser_completion_ = chooser_completion;
  Send(new ViewHostMsg_RunFileChooser(
    routing_id_, multi_select, title,
    webkit_glue::WebStringToFilePath(initial_value)));
  return true;
}

void RenderView::runModalAlertDialog(
    WebFrame* frame, const WebString& message) {
  RunJavaScriptMessage(MessageBoxFlags::kIsJavascriptAlert,
                       UTF16ToWideHack(message),
                       std::wstring(),
                       frame->url(),
                       NULL);
}

bool RenderView::runModalConfirmDialog(
    WebFrame* frame, const WebString& message) {
  return RunJavaScriptMessage(MessageBoxFlags::kIsJavascriptConfirm,
                              UTF16ToWideHack(message),
                              std::wstring(),
                              frame->url(),
                              NULL);
}

bool RenderView::runModalPromptDialog(
    WebFrame* frame, const WebString& message, const WebString& default_value,
    WebString* actual_value) {
  std::wstring result;
  bool ok = RunJavaScriptMessage(MessageBoxFlags::kIsJavascriptPrompt,
                                 UTF16ToWideHack(message),
                                 UTF16ToWideHack(default_value),
                                 frame->url(),
                                 &result);
  if (ok)
    actual_value->assign(WideToUTF16Hack(result));
  return ok;
}

bool RenderView::runModalBeforeUnloadDialog(
    WebFrame* frame, const WebString& message) {
  bool success = false;
  // This is an ignored return value, but is included so we can accept the same
  // response as RunJavaScriptMessage.
  std::wstring ignored_result;
  SendAndRunNestedMessageLoop(new ViewHostMsg_RunBeforeUnloadConfirm(
      routing_id_, frame->url(), UTF16ToWideHack(message), &success,
      &ignored_result));
  return success;
}

void RenderView::showContextMenu(
    WebFrame* frame, const WebContextMenuData& data) {
  Send(new ViewHostMsg_ContextMenu(routing_id_, ContextMenuParams(data)));
}

void RenderView::setStatusText(const WebString& text) {
}

void RenderView::UpdateTargetURL(const GURL& url, const GURL& fallback_url) {
  GURL latest_url = url.spec().empty() ? fallback_url : url;
  if (latest_url == target_url_)
    return;

  // Tell the browser to display a destination link.
  if (target_url_status_ == TARGET_INFLIGHT ||
      target_url_status_ == TARGET_PENDING) {
    // If we have a request in-flight, save the URL to be sent when we
    // receive an ACK to the in-flight request. We can happily overwrite
    // any existing pending sends.
    pending_target_url_ = latest_url;
    target_url_status_ = TARGET_PENDING;
  } else {
    Send(new ViewHostMsg_UpdateTargetURL(routing_id_, page_id_, latest_url));
    target_url_ = latest_url;
    target_url_status_ = TARGET_INFLIGHT;
  }
}

void RenderView::StartNavStateSyncTimerIfNecessary() {
  int delay;
  if (send_content_state_immediately_)
    delay = 0;
  else if (is_hidden())
    delay = kDelaySecondsForContentStateSyncHidden;
  else
    delay = kDelaySecondsForContentStateSync;

  if (nav_state_sync_timer_.IsRunning()) {
    // The timer is already running. If the delay of the timer maches the amount
    // we want to delay by, then return. Otherwise stop the timer so that it
    // gets started with the right delay.
    if (nav_state_sync_timer_.GetCurrentDelay().InSeconds() == delay)
      return;
    nav_state_sync_timer_.Stop();
  }

  nav_state_sync_timer_.Start(
      TimeDelta::FromSeconds(delay), this, &RenderView::SyncNavigationState);
}

void RenderView::setMouseOverURL(const WebURL& url) {
  mouse_over_url_ = GURL(url);
  UpdateTargetURL(mouse_over_url_, focus_url_);
}

void RenderView::setKeyboardFocusURL(const WebURL& url) {
  focus_url_ = GURL(url);
  UpdateTargetURL(focus_url_, mouse_over_url_);
}

void RenderView::setToolTipText(const WebString& text, WebTextDirection hint) {
  Send(new ViewHostMsg_SetTooltipText(routing_id_, UTF16ToWideHack(text),
                                      hint));
}

void RenderView::startDragging(const WebPoint& from, const WebDragData& data,
                               WebDragOperationsMask allowed_ops) {
  Send(new ViewHostMsg_StartDragging(routing_id_,
                                     WebDropData(data),
                                     allowed_ops));
}

bool RenderView::acceptsLoadDrops() {
  return renderer_preferences_.can_accept_load_drops;
}

void RenderView::focusNext() {
  Send(new ViewHostMsg_TakeFocus(routing_id_, false));
}

void RenderView::focusPrevious() {
  Send(new ViewHostMsg_TakeFocus(routing_id_, true));
}

void RenderView::navigateBackForwardSoon(int offset) {
  history_back_list_count_ += offset;
  history_forward_list_count_ -= offset;

  Send(new ViewHostMsg_GoToEntryAtOffset(routing_id_, offset));
}

int RenderView::historyBackListCount() {
  return history_back_list_count_;
}

int RenderView::historyForwardListCount() {
  return history_forward_list_count_;
}

void RenderView::didAddHistoryItem() {
  // We don't want to update the history length for the start page
  // navigation.
  WebFrame* main_frame = webview()->mainFrame();
  DCHECK(main_frame != NULL);

  WebDataSource* ds = main_frame->dataSource();
  DCHECK(ds != NULL);

  NavigationState* navigation_state = NavigationState::FromDataSource(ds);
  DCHECK(navigation_state);
  if (navigation_state->transition_type() == PageTransition::START_PAGE)
    return;

  history_back_list_count_++;
  history_forward_list_count_ = 0;
}

void RenderView::didUpdateInspectorSettings() {
  Send(new ViewHostMsg_UpdateInspectorSettings(
      routing_id_, webview()->inspectorSettings().utf8()));
}

void RenderView::queryAutofillSuggestions(const WebNode& node,
                                          const WebString& name,
                                          const WebString& value) {
  static int query_counter = 0;
  autofill_query_id_ = query_counter++;
  autofill_query_node_ = node;
  Send(new ViewHostMsg_QueryFormFieldAutofill(
      routing_id_, autofill_query_id_, name, value));
}

void RenderView::removeAutofillSuggestions(const WebString& name,
                                           const WebString& value) {
  Send(new ViewHostMsg_RemoveAutofillEntry(routing_id_, name, value));
}

// WebKit::WebWidgetClient ----------------------------------------------------

// We are supposed to get a single call to Show for a newly created RenderView
// that was created via RenderView::CreateWebView.  So, we wait until this
// point to dispatch the ShowView message.
//
// This method provides us with the information about how to display the newly
// created RenderView (i.e., as a constrained popup or as a new tab).
//
void RenderView::show(WebNavigationPolicy policy) {
  DCHECK(!did_show_) << "received extraneous Show call";
  DCHECK(opener_id_ != MSG_ROUTING_NONE);

  if (did_show_)
    return;
  did_show_ = true;

  // Force new windows to a popup if they were not opened with a user gesture.
  if (!opened_by_user_gesture_) {
    // We exempt background tabs for compat with older versions of Chrome.
    // TODO(darin): This seems bogus.  These should have a user gesture, so
    // we probably don't need this check.
    if (policy != WebKit::WebNavigationPolicyNewBackgroundTab)
      policy = WebKit::WebNavigationPolicyNewPopup;
  }

  // NOTE: initial_pos_ may still have its default values at this point, but
  // that's okay.  It'll be ignored if disposition is not NEW_POPUP, or the
  // browser process will impose a default position otherwise.
  Send(new ViewHostMsg_ShowView(opener_id_, routing_id_,
      NavigationPolicyToDisposition(policy), initial_pos_,
      opened_by_user_gesture_, creator_url_));
  SetPendingWindowRect(initial_pos_);
}

void RenderView::closeWidgetSoon() {
  if (!popup_notification_visible_)
    RenderWidget::closeWidgetSoon();
}

void RenderView::runModal() {
  DCHECK(did_show_) << "should already have shown the view";

  SendAndRunNestedMessageLoop(new ViewHostMsg_RunModal(routing_id_));
}

// WebKit::WebFrameClient -----------------------------------------------------

WebPlugin* RenderView::createPlugin(
    WebFrame* frame, const WebPluginParams& params) {
  return new webkit_glue::WebPluginImpl(frame, params, AsWeakPtr());
}

WebWorker* RenderView::createWorker(WebFrame* frame, WebWorkerClient* client) {
  return new WebWorkerProxy(client, RenderThread::current(), routing_id_);
}

WebMediaPlayer* RenderView::createMediaPlayer(
    WebFrame* frame, WebMediaPlayerClient* client) {
  scoped_refptr<media::FilterFactoryCollection> factory =
      new media::FilterFactoryCollection();
  // Add in any custom filter factories first.
  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(switches::kDisableAudio)) {
    // Add the chrome specific audio renderer.
    factory->AddFactory(
        AudioRendererImpl::CreateFactory(audio_message_filter()));
  }

  // TODO(hclam): obtain the following parameters from |client|.
  webkit_glue::MediaResourceLoaderBridgeFactory* bridge_factory =
      new webkit_glue::MediaResourceLoaderBridgeFactory(
          GURL::EmptyGURL(),  // referrer
          "null",             // frame origin
          "null",             // main_frame_origin
          base::GetCurrentProcId(),
          appcache::kNoHostId,
          routing_id());

  // A simple data source that keeps all data in memory.
  media::FilterFactory* simple_data_source_factory =
      webkit_glue::SimpleDataSource::CreateFactory(MessageLoop::current(),
                                                   bridge_factory);
  // A sophisticated data source that does memory caching.
  media::FilterFactory* buffered_data_source_factory =
      webkit_glue::BufferedDataSource::CreateFactory(MessageLoop::current(),
                                                     bridge_factory);
  if (cmd_line->HasSwitch(switches::kSimpleDataSource)) {
    factory->AddFactory(simple_data_source_factory);
    factory->AddFactory(buffered_data_source_factory);
  } else {
    factory->AddFactory(buffered_data_source_factory);
    factory->AddFactory(simple_data_source_factory);
  }
  return new webkit_glue::WebMediaPlayerImpl(client, factory);
}

void RenderView::willClose(WebFrame* frame) {
  if (!frame->parent()) {
    const GURL& url = frame->url();
    if (url.SchemeIs("http") || url.SchemeIs("https"))
      DumpLoadHistograms();
  }

  WebDataSource* ds = frame->dataSource();
  NavigationState* navigation_state = NavigationState::FromDataSource(ds);
  navigation_state->user_script_idle_scheduler()->Cancel();
}

void RenderView::loadURLExternally(
    WebFrame* frame, const WebURLRequest& request,
    WebNavigationPolicy policy) {
  GURL referrer(request.httpHeaderField(WebString::fromUTF8("Referer")));
  if (policy == WebKit::WebNavigationPolicyDownload) {
    Send(new ViewHostMsg_DownloadUrl(routing_id_, request.url(), referrer));
  } else {
    OpenURL(request.url(), referrer, policy);
  }
}

WebNavigationPolicy RenderView::decidePolicyForNavigation(
    WebFrame* frame, const WebURLRequest& request, WebNavigationType type,
    const WebNode&, WebNavigationPolicy default_policy, bool is_redirect) {
  // Webkit is asking whether to navigate to a new URL.
  // This is fine normally, except if we're showing UI from one security
  // context and they're trying to navigate to a different context.
  const GURL& url = request.url();

  // If the browser is interested, then give it a chance to look at top level
  // navigations
  if (renderer_preferences_.browser_handles_top_level_requests &&
      // Only send once.
      last_top_level_navigation_page_id_ != page_id_ &&
      // Not interested in reloads.
      type != WebKit::WebNavigationTypeReload &&
      type != WebKit::WebNavigationTypeFormSubmitted &&
      // Must be a top level frame.
      frame->parent() == NULL) {
    // Skip if navigation is on the same page (using '#').
    GURL frame_origin = GURL(frame->url()).GetOrigin();
    if (url.GetOrigin() != frame_origin || url.ref().empty()) {
      last_top_level_navigation_page_id_ = page_id_;
      GURL referrer(request.httpHeaderField(WebString::fromUTF8("Referer")));
      OpenURL(url, referrer, default_policy);
      return WebKit::WebNavigationPolicyIgnore;  // Suppress the load here.
    }
  }

  // A content initiated navigation may have originated from a link-click,
  // script, drag-n-drop operation, etc.
  bool is_content_initiated =
      NavigationState::FromDataSource(frame->provisionalDataSource())->
          is_content_initiated();

  // We only care about navigations that are within the current tab (as opposed
  // to, for example, opening a new window).
  // But we sometimes navigate to about:blank to clear a tab, and we want to
  // still allow that.
  if (default_policy == WebKit::WebNavigationPolicyCurrentTab &&
      is_content_initiated && frame->parent() == NULL &&
      !url.SchemeIs(chrome::kAboutScheme)) {
    // When we received such unsolicited navigations, we sometimes want to
    // punt them up to the browser to handle.
    if (BindingsPolicy::is_dom_ui_enabled(enabled_bindings_) ||
        BindingsPolicy::is_extension_enabled(enabled_bindings_) ||
        frame->isViewSourceModeEnabled() ||
        url.SchemeIs(chrome::kViewSourceScheme) ||
        url.SchemeIs(chrome::kPrintScheme)) {
      OpenURL(url, GURL(), default_policy);
      return WebKit::WebNavigationPolicyIgnore;  // Suppress the load here.
    }
  }

  // Detect when a page is "forking" a new tab that can be safely rendered in
  // its own process.  This is done by sites like Gmail that try to open links
  // in new windows without script connections back to the original page.  We
  // treat such cases as browser navigations (in which we will create a new
  // renderer for a cross-site navigation), rather than WebKit navigations.
  //
  // We use the following heuristic to decide whether to fork a new page in its
  // own process:
  // The parent page must open a new tab to about:blank, set the new tab's
  // window.opener to null, and then redirect the tab to a cross-site URL using
  // JavaScript.
  //
  // TODO(creis): Deprecate this logic once we can rely on rel=noreferrer
  // (see below).
  bool is_fork =
      // Must start from a tab showing about:blank, which is later redirected.
      GURL(frame->url()) == GURL(chrome::kAboutBlankURL) &&
      // Must be the first real navigation of the tab.
      historyBackListCount() < 1 &&
      historyForwardListCount() < 1 &&
      // The parent page must have set the child's window.opener to null before
      // redirecting to the desired URL.
      frame->opener() == NULL &&
      // Must be a top-level frame.
      frame->parent() == NULL &&
      // Must not have issued the request from this page.
      is_content_initiated &&
      // Must be targeted at the current tab.
      default_policy == WebKit::WebNavigationPolicyCurrentTab &&
      // Must be a JavaScript navigation, which appears as "other".
      type == WebKit::WebNavigationTypeOther;

  // Recognize if this navigation is from a link with rel=noreferrer and
  // target=_blank attributes, in which case the opener will be suppressed. If
  // so, it is safe to load cross-site pages in a separate process, so we
  // should let the browser handle it.
  bool is_noreferrer_and_blank_target =
      // Frame should be top level and not yet navigated.
      frame->parent() == NULL &&
      frame->url().isEmpty() &&
      historyBackListCount() < 1 &&
      historyForwardListCount() < 1 &&
      // Links with rel=noreferrer will have no Referer field, and their
      // resulting frame will have its window.opener suppressed.
      // TODO(creis): should add a request.httpReferrer() method to help avoid
      // typos on the unusual spelling of Referer.
      request.httpHeaderField(WebString::fromUTF8("Referer")).isNull() &&
      opener_suppressed_ &&
      frame->opener() == NULL &&
      // Links with target=_blank will have no name.
      frame->name().isNull() &&
      // Another frame (with a non-empty creator) should have initiated the
      // request, targeted at this frame.
      !creator_url_.is_empty() &&
      is_content_initiated &&
      default_policy == WebKit::WebNavigationPolicyCurrentTab &&
      type == WebKit::WebNavigationTypeOther;

  if (is_fork || is_noreferrer_and_blank_target) {
    // Open the URL via the browser, not via WebKit.
    OpenURL(url, GURL(), default_policy);
    return WebKit::WebNavigationPolicyIgnore;
  }

  return default_policy;
}

bool RenderView::canHandleRequest(
    WebFrame* frame, const WebURLRequest& request) {
  // We allow WebKit to think that everything can be handled even though
  // browser-side we limit what we load.
  return true;
}

WebURLError RenderView::cannotHandleRequestError(
    WebFrame* frame, const WebURLRequest& request) {
  NOTREACHED();  // Since we said we can handle all requests.
  return WebURLError();
}

WebURLError RenderView::cancelledError(
    WebFrame* frame, const WebURLRequest& request) {
  WebURLError error;
  error.domain = WebString::fromUTF8(net::kErrorDomain);
  error.reason = net::ERR_ABORTED;
  error.unreachableURL = request.url();
  return error;
}

void RenderView::unableToImplementPolicyWithError(
    WebFrame*, const WebURLError&) {
  NOTREACHED();  // Since we said we can handle all requests.
}

void RenderView::willSubmitForm(WebFrame* frame, const WebForm& form) {
  NavigationState* navigation_state =
      NavigationState::FromDataSource(frame->provisionalDataSource());

  if (navigation_state->transition_type() == PageTransition::LINK)
    navigation_state->set_transition_type(PageTransition::FORM_SUBMIT);

  // Save these to be processed when the ensuing navigation is committed.
  WebSearchableFormData web_searchable_form_data(form);
  navigation_state->set_searchable_form_url(web_searchable_form_data.url());
  navigation_state->set_searchable_form_encoding(
      webkit_glue::WebStringToStdString(web_searchable_form_data.encoding()));
  navigation_state->set_password_form_data(
      PasswordFormDomManager::CreatePasswordForm(form));

  if (form.isAutoCompleteEnabled()) {
    scoped_ptr<FormFieldValues> form_values(FormFieldValues::Create(form));
    if (form_values.get())
      Send(new ViewHostMsg_FormFieldValuesSubmitted(routing_id_,
                                                    *form_values));
  }
}

void RenderView::willPerformClientRedirect(
    WebFrame* frame, const WebURL& from, const WebURL& to, double interval,
    double fire_time) {
  // Ignore
}

void RenderView::didCancelClientRedirect(WebFrame* frame) {
  // Ignore
}

void RenderView::didCompleteClientRedirect(
    WebFrame* frame, const WebURL& from) {
  if (!frame->parent())
    completed_client_redirect_src_ = from;
}

void RenderView::didCreateDataSource(WebFrame* frame, WebDataSource* ds) {
  // The rest of RenderView assumes that a WebDataSource will always have a
  // non-null NavigationState.
  NavigationState* state = pending_navigation_state_.get() ?
      pending_navigation_state_.release() :
      NavigationState::CreateContentInitiated();

  state->set_user_script_idle_scheduler(
      new UserScriptIdleScheduler(this, frame));
  ds->setExtraData(state);
}

void RenderView::didStartProvisionalLoad(WebFrame* frame) {
  WebDataSource* ds = frame->provisionalDataSource();
  NavigationState* navigation_state = NavigationState::FromDataSource(ds);

  navigation_state->set_start_load_time(Time::Now());

  // Update the request time if WebKit has better knowledge of it.
  if (navigation_state->request_time().is_null()) {
    double event_time = ds->triggeringEventTime();
    if (event_time != 0.0)
      navigation_state->set_request_time(Time::FromDoubleT(event_time));
  }

  bool is_top_most = !frame->parent();
  if (is_top_most) {
    navigation_gesture_ = frame->isProcessingUserGesture() ?
        NavigationGestureUnknown : NavigationGestureAuto;

    // Make sure redirect tracking state is clear for the new load.
    completed_client_redirect_src_ = GURL();
  } else if (frame->parent()->isLoading()) {
    // Take note of AUTO_SUBFRAME loads here, so that we can know how to
    // load an error page.  See DidFailProvisionalLoadWithError.
    navigation_state->set_transition_type(PageTransition::AUTO_SUBFRAME);
  }

  Send(new ViewHostMsg_DidStartProvisionalLoadForFrame(
       routing_id_, is_top_most, ds->request().url()));
}

void RenderView::didReceiveServerRedirectForProvisionalLoad(WebFrame* frame) {
  if (frame->parent())
    return;
  // Received a redirect on the main frame.
  WebDataSource* data_source = frame->provisionalDataSource();
  if (!data_source) {
    // Should only be invoked when we have a data source.
    NOTREACHED();
    return;
  }
  std::vector<GURL> redirects;
  GetRedirectChain(data_source, &redirects);
  if (redirects.size() >= 2) {
    Send(new ViewHostMsg_DidRedirectProvisionalLoad(
         routing_id_, page_id_, redirects[redirects.size() - 2],
         redirects[redirects.size() - 1]));
  }
}

void RenderView::didFailProvisionalLoad(
    WebFrame* frame, const WebURLError& error) {
  // Notify the browser that we failed a provisional load with an error.
  //
  // Note: It is important this notification occur before DidStopLoading so the
  //       SSL manager can react to the provisional load failure before being
  //       notified the load stopped.
  //
  WebDataSource* ds = frame->provisionalDataSource();
  DCHECK(ds);

  const WebURLRequest& failed_request = ds->request();

  bool show_repost_interstitial =
      (error.reason == net::ERR_CACHE_MISS &&
       EqualsASCII(failed_request.httpMethod(), "POST"));
  Send(new ViewHostMsg_DidFailProvisionalLoadWithError(
      routing_id_, !frame->parent(), error.reason, error.unreachableURL,
      show_repost_interstitial));

  // Don't display an error page if this is simply a cancelled load.  Aside
  // from being dumb, WebCore doesn't expect it and it will cause a crash.
  if (error.reason == net::ERR_ABORTED)
    return;

  // Make sure we never show errors in view source mode.
  frame->enableViewSourceMode(false);

  NavigationState* navigation_state = NavigationState::FromDataSource(ds);

  // If this is a failed back/forward/reload navigation, then we need to do a
  // 'replace' load.  This is necessary to avoid messing up session history.
  // Otherwise, we do a normal load, which simulates a 'go' navigation as far
  // as session history is concerned.
  //
  // AUTO_SUBFRAME loads should always be treated as loads that do not advance
  // the page id.
  //
  bool replace =
      navigation_state->pending_page_id() != -1 ||
      navigation_state->transition_type() == PageTransition::AUTO_SUBFRAME;

  // If we failed on a browser initiated request, then make sure that our error
  // page load is regarded as the same browser initiated request.
  if (!navigation_state->is_content_initiated()) {
    pending_navigation_state_.reset(NavigationState::CreateBrowserInitiated(
        navigation_state->pending_page_id(),
        navigation_state->transition_type(),
        navigation_state->request_time()));
  }

  // Provide the user with a more helpful error page?
  if (MaybeLoadAlternateErrorPage(frame, error, replace))
    return;

  // Fallback to a local error page.
  LoadNavigationErrorPage(frame, failed_request, error, std::string(),
                          replace);
}

void RenderView::didReceiveDocumentData(
    WebFrame* frame, const char* data, size_t data_len,
    bool& prevent_default) {
  NavigationState* navigation_state =
      NavigationState::FromDataSource(frame->dataSource());
  if (!navigation_state->postpone_loading_data())
    return;

  // We're going to call commitDocumentData ourselves...
  prevent_default = true;

  // Continue buffering the response data for the original 404 page.  If it
  // grows too large, then we'll just let it through.
  navigation_state->append_postponed_data(data, data_len);
  if (navigation_state->postponed_data().size() >= 512) {
    navigation_state->set_postpone_loading_data(false);
    frame->commitDocumentData(navigation_state->postponed_data().data(),
                              navigation_state->postponed_data().size());
    navigation_state->clear_postponed_data();
  }
}

void RenderView::didCommitProvisionalLoad(
    WebFrame* frame, bool is_new_navigation) {
  NavigationState* navigation_state =
      NavigationState::FromDataSource(frame->dataSource());

  navigation_state->set_commit_load_time(Time::Now());
  if (is_new_navigation) {
    // When we perform a new navigation, we need to update the previous session
    // history entry with state for the page we are leaving.
    UpdateSessionHistory(frame);

    // We bump our Page ID to correspond with the new session history entry.
    page_id_ = next_page_id_++;

    MessageLoop::current()->PostDelayedTask(FROM_HERE,
        method_factory_.NewRunnableMethod(&RenderView::CapturePageInfo,
                                          page_id_, true),
        kDelayForForcedCaptureMs);
  } else {
    // Inspect the navigation_state on this frame to see if the navigation
    // corresponds to a session history navigation...  Note: |frame| may or
    // may not be the toplevel frame, but for the case of capturing session
    // history, the first committed frame suffices.  We keep track of whether
    // we've seen this commit before so that only capture session history once
    // per navigation.
    //
    // Note that we need to check if the page ID changed. In the case of a
    // reload, the page ID doesn't change, and UpdateSessionHistory gets the
    // previous URL and the current page ID, which would be wrong.
    if (navigation_state->pending_page_id() != -1 &&
        navigation_state->pending_page_id() != page_id_ &&
        !navigation_state->request_committed()) {
      // This is a successful session history navigation!
      UpdateSessionHistory(frame);
      page_id_ = navigation_state->pending_page_id();
    }
  }

  // Remember that we've already processed this request, so we don't update
  // the session history again.  We do this regardless of whether this is
  // a session history navigation, because if we attempted a session history
  // navigation without valid HistoryItem state, WebCore will think it is a
  // new navigation.
  navigation_state->set_request_committed(true);

  UpdateURL(frame);

  // If this committed load was initiated by a client redirect, we're
  // at the last stop now, so clear it.
  completed_client_redirect_src_ = GURL();

  // Check whether we have new encoding name.
  UpdateEncoding(frame, frame->view()->pageEncoding().utf8());
}

void RenderView::didClearWindowObject(WebFrame* frame) {
  if (BindingsPolicy::is_dom_automation_enabled(enabled_bindings_))
    BindDOMAutomationController(frame);
  if (BindingsPolicy::is_dom_ui_enabled(enabled_bindings_)) {
    dom_ui_bindings_.set_message_sender(this);
    dom_ui_bindings_.set_routing_id(routing_id_);
    dom_ui_bindings_.BindToJavascript(frame, L"chrome");
  }
  if (BindingsPolicy::is_external_host_enabled(enabled_bindings_)) {
    external_host_bindings_.set_message_sender(this);
    external_host_bindings_.set_routing_id(routing_id_);
    external_host_bindings_.BindToJavascript(frame, L"externalHost");
  }
}

void RenderView::didCreateDocumentElement(WebFrame* frame) {
  if (RenderThread::current()) {  // Will be NULL during unit tests.
    RenderThread::current()->user_script_slave()->InjectScripts(
        frame, UserScript::DOCUMENT_START);
  }
  if (view_type_ == ViewType::EXTENSION_TOOLSTRIP ||
      view_type_ == ViewType::EXTENSION_MOLE) {
    InjectToolstripCSS();
    ExtensionProcessBindings::SetViewType(webview(), view_type_);
  }

  // Notify the browser about non-blank documents loading in the top frame.
  GURL url = frame->url();
  if (url.is_valid() && url.spec() != chrome::kAboutBlankURL) {
    if (frame == webview()->mainFrame())
      Send(new ViewHostMsg_DocumentAvailableInMainFrame(routing_id_));
  }
}

void RenderView::didReceiveTitle(WebFrame* frame, const WebString& title) {
  UpdateTitle(frame, title);

  // Also check whether we have new encoding name.
  UpdateEncoding(frame, frame->view()->pageEncoding().utf8());
}

void RenderView::didFinishDocumentLoad(WebFrame* frame) {
  WebDataSource* ds = frame->dataSource();
  NavigationState* navigation_state = NavigationState::FromDataSource(ds);
  DCHECK(navigation_state);
  navigation_state->set_finish_document_load_time(Time::Now());

  Send(new ViewHostMsg_DocumentLoadedInFrame(routing_id_));

  // The document has now been fully loaded.  Scan for password forms to be
  // sent up to the browser.
  SendPasswordForms(frame);

  // Check whether we have new encoding name.
  UpdateEncoding(frame, frame->view()->pageEncoding().utf8());

  if (RenderThread::current()) {  // Will be NULL during unit tests.
    RenderThread::current()->user_script_slave()->InjectScripts(
        frame, UserScript::DOCUMENT_END);
  }

  navigation_state->user_script_idle_scheduler()->DidFinishDocumentLoad();
}

void RenderView::OnUserScriptIdleTriggered(WebFrame* frame) {
  if (RenderThread::current()) {  // Will be NULL during unit tests.
    RenderThread::current()->user_script_slave()->InjectScripts(
        frame, UserScript::DOCUMENT_IDLE);
  }

  WebFrame* main_frame = webview()->mainFrame();
  if (frame == main_frame) {
    while (!pending_code_execution_queue_.empty()) {
      scoped_refptr<CodeExecutionInfo> info =
          pending_code_execution_queue_.front();
      ExecuteCodeImpl(main_frame, info->request_id, info->extension_id,
                      info->is_js_code, info->code_string);
      pending_code_execution_queue_.pop();
    }
  }
}

void RenderView::didHandleOnloadEvents(WebFrame* frame) {
  // Ignore
}

void RenderView::didFailLoad(WebFrame* frame, const WebURLError& error) {
  // Ignore
}

void RenderView::didFinishLoad(WebFrame* frame) {
  WebDataSource* ds = frame->dataSource();
  NavigationState* navigation_state = NavigationState::FromDataSource(ds);
  DCHECK(navigation_state);
  navigation_state->set_finish_load_time(Time::Now());
  navigation_state->user_script_idle_scheduler()->DidFinishLoad();
}

void RenderView::didChangeLocationWithinPage(
    WebFrame* frame, bool is_new_navigation) {
  // If this was a reference fragment navigation that we initiated, then we
  // could end up having a non-null pending navigation state.  We just need to
  // update the ExtraData on the datasource so that others who read the
  // ExtraData will get the new NavigationState.  Similarly, if we did not
  // initiate this navigation, then we need to take care to reset any pre-
  // existing navigation state to a content-initiated navigation state.
  // DidCreateDataSource conveniently takes care of this for us.
  didCreateDataSource(frame, frame->dataSource());

  didCommitProvisionalLoad(frame, is_new_navigation);

  UpdateTitle(frame, frame->view()->mainFrame()->dataSource()->pageTitle());
}

void RenderView::didUpdateCurrentHistoryItem(WebFrame* frame) {
  StartNavStateSyncTimerIfNecessary();
}

void RenderView::assignIdentifierToRequest(
    WebFrame* frame, unsigned identifier, const WebURLRequest& request) {
  // Ignore
}

void RenderView::willSendRequest(
    WebFrame* frame, unsigned identifier, WebURLRequest& request,
    const WebURLResponse& redirect_response) {
  WebFrame* top_frame = frame->top();
  if (!top_frame)
    top_frame = frame;
  WebDataSource* data_source = top_frame->provisionalDataSource();
  if (!data_source)
    data_source = top_frame->dataSource();
  if (data_source) {
    NavigationState* state = NavigationState::FromDataSource(data_source);
    if (state && state->is_cache_policy_override_set())
      request.setCachePolicy(state->cache_policy_override());
  }
  request.setRequestorID(routing_id_);
}

void RenderView::didReceiveResponse(
    WebFrame* frame, unsigned identifier, const WebURLResponse& response) {
  // Consider loading an alternate error page for 404 responses.
  if (response.httpStatusCode() != 404)
    return;

  // Only do this for responses that correspond to a provisional data source
  // of the top-most frame.  If we have a provisional data source, then we
  // can't have any sub-resources yet, so we know that this response must
  // correspond to a frame load.
  if (!frame->provisionalDataSource() || frame->parent())
    return;

  // If we are in view source mode, then just let the user see the source of
  // the server's 404 error page.
  if (frame->isViewSourceModeEnabled())
    return;

  // Can we even load an alternate error page for this URL?
  if (!GetAlternateErrorPageURL(response.url(), HTTP_404).is_valid())
    return;

  NavigationState* navigation_state =
      NavigationState::FromDataSource(frame->provisionalDataSource());
  navigation_state->set_postpone_loading_data(true);
  navigation_state->clear_postponed_data();
}

void RenderView::didFinishResourceLoad(
    WebFrame* frame, unsigned identifier) {
  NavigationState* navigation_state =
      NavigationState::FromDataSource(frame->dataSource());
  if (!navigation_state->postpone_loading_data())
    return;

  // The server returned a 404 and the content was < 512 bytes (which we
  // suppressed).  Go ahead and fetch the alternate page content.

  const GURL& frame_url = frame->url();

  const GURL& error_page_url = GetAlternateErrorPageURL(frame_url, HTTP_404);
  DCHECK(error_page_url.is_valid());

  WebURLError original_error;
  original_error.unreachableURL = frame_url;

  navigation_state->set_alt_error_page_fetcher(
      new AltErrorPageResourceFetcher(
          error_page_url, frame, original_error,
          NewCallback(this, &RenderView::AltErrorPageFinished)));
}

void RenderView::didFailResourceLoad(
    WebFrame* frame, unsigned identifier, const WebURLError& error) {
  // Ignore
}

void RenderView::didLoadResourceFromMemoryCache(
    WebFrame* frame, const WebURLRequest& request,
    const WebURLResponse& response) {
  // Let the browser know we loaded a resource from the memory cache.  This
  // message is needed to display the correct SSL indicators.
  Send(new ViewHostMsg_DidLoadResourceFromMemoryCache(
      routing_id_,
      request.url(),
      frame->securityOrigin().toString().utf8(),
      frame->top()->securityOrigin().toString().utf8(),
      response.securityInfo()));
}

void RenderView::didDisplayInsecureContent(WebFrame* frame) {
  Send(new ViewHostMsg_DidDisplayInsecureContent(routing_id_));
}

void RenderView::didRunInsecureContent(
    WebFrame* frame, const WebSecurityOrigin& origin) {
  Send(new ViewHostMsg_DidRunInsecureContent(
      routing_id_,
      origin.toString().utf8()));
}

bool RenderView::allowScript(WebFrame* frame, bool enabled_per_settings) {
  if (enabled_per_settings)
    return true;

  WebSecurityOrigin origin = frame->securityOrigin();
  if (origin.isEmpty())
    return false;  // Uninitialized document?

  if (EqualsASCII(origin.protocol(), chrome::kChromeUIScheme))
    return true;  // Browser UI elements should still work.

  // If the scheme is ftp: or file:, an empty file name indicates a directory
  // listing, which requires JavaScript to function properly.
  GURL frame_url = frame->url();
  const char* kDirProtocols[] = { "ftp", "file" };
  for (size_t i = 0; i < arraysize(kDirProtocols); ++i) {
    if (EqualsASCII(origin.protocol(), kDirProtocols[i])) {
      return frame_url.SchemeIs(kDirProtocols[i]) &&
             frame_url.ExtractFileName().empty();
    }
  }

  return false;  // Other protocols fall through here.
}

void RenderView::didExhaustMemoryAvailableForScript(WebFrame* frame) {
  Send(new ViewHostMsg_JSOutOfMemory(routing_id_));
}

void RenderView::didCreateScriptContext(WebFrame* frame) {
  EventBindings::HandleContextCreated(frame, false);
}

void RenderView::didDestroyScriptContext(WebFrame* frame) {
  EventBindings::HandleContextDestroyed(frame);
}

void RenderView::didCreateIsolatedScriptContext(WebFrame* frame) {
  EventBindings::HandleContextCreated(frame, true);
}

void RenderView::didChangeContentsSize(WebFrame* frame, const WebSize& size) {
  // We don't always want to send the change messages over IPC, only if we've
  // be put in that mode by getting a |ViewMsg_EnablePreferredSizeChangedMode|
  // message.
  if (send_preferred_size_changes_) {
    // WebCore likes to tell us things have changed even when they haven't, so
    // cache the width and height and only send the IPC message when we're sure
    // they're different.
    int width = webview()->mainFrame()->contentsPreferredWidth();
    if (width != preferred_size_.width() ||
        size.height != preferred_size_.height()) {
      preferred_size_.set_width(width);

      // TODO(erikkay) the contents size is not really the same as the
      // preferred size.  It's just the current size.  This means that for
      // height, it will only ever grow, it will never shrink.
      preferred_size_.set_height(size.height);
      Send(new ViewHostMsg_DidContentsPreferredSizeChange(routing_id_,
                                                          preferred_size_));
    }
  }
}

void RenderView::reportFindInPageMatchCount(int request_id, int count,
                                            bool final_update) {
  // If we have a message that has been queued up, then we should just replace
  // it. The ACK from the browser will make sure it gets sent when the browser
  // wants it.
  if (queued_find_reply_message_.get()) {
    IPC::Message* msg = new ViewHostMsg_Find_Reply(
        routing_id_,
        request_id,
        count,
        gfx::Rect(),
        -1,  // Don't update active match ordinal.
        final_update);
    queued_find_reply_message_.reset(msg);
  } else {
    // Send the search result over to the browser process.
    Send(new ViewHostMsg_Find_Reply(
        routing_id_,
        request_id,
        count,
        gfx::Rect(),
        -1,  // // Don't update active match ordinal.
        final_update));
  }
}

void RenderView::reportFindInPageSelection(int request_id,
                                           int active_match_ordinal,
                                           const WebRect& selection_rect) {
  // Send the search result over to the browser process.
  Send(new ViewHostMsg_Find_Reply(routing_id_,
                                  request_id,
                                  -1,
                                  selection_rect,
                                  active_match_ordinal,
                                  false));
}

// webkit_glue::WebPluginPageDelegate -----------------------------------------

webkit_glue::WebPluginDelegate* RenderView::CreatePluginDelegate(
    const GURL& url,
    const std::string& mime_type,
    std::string* actual_mime_type) {
  if (!PluginChannelHost::IsListening())
    return NULL;

  GURL policy_url;
  WebFrame* main_frame = webview()->mainFrame();
  if (main_frame)
    policy_url = main_frame->url();

  FilePath path;
  render_thread_->Send(new ViewHostMsg_GetPluginPath(
      url, policy_url, mime_type, &path, actual_mime_type));
  if (path.value().empty())
    return NULL;

  const std::string* mime_type_to_use;
  if (!actual_mime_type->empty())
    mime_type_to_use = actual_mime_type;
  else
    mime_type_to_use = &mime_type;

#if defined(PEPPER_APIS_ENABLED)
  const char kPepperPrefix[] = "pepper-";
  if (StartsWithASCII(*mime_type_to_use, kPepperPrefix, true)) {
    return WebPluginDelegatePepper::Create(
        path, *mime_type_to_use, gfx::NativeViewFromId(host_window_));
  }
#endif

  bool in_process_plugin = RenderProcess::current()->in_process_plugins();
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kInternalNaCl)) {
    if (mime_type == "application/x-nacl-srpc") {
      in_process_plugin = true;
    }
  }
  if (in_process_plugin) {
#if defined(OS_WIN)  // In-proc plugins aren't supported on Linux or Mac.
    return WebPluginDelegateImpl::Create(
        path, *mime_type_to_use, gfx::NativeViewFromId(host_window_));
#else
    NOTIMPLEMENTED();
    return NULL;
#endif
  }

  return new WebPluginDelegateProxy(*mime_type_to_use, AsWeakPtr());
}

void RenderView::CreatedPluginWindow(gfx::PluginWindowHandle window) {
#if defined(OS_LINUX)
  RenderThread::current()->Send(new ViewHostMsg_CreatePluginContainer(
      routing_id(), window));
#endif
}

void RenderView::WillDestroyPluginWindow(gfx::PluginWindowHandle window) {
#if defined(OS_LINUX)
  RenderThread::current()->Send(new ViewHostMsg_DestroyPluginContainer(
      routing_id(), window));
#endif
  CleanupWindowInPluginMoves(window);
}

void RenderView::DidMovePlugin(const webkit_glue::WebPluginGeometry& move) {
  SchedulePluginMove(move);
}

void RenderView::DidStartLoadingForPlugin() {
  // TODO(darin): Make is_loading_ be a counter!
  didStartLoading();
}

void RenderView::DidStopLoadingForPlugin() {
  // TODO(darin): Make is_loading_ be a counter!
  didStopLoading();
}

void RenderView::ShowModalHTMLDialogForPlugin(
    const GURL& url,
    const gfx::Size& size,
    const std::string& json_arguments,
    std::string* json_retval) {
  SendAndRunNestedMessageLoop(new ViewHostMsg_ShowModalHTMLDialog(
      routing_id_, url, size.width(), size.height(), json_arguments,
      json_retval));
}

void RenderView::SyncNavigationState() {
  if (!webview())
    return;

  const WebHistoryItem& item = webview()->mainFrame()->currentHistoryItem();
  if (item.isNull())
    return;

  Send(new ViewHostMsg_UpdateState(
      routing_id_, page_id_, webkit_glue::HistoryItemToString(item)));
}

bool RenderView::DownloadImage(int id, const GURL& image_url, int image_size) {
  // Make sure webview was not shut down.
  if (!webview())
    return false;
  // Create an image resource fetcher and assign it with a call back object.
  image_fetchers_.insert(new ImageResourceFetcher(
      image_url, webview()->mainFrame(), id, image_size,
      NewCallback(this, &RenderView::DidDownloadImage)));
  return true;
}

void RenderView::DidDownloadImage(ImageResourceFetcher* fetcher,
                                  const SkBitmap& image) {
  // Notify requester of image download status.
  Send(new ViewHostMsg_DidDownloadFavIcon(routing_id_,
                                          fetcher->id(),
                                          fetcher->image_url(),
                                          image.isNull(),
                                          image));
  // Dispose of the image fetcher.
  DCHECK(image_fetchers_.find(fetcher) != image_fetchers_.end());
  image_fetchers_.erase(fetcher);
  // We're in the callback from the ImageResourceFetcher, best to delay
  // deletion.
  MessageLoop::current()->DeleteSoon(FROM_HERE, fetcher);
}

void RenderView::OnDownloadFavIcon(int id,
                                   const GURL& image_url,
                                   int image_size) {
  bool data_image_failed = false;
  if (image_url.SchemeIs("data")) {
    SkBitmap data_image = ImageFromDataUrl(image_url);
    data_image_failed = data_image.empty();
    if (!data_image_failed) {
      Send(new ViewHostMsg_DidDownloadFavIcon(routing_id_, id, image_url, false,
                                              data_image));
    }
  }

  if (data_image_failed ||
      !DownloadImage(id, image_url, image_size)) {
    Send(new ViewHostMsg_DidDownloadFavIcon(routing_id_, id, image_url, true,
                                            SkBitmap()));
  }
}

SkBitmap RenderView::ImageFromDataUrl(const GURL& url) const {
  std::string mime_type, char_set, data;
  if (net::DataURL::Parse(url, &mime_type, &char_set, &data) && !data.empty()) {
    // Decode the favicon using WebKit's image decoder.
    webkit_glue::ImageDecoder decoder(gfx::Size(kFavIconSize, kFavIconSize));
    const unsigned char* src_data =
        reinterpret_cast<const unsigned char*>(&data[0]);

    return decoder.Decode(src_data, data.size());
  }
  return SkBitmap();
}

void RenderView::OnGetApplicationInfo(int page_id) {
  webkit_glue::WebApplicationInfo app_info;
  if (page_id == page_id_)
    webkit_glue::GetApplicationInfo(webview(), &app_info);

  // Prune out any data URLs in the set of icons.  The browser process expects
  // any icon with a data URL to have originated from a favicon.  We don't want
  // to decode arbitrary data URLs in the browser process.  See
  // http://b/issue?id=1162972
  for (size_t i = 0; i < app_info.icons.size(); ++i) {
    if (app_info.icons[i].url.SchemeIs(chrome::kDataScheme)) {
      app_info.icons.erase(app_info.icons.begin() + i);
      --i;
    }
  }

  Send(new ViewHostMsg_DidGetApplicationInfo(routing_id_, page_id, app_info));
}

GURL RenderView::GetAlternateErrorPageURL(const GURL& failed_url,
                                          ErrorPageType error_type) {
  if (failed_url.SchemeIsSecure()) {
    // If the URL that failed was secure, then the embedding web page was not
    // expecting a network attacker to be able to manipulate its contents.  As
    // we fetch alternate error pages over HTTP, we would be allowing a network
    // attacker to manipulate the contents of the response if we tried to use
    // the link doctor here.
    return GURL::EmptyGURL();
  }

  // Grab the base URL from the browser process.
  if (!alternate_error_page_url_.is_valid())
    return GURL::EmptyGURL();

  // Strip query params from the failed URL.
  GURL::Replacements remove_params;
  remove_params.ClearUsername();
  remove_params.ClearPassword();
  remove_params.ClearQuery();
  remove_params.ClearRef();
  const GURL url_to_send = failed_url.ReplaceComponents(remove_params);

  // Construct the query params to send to link doctor.
  std::string params(alternate_error_page_url_.query());
  params.append("&url=");
  params.append(EscapeQueryParamValue(url_to_send.spec()));
  params.append("&sourceid=chrome");
  params.append("&error=");
  switch (error_type) {
    case DNS_ERROR:
      params.append("dnserror");
      break;

    case HTTP_404:
      params.append("http404");
      break;

    case CONNECTION_ERROR:
      params.append("connectionfailure");
      break;

    default:
      NOTREACHED() << "unknown ErrorPageType";
  }

  // OK, build the final url to return.
  GURL::Replacements link_doctor_params;
  link_doctor_params.SetQueryStr(params);
  GURL url = alternate_error_page_url_.ReplaceComponents(link_doctor_params);
  return url;
}

void RenderView::OnFind(int request_id, const string16& search_text,
                        const WebFindOptions& options) {
  WebFrame* main_frame = webview()->mainFrame();
  WebFrame* frame_after_main = main_frame->traverseNext(true);
  WebFrame* focused_frame = webview()->focusedFrame();
  WebFrame* search_frame = focused_frame;  // start searching focused frame.

  bool multi_frame = (frame_after_main != main_frame);

  // If we have multiple frames, we don't want to wrap the search within the
  // frame, so we check here if we only have main_frame in the chain.
  bool wrap_within_frame = !multi_frame;

  WebRect selection_rect;
  bool result = false;

  do {
    result = search_frame->find(
        request_id, search_text, options, wrap_within_frame, &selection_rect);

    if (!result) {
      // don't leave text selected as you move to the next frame.
      search_frame->executeCommand(WebString::fromUTF8("Unselect"));

      // Find the next frame, but skip the invisible ones.
      do {
        // What is the next frame to search? (we might be going backwards). Note
        // that we specify wrap=true so that search_frame never becomes NULL.
        search_frame = options.forward ?
            search_frame->traverseNext(true) :
            search_frame->traversePrevious(true);
      } while (!search_frame->hasVisibleContent() &&
               search_frame != focused_frame);

      // Make sure selection doesn't affect the search operation in new frame.
      search_frame->executeCommand(WebString::fromUTF8("Unselect"));

      // If we have multiple frames and we have wrapped back around to the
      // focused frame, we need to search it once more allowing wrap within
      // the frame, otherwise it will report 'no match' if the focused frame has
      // reported matches, but no frames after the focused_frame contain a
      // match for the search word(s).
      if (multi_frame && search_frame == focused_frame) {
        result = search_frame->find(
            request_id, search_text, options, true,  // Force wrapping.
            &selection_rect);
      }
    }

    webview()->setFocusedFrame(search_frame);
  } while (!result && search_frame != focused_frame);

  if (options.findNext) {
    // Force the main_frame to report the actual count.
    main_frame->increaseMatchCount(0, request_id);
  } else {
    // If nothing is found, set result to "0 of 0", otherwise, set it to
    // "-1 of 1" to indicate that we found at least one item, but we don't know
    // yet what is active.
    int ordinal = result ? -1 : 0;  // -1 here means, we might know more later.
    int match_count = result ? 1 : 0;  // 1 here means possibly more coming.

    // If we find no matches then this will be our last status update.
    // Otherwise the scoping effort will send more results.
    bool final_status_update = !result;

    // Send the search result over to the browser process.
    Send(new ViewHostMsg_Find_Reply(routing_id_,
                                    request_id,
                                    match_count,
                                    selection_rect,
                                    ordinal,
                                    final_status_update));

    // Scoping effort begins, starting with the mainframe.
    search_frame = main_frame;

    main_frame->resetMatchCount();

    do {
      // Cancel all old scoping requests before starting a new one.
      search_frame->cancelPendingScopingEffort();

      // We don't start another scoping effort unless at least one match has
      // been found.
      if (result) {
        // Start new scoping request. If the scoping function determines that it
        // needs to scope, it will defer until later.
        search_frame->scopeStringMatches(request_id,
                                         search_text,
                                         options,
                                         true);  // reset the tickmarks
      }

      // Iterate to the next frame. The frame will not necessarily scope, for
      // example if it is not visible.
      search_frame = search_frame->traverseNext(true);
    } while (search_frame != main_frame);
  }
}

void RenderView::OnDeterminePageText() {
  if (!is_loading_) {
    if (!webview())
      return;
    WebFrame* main_frame = webview()->mainFrame();
    std::wstring contents;
    CaptureText(main_frame, &contents);
    Send(new ViewMsg_DeterminePageText_Reply(routing_id_, contents));
    determine_page_text_after_loading_stops_ = false;
    return;
  }

  // We set |determine_page_text_after_loading_stops_| true here so that,
  // after page has been loaded completely, the text in the page is captured.
  determine_page_text_after_loading_stops_ = true;
}

void RenderView::DnsPrefetch(const std::vector<std::string>& host_names) {
  Send(new ViewHostMsg_DnsPrefetch(host_names));
}

void RenderView::OnZoom(int function) {
  static const bool kZoomIsTextOnly = false;
  switch (function) {
    case PageZoom::SMALLER:
      webview()->zoomOut(kZoomIsTextOnly);
      break;
    case PageZoom::STANDARD:
      webview()->zoomDefault();
      break;
    case PageZoom::LARGER:
      webview()->zoomIn(kZoomIsTextOnly);
      break;
    default:
      NOTREACHED();
  }
}

void RenderView::OnSetPageEncoding(const std::string& encoding_name) {
  webview()->setPageEncoding(WebString::fromUTF8(encoding_name));
}

void RenderView::OnResetPageEncodingToDefault() {
  WebString no_encoding;
  webview()->setPageEncoding(no_encoding);
}

WebFrame* RenderView::GetChildFrame(const std::wstring& xpath) const {
  if (xpath.empty())
    return webview()->mainFrame();

  // xpath string can represent a frame deep down the tree (across multiple
  // frame DOMs).
  // Example, /html/body/table/tbody/tr/td/iframe\n/frameset/frame[0]
  // should break into 2 xpaths
  // /html/body/table/tbody/tr/td/iframe & /frameset/frame[0]

  WebFrame* frame = webview()->mainFrame();

  std::wstring xpath_remaining = xpath;
  while (!xpath_remaining.empty()) {
    std::wstring::size_type delim_pos = xpath_remaining.find_first_of(L'\n');
    std::wstring xpath_child;
    if (delim_pos != std::wstring::npos) {
      xpath_child = xpath_remaining.substr(0, delim_pos);
      xpath_remaining.erase(0, delim_pos + 1);
    } else {
      xpath_remaining.swap(xpath_child);
    }
    frame = frame->findChildByExpression(WideToUTF16Hack(xpath_child));
  }

  return frame;
}

void RenderView::EvaluateScript(const std::wstring& frame_xpath,
                                const std::wstring& script) {
  WebFrame* web_frame = GetChildFrame(frame_xpath);
  if (!web_frame)
    return;

  web_frame->executeScript(WebScriptSource(WideToUTF16Hack(script)));
}

void RenderView::InsertCSS(const std::wstring& frame_xpath,
                           const std::string& css,
                           const std::string& id) {
  WebFrame* web_frame = GetChildFrame(frame_xpath);
  if (!web_frame)
    return;

  web_frame->insertStyleText(WebString::fromUTF8(css), WebString::fromUTF8(id));
}

void RenderView::OnScriptEvalRequest(const std::wstring& frame_xpath,
                                     const std::wstring& jscript) {
  EvaluateScript(frame_xpath, jscript);
}

void RenderView::OnCSSInsertRequest(const std::wstring& frame_xpath,
                                    const std::string& css,
                                    const std::string& id) {
  InsertCSS(frame_xpath, css, id);

  // Notify RenderViewHost that css has been inserted into the frame.
  Send(new ViewHostMsg_OnCSSInserted(routing_id_));
}

void RenderView::OnAddMessageToConsole(
    const string16& frame_xpath,
    const string16& message,
    const WebConsoleMessage::Level& level) {
  WebFrame* web_frame = GetChildFrame(UTF16ToWideHack(frame_xpath));
  if (web_frame)
    web_frame->addMessageToConsole(WebConsoleMessage(level, message));
}

void RenderView::OnAllowBindings(int enabled_bindings_flags) {
  enabled_bindings_ |= enabled_bindings_flags;
}

void RenderView::OnSetDOMUIProperty(const std::string& name,
                                    const std::string& value) {
  DCHECK(BindingsPolicy::is_dom_ui_enabled(enabled_bindings_));
  dom_ui_bindings_.SetProperty(name, value);
}

void RenderView::OnReservePageIDRange(int size_of_range) {
  next_page_id_ += size_of_range + 1;
}

void RenderView::OnDragSourceEndedOrMoved(const gfx::Point& client_point,
                                          const gfx::Point& screen_point,
                                          bool ended,
                                          WebDragOperation op) {
  if (ended) {
    webview()->dragSourceEndedAt(client_point, screen_point, op);
  } else {
    webview()->dragSourceMovedTo(client_point, screen_point);
  }
}

void RenderView::OnDragSourceSystemDragEnded() {
  webview()->dragSourceSystemDragEnded();
}

void RenderView::OnFormFill(const FormData& form) {
  webkit_glue::FillForm(this->webview(), form);
}

void RenderView::OnFillPasswordForm(
    const webkit_glue::PasswordFormDomManager::FillData& form_data) {
  webkit_glue::FillPasswordForm(this->webview(), form_data);
}

void RenderView::OnDragTargetDragEnter(const WebDropData& drop_data,
                                       const gfx::Point& client_point,
                                       const gfx::Point& screen_point,
                                       WebDragOperationsMask ops) {
  WebDragOperation operation = webview()->dragTargetDragEnter(
      drop_data.ToDragData(),
      drop_data.identity,
      client_point,
      screen_point,
      ops);

  Send(new ViewHostMsg_UpdateDragCursor(routing_id_, operation));
}

void RenderView::OnDragTargetDragOver(const gfx::Point& client_point,
                                      const gfx::Point& screen_point,
                                      WebDragOperationsMask ops) {
  WebDragOperation operation = webview()->dragTargetDragOver(
      client_point,
      screen_point,
      ops);

  Send(new ViewHostMsg_UpdateDragCursor(routing_id_, operation));
}

void RenderView::OnDragTargetDragLeave() {
  webview()->dragTargetDragLeave();
}

void RenderView::OnDragTargetDrop(const gfx::Point& client_point,
                                  const gfx::Point& screen_point) {
  webview()->dragTargetDrop(client_point, screen_point);
}

void RenderView::OnUpdateWebPreferences(const WebPreferences& prefs) {
  webkit_preferences_ = prefs;
  webkit_preferences_.Apply(webview());
}

void RenderView::OnSetAltErrorPageURL(const GURL& url) {
  alternate_error_page_url_ = url;
}

void RenderView::OnInstallMissingPlugin() {
  // This could happen when the first default plugin is deleted.
  if (first_default_plugin_)
    first_default_plugin_->InstallMissingPlugin();
}

void RenderView::OnFileChooserResponse(
    const std::vector<FilePath>& file_names) {
  // This could happen if we navigated to a different page before the user
  // closed the chooser.
  if (!file_chooser_completion_)
    return;

  WebVector<WebString> ws_file_names(file_names.size());
  for (size_t i = 0; i < file_names.size(); ++i) {
    ws_file_names[i] = webkit_glue::FilePathToWebString(file_names[i]);
  }

  file_chooser_completion_->didChooseFile(ws_file_names);
  // Reset the chooser pointer
  file_chooser_completion_ = NULL;
}

void RenderView::OnEnableViewSourceMode() {
  if (!webview())
    return;
  WebFrame* main_frame = webview()->mainFrame();
  if (!main_frame)
    return;

  main_frame->enableViewSourceMode(true);
}

void RenderView::OnEnablePreferredSizeChangedMode() {
  send_preferred_size_changes_ = true;
}

void RenderView::OnSetRendererPrefs(const RendererPreferences& renderer_prefs) {
  renderer_preferences_ = renderer_prefs;
  UpdateFontRenderingFromRendererPrefs();
#if defined(OS_LINUX) && !defined(TOOLKIT_VIEWS)
  WebColorName name = WebKit::WebColorWebkitFocusRingColor;
  WebKit::setNamedColors(&name, &renderer_prefs.focus_ring_color, 1);
#endif
}

void RenderView::OnMediaPlayerActionAt(const gfx::Point& location,
                                       const WebMediaPlayerAction& action) {
  if (webview())
    webview()->performMediaPlayerAction(action, location);
}

void RenderView::OnNotifyRendererViewType(ViewType::Type type) {
  // When this is first set, the bindings aren't fully loaded.  We only need
  // to call through this API after the page has already been loaded.  It's
  // also called in didCreateDocumentElement to bootstrap.
  if (view_type_ != ViewType::INVALID) {
    if (type == ViewType::EXTENSION_MOLE ||
        type == ViewType::EXTENSION_TOOLSTRIP) {
      ExtensionProcessBindings::SetViewType(webview(), type);
    }
  }
  view_type_ = type;
}

void RenderView::OnUpdateBrowserWindowId(int window_id) {
  browser_window_id_ = window_id;
}

void RenderView::OnUpdateBackForwardListCount(int back_list_count,
                                              int forward_list_count) {
  history_back_list_count_ = back_list_count;
  history_forward_list_count_ = forward_list_count;
}

void RenderView::OnGetAccessibilityInfo(
    const webkit_glue::WebAccessibility::InParams& in_params,
    webkit_glue::WebAccessibility::OutParams* out_params) {
#if defined(OS_WIN)
  if (!accessibility_.get()) {
    // TODO(dglazkov): Once implemented for all ports, remove lazy
    // instantiation of accessibility_.
    accessibility_.reset(WebAccessibilityCache::create());
    accessibility_->initialize(webview());
  }

  webkit_glue::WebAccessibility::GetAccObjInfo(accessibility_.get(),
                                               in_params,
                                               out_params);

#else  // defined(OS_WIN)
  // TODO(port): accessibility not yet implemented
  NOTIMPLEMENTED();
#endif
}

void RenderView::OnClearAccessibilityInfo(int acc_obj_id, bool clear_all) {
#if defined(OS_WIN)
  if (!accessibility_.get()) {
    // If accessibility is not activated, ignore clearing message.
    return;
  }

  if (clear_all) {
    accessibility_->clear();
    return;
  }

  accessibility_->remove(acc_obj_id);

#else  // defined(OS_WIN)
  // TODO(port): accessibility not yet implemented
  NOTIMPLEMENTED();
#endif
}

void RenderView::OnGetAllSavableResourceLinksForCurrentPage(
    const GURL& page_url) {
  // Prepare list to storage all savable resource links.
  std::vector<GURL> resources_list;
  std::vector<GURL> referrers_list;
  std::vector<GURL> frames_list;
  webkit_glue::SavableResourcesResult result(&resources_list,
                                             &referrers_list,
                                             &frames_list);

  if (!webkit_glue::GetAllSavableResourceLinksForCurrentPage(
          webview(),
          page_url,
          &result,
          chrome::kSavableSchemes)) {
    // If something is wrong when collecting all savable resource links,
    // send empty list to embedder(browser) to tell it failed.
    referrers_list.clear();
    resources_list.clear();
    frames_list.clear();
  }

  // Send result of all savable resource links to embedder.
  Send(new ViewHostMsg_SendCurrentPageAllSavableResourceLinks(routing_id_,
                                                              resources_list,
                                                              referrers_list,
                                                              frames_list));
}

void RenderView::OnGetSerializedHtmlDataForCurrentPageWithLocalLinks(
    const std::vector<GURL>& links,
    const std::vector<FilePath>& local_paths,
    const FilePath& local_directory_name) {
  webkit_glue::DomSerializer dom_serializer(webview()->mainFrame(),
                                            true,
                                            this,
                                            links,
                                            local_paths,
                                            local_directory_name);
  dom_serializer.SerializeDom();
}

void RenderView::DidSerializeDataForFrame(const GURL& frame_url,
    const std::string& data, PageSavingSerializationStatus status) {
  Send(new ViewHostMsg_SendSerializedHtmlData(routing_id_,
      frame_url, data, static_cast<int32>(status)));
}

void RenderView::OnMsgShouldClose() {
  bool should_close = webview()->dispatchBeforeUnloadEvent();
  Send(new ViewHostMsg_ShouldClose_ACK(routing_id_, should_close));
}

void RenderView::OnClosePage(const ViewMsg_ClosePage_Params& params) {
  // TODO(creis): We'd rather use webview()->Close() here, but that currently
  // sets the WebView's delegate_ to NULL, preventing any JavaScript dialogs
  // in the onunload handler from appearing.  For now, we're bypassing that and
  // calling the FrameLoader's CloseURL method directly.  This should be
  // revisited to avoid having two ways to close a page.  Having a single way
  // to close that can run onunload is also useful for fixing
  // http://b/issue?id=753080.
  WebFrame* main_frame = webview()->mainFrame();
  if (main_frame) {
    const GURL& url = main_frame->url();
    // TODO(davemoore) this code should be removed once willClose() gets
    // called when a page is destroyed. DumpLoadHistograms() is safe to call
    // multiple times for the same frame, but it will simplify things.
    if (url.SchemeIs(chrome::kHttpScheme) ||
        url.SchemeIs(chrome::kHttpsScheme))
      DumpLoadHistograms();
  }
  webview()->dispatchUnloadEvent();

  // Just echo back the params in the ACK.
  Send(new ViewHostMsg_ClosePage_ACK(routing_id_, params));
}

void RenderView::OnThemeChanged() {
#if defined(OS_WIN)
  gfx::NativeTheme::instance()->CloseHandles();
  gfx::Rect view_rect(0, 0, size_.width(), size_.height());
  didInvalidateRect(view_rect);
#else  // defined(OS_WIN)
  // TODO(port): we don't support theming on non-Windows platforms yet
  NOTIMPLEMENTED();
#endif
}

void RenderView::OnMessageFromExternalHost(const std::string& message,
                                           const std::string& origin,
                                           const std::string& target) {
  if (message.empty())
    return;

  external_host_bindings_.ForwardMessageFromExternalHost(message, origin,
                                                         target);
}

void RenderView::OnDisassociateFromPopupCount() {
  if (decrement_shared_popup_at_destruction_)
    shared_popup_counter_->data--;
  shared_popup_counter_ = new SharedRenderViewCounter(0);
  decrement_shared_popup_at_destruction_ = false;
}

bool RenderView::MaybeLoadAlternateErrorPage(WebFrame* frame,
                                             const WebURLError& error,
                                             bool replace) {
  // We only show alternate error pages in the main frame.  They are
  // intended to assist the user when navigating, so there is not much
  // value in showing them for failed subframes.  Ideally, we would be
  // able to use the TYPED transition type for this, but that flag is
  // not preserved across page reloads.
  if (frame->parent())
    return false;

  // Use the alternate error page service if this is a DNS failure or
  // connection failure.
  int ec = error.reason;
  if (ec != net::ERR_NAME_NOT_RESOLVED &&
      ec != net::ERR_CONNECTION_FAILED &&
      ec != net::ERR_CONNECTION_REFUSED &&
      ec != net::ERR_ADDRESS_UNREACHABLE &&
      ec != net::ERR_CONNECTION_TIMED_OUT)
    return false;

  const GURL& error_page_url = GetAlternateErrorPageURL(error.unreachableURL,
      ec == net::ERR_NAME_NOT_RESOLVED ? DNS_ERROR : CONNECTION_ERROR);
  if (!error_page_url.is_valid())
    return false;

  // Load an empty page first so there is an immediate response to the error,
  // and then kick off a request for the alternate error page.
  frame->loadHTMLString(std::string(),
                        GURL(kUnreachableWebDataURL),
                        error.unreachableURL,
                        replace);

  // Now, create a fetcher for the error page and associate it with the data
  // source we just created via the LoadHTMLString call.  That way if another
  // navigation occurs, the fetcher will get destroyed.
  NavigationState* navigation_state =
      NavigationState::FromDataSource(frame->provisionalDataSource());
  navigation_state->set_alt_error_page_fetcher(
      new AltErrorPageResourceFetcher(
          error_page_url, frame, error,
          NewCallback(this, &RenderView::AltErrorPageFinished)));
  return true;
}

std::string RenderView::GetAltHTMLForTemplate(
    const DictionaryValue& error_strings, int template_resource_id) const {
  const base::StringPiece template_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          template_resource_id));

  if (template_html.empty()) {
    NOTREACHED() << "unable to load template. ID: " << template_resource_id;
    return "";
  }

  // "t" is the id of the templates root node.
  return jstemplate_builder::GetTemplatesHtml(
      template_html, &error_strings, "t");
}

void RenderView::AltErrorPageFinished(WebFrame* frame,
                                      const WebURLError& original_error,
                                      const std::string& html) {
  // Here, we replace the blank page we loaded previously.

  // If we failed to download the alternate error page, fall back to the
  // original error page if present.  Otherwise, LoadNavigationErrorPage
  // will simply display a default error page.
  const std::string* html_to_load = &html;
  if (html.empty()) {
    NavigationState* navigation_state =
        NavigationState::FromDataSource(frame->dataSource());
    html_to_load = &navigation_state->postponed_data();
  }
  LoadNavigationErrorPage(
      frame, WebURLRequest(), original_error, *html_to_load, true);
}

void RenderView::OnMoveOrResizeStarted() {
  if (webview())
    webview()->hideAutofillPopup();
}

void RenderView::OnResize(const gfx::Size& new_size,
                          const gfx::Rect& resizer_rect) {
  if (webview())
    webview()->hideAutofillPopup();
  RenderWidget::OnResize(new_size, resizer_rect);
}

void RenderView::OnClearFocusedNode() {
  if (webview())
    webview()->clearFocusedNode();
}

void RenderView::OnSetBackground(const SkBitmap& background) {
  if (webview())
    webview()->setIsTransparent(!background.empty());

  SetBackground(background);
}

void RenderView::OnSetActive(bool active) {
  if (webview())
    webview()->setIsActive(active);
}

void RenderView::SendExtensionRequest(const std::string& name,
                                      const ListValue& args,
                                      int request_id,
                                      bool has_callback) {
  Send(new ViewHostMsg_ExtensionRequest(routing_id_, name, args, request_id,
                                        has_callback));
}

void RenderView::OnExtensionResponse(int request_id,
                                     bool success,
                                     const std::string& response,
                                     const std::string& error) {
  ExtensionProcessBindings::HandleResponse(
      request_id, success, response, error);
}

void RenderView::InjectToolstripCSS() {
  if (view_type_ != ViewType::EXTENSION_TOOLSTRIP)
    return;

  static const base::StringPiece toolstrip_css(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_EXTENSION_TOOLSTRIP_CSS));
  std::string css = toolstrip_css.as_string();
  InsertCSS(L"", css, "ToolstripDefaultCSS");
}

void RenderView::OnExtensionMessageInvoke(const std::string& function_name,
                                          const ListValue& args) {
  RendererExtensionBindings::Invoke(function_name, args, this);
}

// Dump all load time histograms.
//
// There are 13 histograms measuring various times.
// The time points we keep are
//    request: time document was requested by user
//    start: time load of document started
//    commit: time load of document started
//    finish_document: main document loaded, before onload()
//    finish: after onload() and all resources are loaded
//    first_paint: first paint performed
//    first_paint_after_load: first paint performed after load is finished
//    begin: request if it was user requested, start otherwise
//
// The times that we histogram are
//    request->start,
//    start->commit,
//    commit->finish_document,
//    finish_document->finish,
//    begin->commit,
//    begin->finishDoc,
//    begin->finish,
//    begin->first_paint,
//    begin->first_paint_after_load
//    commit->finishDoc,
//    commit->first_paint,
//    commit->first_paint_after_load,
//    finish->first_paint_after_load,
//
// It's possible for the request time not to be set, if a client
// redirect had been done (the user never requested the page)
// Also, it's possible to load a page without ever laying it out
// so first_paint and first_paint_after_load can be 0.
void RenderView::DumpLoadHistograms() const {
  WebFrame* main_frame = webview()->mainFrame();
  NavigationState* navigation_state =
      NavigationState::FromDataSource(main_frame->dataSource());
  Time finish = navigation_state->finish_load_time();

  // If we've already dumped or we haven't finished loading, do nothing.
  if (navigation_state->load_histograms_recorded() || finish.is_null())
    return;

  LogNavigationState(navigation_state, main_frame->dataSource());

  Time request = navigation_state->request_time();
  Time start = navigation_state->start_load_time();
  Time commit = navigation_state->commit_load_time();
  Time finish_doc = navigation_state->finish_document_load_time();
  Time first_paint = navigation_state->first_paint_time();
  Time first_paint_after_load =
      navigation_state->first_paint_after_load_time();

  Time begin;
  // Client side redirects will have no request time.
  if (request.is_null()) {
    begin = start;
  } else {
    begin = request;
    UMA_HISTOGRAM_MEDIUM_TIMES("Renderer4.RequestToStart", start - request);
  }
  UMA_HISTOGRAM_MEDIUM_TIMES("Renderer4.StartToCommit", commit - start);
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "Renderer4.CommitToFinishDoc", finish_doc - commit);
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "Renderer4.FinishDocToFinish", finish - finish_doc);

  UMA_HISTOGRAM_MEDIUM_TIMES("Renderer4.BeginToCommit", commit - begin);
  UMA_HISTOGRAM_MEDIUM_TIMES("Renderer4.BeginToFinishDoc", finish_doc - begin);

  static const TimeDelta kBeginToFinishMin(TimeDelta::FromMilliseconds(10));
  static const TimeDelta kBeginToFinishMax(TimeDelta::FromMinutes(10));
  static const size_t kBeginToFinishBucketCount(100);

  UMA_HISTOGRAM_CUSTOM_TIMES("Renderer4.BeginToFinish",
      finish - begin, kBeginToFinishMin,
      kBeginToFinishMax, kBeginToFinishBucketCount);
  UMA_HISTOGRAM_CUSTOM_TIMES("Renderer4.StartToFinish",
      finish - start, kBeginToFinishMin,
      kBeginToFinishMax, kBeginToFinishBucketCount);
  if (!request.is_null())
    UMA_HISTOGRAM_CUSTOM_TIMES("Renderer4.RequestToFinish",
        finish - request, kBeginToFinishMin,
        kBeginToFinishMax, kBeginToFinishBucketCount);

  static bool use_dns_histogram(FieldTrialList::Find("DnsImpact") &&
      !FieldTrialList::Find("DnsImpact")->group_name().empty());
  if (use_dns_histogram) {
    UMA_HISTOGRAM_CUSTOM_TIMES(
        FieldTrial::MakeName("Renderer4.BeginToFinish", "DnsImpact").data(),
        finish - begin, kBeginToFinishMin,
        kBeginToFinishMax, kBeginToFinishBucketCount);
    UMA_HISTOGRAM_CUSTOM_TIMES(
        FieldTrial::MakeName("Renderer4.StartToFinish", "DnsImpact").data(),
        finish - start, kBeginToFinishMin,
        kBeginToFinishMax, kBeginToFinishBucketCount);
    if (!request.is_null())
      UMA_HISTOGRAM_CUSTOM_TIMES(
          FieldTrial::MakeName("Renderer4.RequestToFinish", "DnsImpact").data(),
          finish - request, kBeginToFinishMin,
          kBeginToFinishMax, kBeginToFinishBucketCount);
  }

  static bool use_sdch_histogram(FieldTrialList::Find("GlobalSdch") &&
      !FieldTrialList::Find("GlobalSdch")->group_name().empty());
  if (use_sdch_histogram) {
    UMA_HISTOGRAM_CUSTOM_TIMES(
        FieldTrial::MakeName("Renderer4.BeginToFinish", "GlobalSdch").data(),
        finish - begin, kBeginToFinishMin,
        kBeginToFinishMax, kBeginToFinishBucketCount);
    UMA_HISTOGRAM_CUSTOM_TIMES(
        FieldTrial::MakeName("Renderer4.StartToFinish", "GlobalSdch").data(),
        finish - start, kBeginToFinishMin,
        kBeginToFinishMax, kBeginToFinishBucketCount);
    if (!request.is_null())
      UMA_HISTOGRAM_CUSTOM_TIMES(
          FieldTrial::MakeName("Renderer4.RequestToFinish",
                               "GlobalSdch").data(),
          finish - request, kBeginToFinishMin,
          kBeginToFinishMax, kBeginToFinishBucketCount);
  }

  static bool use_socket_late_binding_histogram =
      FieldTrialList::Find("SocketLateBinding") &&
      !FieldTrialList::Find("SocketLateBinding")->group_name().empty();
  if (use_socket_late_binding_histogram) {
    UMA_HISTOGRAM_CUSTOM_TIMES(
        FieldTrial::MakeName("Renderer4.BeginToFinish",
                             "SocketLateBinding").data(),
        finish - begin, kBeginToFinishMin,
        kBeginToFinishMax, kBeginToFinishBucketCount);
    UMA_HISTOGRAM_CUSTOM_TIMES(
        FieldTrial::MakeName("Renderer4.StartToFinish",
                             "SocketLateBinding").data(),
        finish - start, kBeginToFinishMin,
        kBeginToFinishMax, kBeginToFinishBucketCount);
    if (!request.is_null())
      UMA_HISTOGRAM_CUSTOM_TIMES(
          FieldTrial::MakeName("Renderer4.RequestToFinish",
                               "SocketLateBinding").data(),
          finish - request, kBeginToFinishMin,
          kBeginToFinishMax, kBeginToFinishBucketCount);
  }

  static bool use_cache_histogram1(FieldTrialList::Find("CacheSize") &&
      !FieldTrialList::Find("CacheSize")->group_name().empty());
  if (use_cache_histogram1)
    UMA_HISTOGRAM_CUSTOM_TIMES(
        FieldTrial::MakeName("Renderer4.StartToFinish", "CacheSize").data(),
        finish - start, kBeginToFinishMin,
        kBeginToFinishMax, kBeginToFinishBucketCount);

  static bool use_cache_histogram2(FieldTrialList::Find("NewEviction") &&
      !FieldTrialList::Find("NewEviction")->group_name().empty());
  if (use_cache_histogram2)
    UMA_HISTOGRAM_CUSTOM_TIMES(
        FieldTrial::MakeName("Renderer4.StartToFinish", "NewEviction").data(),
        finish - start, kBeginToFinishMin,
        kBeginToFinishMax, kBeginToFinishBucketCount);

  static bool use_async_tcp(FieldTrialList::Find("AsyncSlowStart") &&
      !FieldTrialList::Find("AsyncSlowStart")->group_name().empty());
  if (use_async_tcp) {
    UMA_HISTOGRAM_CUSTOM_TIMES(
        FieldTrial::MakeName("Renderer4.StartToFinish",
                             "AsyncSlowStart").data(),
        finish - start, kBeginToFinishMin,
        kBeginToFinishMax, kBeginToFinishBucketCount);
  }

  UMA_HISTOGRAM_MEDIUM_TIMES("Renderer4.CommitToFinish", finish - commit);

  if (!first_paint.is_null()) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Renderer4.BeginToFirstPaint", first_paint - begin);
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Renderer4.CommitToFirstPaint", first_paint - commit);
  }

  if (!first_paint_after_load.is_null()) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Renderer4.BeginToFirstPaintAfterLoad", first_paint_after_load - begin);
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Renderer4.CommitToFirstPaintAfterLoad",
        first_paint_after_load - commit);
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Renderer4.FinishToFirstPaintAfterLoad",
        first_paint_after_load - finish);
  }

  navigation_state->set_load_histograms_recorded(true);
}

void RenderView::LogNavigationState(const NavigationState* state,
                                    const WebDataSource* ds) const {
  // Because this function gets called on every page load,
  // take extra care to optimize it away if logging is turned off.
  if (logging::LOG_INFO < logging::GetMinLogLevel())
    return;

  DCHECK(state);
  DCHECK(ds);
  GURL url(ds->request().url());
  Time start = state->start_load_time();
  Time finish = state->finish_load_time();
  // TODO(mbelshe): should we log more stats?
  LOG(INFO) << "PLT: "
            << (finish - start).InMilliseconds()
            << "ms "
            << url.spec();
}

void RenderView::focusAccessibilityObject(
    const WebAccessibilityObject& acc_obj) {
#if defined(OS_WIN)
  // TODO(dglazkov): Current logic implies that focus change can only be made
  // after at least one call to RenderView::OnGetAccessibilityInfo, which is
  // where accessibility is initialized. We should determine whether that's
  // right.
  if (!accessibility_.get())
    return;

  // Retrieve the accessibility object id of the AccessibilityObject.
  int acc_obj_id = accessibility_->addOrGetId(acc_obj);

  // If id is valid, alert the browser side that an accessibility focus change
  // occurred.
  if (acc_obj_id >= 0)
    Send(new ViewHostMsg_AccessibilityFocusChange(routing_id_, acc_obj_id));

#else  // defined(OS_WIN)
  // TODO(port): accessibility not yet implemented
  NOTIMPLEMENTED();
#endif
}

void RenderView::SendPasswordForms(WebFrame* frame) {
  WebVector<WebForm> forms;
  frame->forms(forms);

  std::vector<PasswordForm> password_forms;
  for (size_t i = 0; i < forms.size(); ++i) {
    const WebForm& form = forms[i];

    // Respect autocomplete=off.
    if (form.isAutoCompleteEnabled()) {
      scoped_ptr<PasswordForm> password_form(
          PasswordFormDomManager::CreatePasswordForm(form));
      if (password_form.get())
        password_forms.push_back(*password_form);
    }
  }

  if (!password_forms.empty())
    Send(new ViewHostMsg_PasswordFormsSeen(routing_id_, password_forms));
}

void RenderView::Print(WebFrame* frame, bool script_initiated) {
  DCHECK(frame);
  if (print_helper_.get() == NULL) {
    print_helper_.reset(new PrintWebViewHelper(this));
  }
  print_helper_->Print(frame, script_initiated);
}

void RenderView::OnSetEditCommandsForNextKeyEvent(
    const EditCommands& edit_commands) {
  edit_commands_ = edit_commands;
}

void RenderView::OnExecuteCode(int request_id, const std::string& extension_id,
                               bool is_js_code,
                               const std::string& code_string) {
  WebFrame* main_frame = webview() ? webview()->mainFrame() : NULL;
  if (!main_frame) {
    Send(new ViewMsg_ExecuteCodeFinished(routing_id_, request_id, false));
    return;
  }

  WebDataSource* ds = main_frame->dataSource();
  NavigationState* navigation_state = NavigationState::FromDataSource(ds);
  if (!navigation_state->user_script_idle_scheduler()->has_run()) {
    scoped_refptr<CodeExecutionInfo> info = new CodeExecutionInfo(
        request_id, extension_id, is_js_code, code_string);
    pending_code_execution_queue_.push(info);
    return;
  }

  ExecuteCodeImpl(main_frame, request_id, extension_id, is_js_code,
                  code_string);
}

void RenderView::ExecuteCodeImpl(WebFrame* frame,
                                 int request_id,
                                 const std::string& extension_id,
                                 bool is_js_code,
                                 const std::string& code_string) {
  if (is_js_code) {
    std::vector<WebScriptSource> sources;
    sources.push_back(
        WebScriptSource(WebString::fromUTF8(code_string)));
    UserScriptSlave::InsertInitExtensionCode(&sources, extension_id);
    frame->executeScriptInIsolatedWorld(
        UserScriptSlave::GetIsolatedWorldId(extension_id),
        &sources.front(), sources.size(), EXTENSION_GROUP_CONTENT_SCRIPTS);
  } else {
    frame->insertStyleText(WebString::fromUTF8(code_string), WebString());
  }

  Send(new ViewMsg_ExecuteCodeFinished(routing_id_, request_id, true));
}

void RenderView::Close() {
  // We need to grab a pointer to the doomed WebView before we destroy it.
  WebView* doomed = webview();
  RenderWidget::Close();
  Singleton<ViewMap>::get()->erase(doomed);
}

void RenderView::DidHandleKeyEvent() {
  edit_commands_.clear();
}

void RenderView::EnsureDocumentTag() {
  // TODO(darin): There's actually no reason for this to be here.  We should
  // have the browser side manage the document tag.
#if defined(OS_MACOSX)
  if (!has_document_tag_) {
    // Make the call to get the tag.
    Send(new ViewHostMsg_GetDocumentTag(routing_id_, &document_tag_));
    has_document_tag_ = true;
  }
#endif
}

bool RenderView::SendAndRunNestedMessageLoop(IPC::SyncMessage* message) {
  PluginChannelHost::Broadcast(
      new PluginMsg_SignalModalDialogEvent(host_window_));

  message->EnableMessagePumping();  // Runs a nested message loop.
  bool rv = Send(message);

  PluginChannelHost::Broadcast(
      new PluginMsg_ResetModalDialogEvent(host_window_));

  return rv;
}
