// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <set>
#include <string>
#include <vector>

#include "build/build_config.h"

#include "base/file_path.h"
#include "base/nullable_string16.h"
#include "base/platform_file.h"
#include "base/process.h"
#include "base/shared_memory.h"
#include "base/sync_socket.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/geoposition.h"
#include "chrome/common/instant_types.h"
#include "chrome/common/nacl_types.h"
#include "chrome/common/page_zoom.h"
#include "chrome/common/translate_errors.h"
#include "chrome/common/window_container_type.h"
#include "content/common/notification_type.h"
#include "ipc/ipc_message_macros.h"
#include "media/audio/audio_buffers_state.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFindOptions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaPlayerAction.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#include "webkit/glue/context_menu.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/plugins/npapi/webplugininfo.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

#if defined(OS_MACOSX)
#include "chrome/common/font_descriptor_mac.h"
#endif

// TODO(mpcomplete): rename ViewMsg and ViewHostMsg to something that makes
// more sense with our current design.

// IPC_MESSAGE macros choke on extra , in the std::map, when expanding. We need
// to typedef it to avoid that.
// Substitution map for l10n messages.
typedef std::map<std::string, std::string> SubstitutionMap;

class Value;
class SkBitmap;
class WebCursor;
struct ThumbnailScore;

namespace gfx {
class Rect;
}

namespace IPC {
struct ChannelHandle;
class Message;
}

namespace webkit_blob {
class BlobData;
}

namespace file_util {
struct FileInfo;
}

#define IPC_MESSAGE_START ViewMsgStart

//-----------------------------------------------------------------------------
// RenderView messages
// These are messages sent from the browser to the renderer process.

// Used typically when recovering from a crash.  The new rendering process
// sets its global "next page id" counter to the given value.
IPC_MESSAGE_CONTROL1(ViewMsg_SetNextPageID,
                     int32 /* next_page_id */)

// Sends System Colors corresponding to a set of CSS color keywords
// down the pipe.
// This message must be sent to the renderer immediately on launch
// before creating any new views.
// The message can also be sent during a renderer's lifetime if system colors
// are updated.
// TODO(jeremy): Possibly change IPC format once we have this all hooked up.
IPC_MESSAGE_ROUTED1(ViewMsg_SetCSSColors,
                    std::vector<CSSColors::CSSColorMapping>)

// Asks the browser for a unique routing ID.
IPC_SYNC_MESSAGE_CONTROL0_1(ViewHostMsg_GenerateRoutingID,
                            int /* routing_id */)

// Tells the renderer to create a new view.
// This message is slightly different, the view it takes (via
// ViewMsg_New_Params) is the view to create, the message itself is sent as a
// non-view control message.
IPC_MESSAGE_CONTROL1(ViewMsg_New,
                     ViewMsg_New_Params)

// Tells the renderer to set its maximum cache size to the supplied value.
IPC_MESSAGE_CONTROL3(ViewMsg_SetCacheCapacities,
                     size_t /* min_dead_capacity */,
                     size_t /* max_dead_capacity */,
                     size_t /* capacity */)

// Tells the renderer to clear the cache.
IPC_MESSAGE_CONTROL0(ViewMsg_ClearCache)

// Reply in response to ViewHostMsg_ShowView or ViewHostMsg_ShowWidget.
// similar to the new command, but used when the renderer created a view
// first, and we need to update it.
IPC_MESSAGE_ROUTED1(ViewMsg_CreatingNew_ACK,
                    gfx::NativeViewId /* parent_hwnd */)

// Sends updated preferences to the renderer.
IPC_MESSAGE_ROUTED1(ViewMsg_SetRendererPrefs,
                    RendererPreferences)

// Tells the renderer to perform the given action on the media player
// located at the given point.
IPC_MESSAGE_ROUTED2(ViewMsg_MediaPlayerActionAt,
                    gfx::Point, /* location */
                    WebKit::WebMediaPlayerAction)

IPC_MESSAGE_ROUTED0(ViewMsg_PrintNodeUnderContextMenu)

// Tells the renderer to print the print preview tab's PDF plugin without
// showing the print dialog.
IPC_MESSAGE_ROUTED1(ViewMsg_PrintForPrintPreview,
                    DictionaryValue /* settings*/)

// Tells the render view to close.
IPC_MESSAGE_ROUTED0(ViewMsg_Close)

// Tells the render view to change its size.  A ViewHostMsg_PaintRect message
// is generated in response provided new_size is not empty and not equal to
// the view's current size.  The generated ViewHostMsg_PaintRect message will
// have the IS_RESIZE_ACK flag set. It also receives the resizer rect so that
// we don't have to fetch it every time WebKit asks for it.
IPC_MESSAGE_ROUTED2(ViewMsg_Resize,
                    gfx::Size /* new_size */,
                    gfx::Rect /* resizer_rect */)

// Sent to inform the view that it was hidden.  This allows it to reduce its
// resource utilization.
IPC_MESSAGE_ROUTED0(ViewMsg_WasHidden)

// Tells the render view that it is no longer hidden (see WasHidden), and the
// render view is expected to respond with a full repaint if needs_repainting
// is true.  In that case, the generated ViewHostMsg_PaintRect message will
// have the IS_RESTORE_ACK flag set.  If needs_repainting is false, then this
// message does not trigger a message in response.
IPC_MESSAGE_ROUTED1(ViewMsg_WasRestored,
                    bool /* needs_repainting */)

// Tells the render view to capture a thumbnail image of the page. The
// render view responds with a ViewHostMsg_Thumbnail.
IPC_MESSAGE_ROUTED0(ViewMsg_CaptureThumbnail)

// Tells the render view to capture a thumbnail image of the page. The
// render view responds with a ViewHostMsg_Snapshot.
IPC_MESSAGE_ROUTED0(ViewMsg_CaptureSnapshot)

// Tells the render view to switch the CSS to print media type, renders every
// requested pages and switch back the CSS to display media type.
IPC_MESSAGE_ROUTED0(ViewMsg_PrintPages)

// Tells the render view that printing is done so it can clean up.
IPC_MESSAGE_ROUTED2(ViewMsg_PrintingDone,
                    int /* document_cookie */,
                    bool /* success */)

// Tells the render view to switch the CSS to print media type, renders every
// requested pages for print preview.
IPC_MESSAGE_ROUTED0(ViewMsg_PrintPreview)

// Sends back to the browser the rendered "printed document" for preview that
// was requested by a ViewMsg_PrintPreview message. The memory handle in this
// message is already valid in the browser process.
IPC_MESSAGE_ROUTED1(ViewHostMsg_PagesReadyForPreview,
                    ViewHostMsg_DidPreviewDocument_Params /* params */)

// Tells the renderer to dump as much memory as it can, perhaps because we
// have memory pressure or the renderer is (or will be) paged out.  This
// should only result in purging objects we can recalculate, e.g. caches or
// JS garbage, not in purging irreplaceable objects.
IPC_MESSAGE_CONTROL0(ViewMsg_PurgeMemory)

// Sent to render the view into the supplied transport DIB, resize
// the web widget to match the |page_size|, scale it by the
// appropriate scale to make it fit the |desired_size|, and return
// it.  In response to this message, the host generates a
// ViewHostMsg_PaintAtSize_ACK message.  Note that the DIB *must* be
// the right size to receive an RGBA image at the |desired_size|.
// |tag| is sent along with ViewHostMsg_PaintAtSize_ACK unmodified to
// identify the PaintAtSize message the ACK belongs to.
IPC_MESSAGE_ROUTED4(ViewMsg_PaintAtSize,
                    TransportDIB::Handle /* dib_handle */,
                    int /* tag */,
                    gfx::Size /* page_size */,
                    gfx::Size /* desired_size */)

// Tells the render view that a ViewHostMsg_UpdateRect message was processed.
// This signals the render view that it can send another UpdateRect message.
IPC_MESSAGE_ROUTED0(ViewMsg_UpdateRect_ACK)

// Message payload includes:
// 1. A blob that should be cast to WebInputEvent
// 2. An optional boolean value indicating if a RawKeyDown event is associated
//    to a keyboard shortcut of the browser.
IPC_MESSAGE_ROUTED0(ViewMsg_HandleInputEvent)

// This message notifies the renderer that the next key event is bound to one
// or more pre-defined edit commands. If the next key event is not handled
// by webkit, the specified edit commands shall be executed against current
// focused frame.
// Parameters
// * edit_commands (see chrome/common/edit_command_types.h)
//   Contains one or more edit commands.
// See third_party/WebKit/Source/WebCore/editing/EditorCommand.cpp for detailed
// definition of webkit edit commands.
//
// This message must be sent just before sending a key event.
IPC_MESSAGE_ROUTED1(ViewMsg_SetEditCommandsForNextKeyEvent,
                    std::vector<EditCommand> /* edit_commands */)

// Message payload is the name/value of a WebCore edit command to execute.
IPC_MESSAGE_ROUTED2(ViewMsg_ExecuteEditCommand,
                    std::string, /* name */
                    std::string /* value */)

IPC_MESSAGE_ROUTED0(ViewMsg_MouseCaptureLost)

// TODO(darin): figure out how this meshes with RestoreFocus
IPC_MESSAGE_ROUTED1(ViewMsg_SetFocus, bool /* enable */)

// Tells the renderer to focus the first (last if reverse is true) focusable
// node.
IPC_MESSAGE_ROUTED1(ViewMsg_SetInitialFocus, bool /* reverse */)

// Tells the renderer to scroll the currently focused node into view only if
// the currently focused node is a Text node (textfield, text area or content
// editable divs).
IPC_MESSAGE_ROUTED0(ViewMsg_ScrollFocusedEditableNodeIntoView)

// Tells the renderer to perform the specified navigation, interrupting any
// existing navigation.
IPC_MESSAGE_ROUTED1(ViewMsg_Navigate, ViewMsg_Navigate_Params)

IPC_MESSAGE_ROUTED0(ViewMsg_Stop)

// Tells the renderer to reload the current focused frame
IPC_MESSAGE_ROUTED0(ViewMsg_ReloadFrame)

// This message notifies the renderer that the user has closed the FindInPage
// window (and what action to take regarding the selection).
IPC_MESSAGE_ROUTED1(ViewMsg_StopFinding,
                    ViewMsg_StopFinding_Params /* action */)

// These messages are typically generated from context menus and request the
// renderer to apply the specified operation to the current selection.
IPC_MESSAGE_ROUTED0(ViewMsg_Undo)
IPC_MESSAGE_ROUTED0(ViewMsg_Redo)
IPC_MESSAGE_ROUTED0(ViewMsg_Cut)
IPC_MESSAGE_ROUTED0(ViewMsg_Copy)
#if defined(OS_MACOSX)
IPC_MESSAGE_ROUTED0(ViewMsg_CopyToFindPboard)
#endif
IPC_MESSAGE_ROUTED0(ViewMsg_Paste)
// Replaces the selected region or a word around the cursor with the
// specified string.
IPC_MESSAGE_ROUTED1(ViewMsg_Replace, string16)
IPC_MESSAGE_ROUTED0(ViewMsg_ToggleSpellCheck)
IPC_MESSAGE_ROUTED0(ViewMsg_Delete)
IPC_MESSAGE_ROUTED0(ViewMsg_SelectAll)
IPC_MESSAGE_ROUTED1(ViewMsg_ToggleSpellPanel, bool)
IPC_MESSAGE_ROUTED3(ViewMsg_SpellChecker_RespondTextCheck,
                    int        /* request identifier given by WebKit */,
                    int        /* document tag */,
                    std::vector<WebKit::WebTextCheckingResult>)

// This message tells the renderer to advance to the next misspelling. It is
// sent when the user clicks the "Find Next" button on the spelling panel.
IPC_MESSAGE_ROUTED0(ViewMsg_AdvanceToNextMisspelling)

// Copies the image at location x, y to the clipboard (if there indeed is an
// image at that location).
IPC_MESSAGE_ROUTED2(ViewMsg_CopyImageAt,
                    int /* x */,
                    int /* y */)

// History system notification that the visited link database has been
// replaced. It has one SharedMemoryHandle argument consisting of the table
// handle. This handle is valid in the context of the renderer
IPC_MESSAGE_CONTROL1(ViewMsg_VisitedLink_NewTable, base::SharedMemoryHandle)

// History system notification that a link has been added and the link
// coloring state for the given hash must be re-calculated.
IPC_MESSAGE_CONTROL1(ViewMsg_VisitedLink_Add, std::vector<uint64>)

// History system notification that one or more history items have been
// deleted, which at this point means that all link coloring state must be
// re-calculated.
IPC_MESSAGE_CONTROL0(ViewMsg_VisitedLink_Reset)

// Notification that the user scripts have been updated. It has one
// SharedMemoryHandle argument consisting of the pickled script data. This
// handle is valid in the context of the renderer.
IPC_MESSAGE_CONTROL1(ViewMsg_UserScripts_UpdatedScripts,
                     base::SharedMemoryHandle)

// Sent when the user wants to search for a word on the page (find in page).
IPC_MESSAGE_ROUTED3(ViewMsg_Find,
                    int /* request_id */,
                    string16 /* search_text */,
                    WebKit::WebFindOptions)

// Send from the renderer to the browser to return the script running result.
IPC_MESSAGE_ROUTED2(ViewMsg_ExecuteCodeFinished,
                    int, /* request id */
                    bool /* whether the script ran successfully */)

// Sent when user prompting is required before a ViewHostMsg_GetCookies
// message can complete.  This message indicates that the renderer should
// pump messages while waiting for cookies.
IPC_MESSAGE_CONTROL0(ViewMsg_SignalCookiePromptEvent)

// Request for the renderer to evaluate an xpath to a frame and execute a
// javascript: url in that frame's context. The message is completely
// asynchronous and no corresponding response message is sent back.
//
// frame_xpath contains the modified xpath notation to identify an inner
// subframe (starting from the root frame). It is a concatenation of
// number of smaller xpaths delimited by '\n'. Each chunk in the string can
// be evaluated to a frame in its parent-frame's context.
//
// Example: /html/body/iframe/\n/html/body/div/iframe/\n/frameset/frame[0]
// can be broken into 3 xpaths
// /html/body/iframe evaluates to an iframe within the root frame
// /html/body/div/iframe evaluates to an iframe within the level-1 iframe
// /frameset/frame[0] evaluates to first frame within the level-2 iframe
//
// jscript_url is the string containing the javascript: url to be executed
// in the target frame's context. The string should start with "javascript:"
// and continue with a valid JS text.
//
// If the fourth parameter is true the result is sent back to the renderer
// using the message ViewHostMsg_ScriptEvalResponse.
// ViewHostMsg_ScriptEvalResponse is passed the ID parameter so that the
// client can uniquely identify the request.
IPC_MESSAGE_ROUTED4(ViewMsg_ScriptEvalRequest,
                    string16,  /* frame_xpath */
                    string16,  /* jscript_url */
                    int,  /* ID */
                    bool  /* If true, result is sent back. */)

// Request for the renderer to evaluate an xpath to a frame and insert css
// into that frame's document. See ViewMsg_ScriptEvalRequest for details on
// allowed xpath expressions.
IPC_MESSAGE_ROUTED3(ViewMsg_CSSInsertRequest,
                    std::wstring,  /* frame_xpath */
                    std::string,  /* css string */
                    std::string  /* element id */)

// Log a message to the console of the target frame
IPC_MESSAGE_ROUTED3(ViewMsg_AddMessageToConsole,
                    string16 /* frame_xpath */,
                    string16 /* message */,
                    WebKit::WebConsoleMessage::Level /* message_level */)

// RenderViewHostDelegate::RenderViewCreated method sends this message to a
// new renderer to notify it that it will host developer tools UI and should
// set up all neccessary bindings and create DevToolsClient instance that
// will handle communication with inspected page DevToolsAgent.
IPC_MESSAGE_ROUTED0(ViewMsg_SetupDevToolsClient)

// Change the zoom level for the current main frame.  If the level actually
// changes, a ViewHostMsg_DidZoomURL message will be sent back to the browser
// telling it what url got zoomed and what its current zoom level is.
IPC_MESSAGE_ROUTED1(ViewMsg_Zoom,
                    PageZoom::Function /* function */)

// Set the zoom level for the current main frame.  If the level actually
// changes, a ViewHostMsg_DidZoomURL message will be sent back to the browser
// telling it what url got zoomed and what its current zoom level is.
IPC_MESSAGE_ROUTED1(ViewMsg_SetZoomLevel,
                    double /* zoom_level */)

// Set the zoom level for a particular url that the renderer is in the
// process of loading.  This will be stored, to be used if the load commits
// and ignored otherwise.
IPC_MESSAGE_ROUTED2(ViewMsg_SetZoomLevelForLoadingURL,
                    GURL /* url */,
                    double /* zoom_level */)

// Set the zoom level for a particular url, so all render views
// displaying this url can update their zoom levels to match.
IPC_MESSAGE_CONTROL2(ViewMsg_SetZoomLevelForCurrentURL,
                     GURL /* url */,
                     double /* zoom_level */)

// Set the content settings for a particular url that the renderer is in the
// process of loading.  This will be stored, to be used if the load commits
// and ignored otherwise.
IPC_MESSAGE_ROUTED2(ViewMsg_SetContentSettingsForLoadingURL,
                    GURL /* url */,
                    ContentSettings /* content_settings */)

// Set the content settings for a particular url, so all render views
// displaying this host url update their content settings to match.
IPC_MESSAGE_CONTROL2(ViewMsg_SetContentSettingsForCurrentURL,
                     GURL /* url */,
                     ContentSettings /* content_settings */)

// Change encoding of page in the renderer.
IPC_MESSAGE_ROUTED1(ViewMsg_SetPageEncoding,
                    std::string /*new encoding name*/)

// Reset encoding of page in the renderer back to default.
IPC_MESSAGE_ROUTED0(ViewMsg_ResetPageEncodingToDefault)

// Requests the renderer to reserve a range of page ids.
IPC_MESSAGE_ROUTED1(ViewMsg_ReservePageIDRange,
                    int  /* size_of_range */)

// D&d drop target messages.
IPC_MESSAGE_ROUTED4(ViewMsg_DragTargetDragEnter,
                    WebDropData /* drop_data */,
                    gfx::Point /* client_pt */,
                    gfx::Point /* screen_pt */,
                    WebKit::WebDragOperationsMask /* ops_allowed */)
IPC_MESSAGE_ROUTED3(ViewMsg_DragTargetDragOver,
                    gfx::Point /* client_pt */,
                    gfx::Point /* screen_pt */,
                    WebKit::WebDragOperationsMask /* ops_allowed */)
IPC_MESSAGE_ROUTED0(ViewMsg_DragTargetDragLeave)
IPC_MESSAGE_ROUTED2(ViewMsg_DragTargetDrop,
                    gfx::Point /* client_pt */,
                    gfx::Point /* screen_pt */)

// Notifies the renderer of updates in mouse position of an in-progress
// drag.  if |ended| is true, then the user has ended the drag operation.
IPC_MESSAGE_ROUTED4(ViewMsg_DragSourceEndedOrMoved,
                    gfx::Point /* client_pt */,
                    gfx::Point /* screen_pt */,
                    bool /* ended */,
                    WebKit::WebDragOperation /* drag_operation */)

// Notifies the renderer that the system DoDragDrop call has ended.
IPC_MESSAGE_ROUTED0(ViewMsg_DragSourceSystemDragEnded)

// Used to tell a render view whether it should expose various bindings
// that allow JS content extended privileges.  See BindingsPolicy for valid
// flag values.
IPC_MESSAGE_ROUTED1(ViewMsg_AllowBindings,
                    int /* enabled_bindings_flags */)

// Tell the renderer to add a property to the WebUI binding object.  This
// only works if we allowed WebUI bindings.
IPC_MESSAGE_ROUTED2(ViewMsg_SetWebUIProperty,
                    std::string /* property_name */,
                    std::string /* property_value_json */)

// This message starts/stop monitoring the input method status of the focused
// edit control of a renderer process.
// Parameters
// * is_active (bool)
//   Indicates if an input method is active in the browser process.
//   The possible actions when a renderer process receives this message are
//   listed below:
//     Value Action
//     true  Start sending IPC message ViewHostMsg_ImeUpdateTextInputState
//           to notify the input method status of the focused edit control.
//     false Stop sending IPC message ViewHostMsg_ImeUpdateTextInputState.
IPC_MESSAGE_ROUTED1(ViewMsg_SetInputMethodActive,
                    bool /* is_active */)

// This message sends a string being composed with an input method.
IPC_MESSAGE_ROUTED4(
    ViewMsg_ImeSetComposition,
    string16, /* text */
    std::vector<WebKit::WebCompositionUnderline>, /* underlines */
    int, /* selectiont_start */
    int /* selection_end */)

// This message confirms an ongoing composition.
IPC_MESSAGE_ROUTED1(ViewMsg_ImeConfirmComposition,
                    string16 /* text */)

// This passes a set of webkit preferences down to the renderer.
IPC_MESSAGE_ROUTED1(ViewMsg_UpdateWebPreferences, WebPreferences)

// Used to notify the render-view that the browser has received a reply for
// the Find operation and is interested in receiving the next one. This is
// used to prevent the renderer from spamming the browser process with
// results.
IPC_MESSAGE_ROUTED0(ViewMsg_FindReplyACK)

// Used to notify the render-view that we have received a target URL. Used
// to prevent target URLs spamming the browser.
IPC_MESSAGE_ROUTED0(ViewMsg_UpdateTargetURL_ACK)

// Sets the alternate error page URL (link doctor) for the renderer process.
IPC_MESSAGE_ROUTED1(ViewMsg_SetAltErrorPageURL, GURL)

// Install the first missing pluign.
IPC_MESSAGE_ROUTED0(ViewMsg_InstallMissingPlugin)

// Tells the renderer to empty its plugin list cache, optional reloading
// pages containing plugins.
IPC_MESSAGE_CONTROL1(ViewMsg_PurgePluginListCache,
                     bool /* reload_pages */)

// Tells the render view to load all blocked plugins.
IPC_MESSAGE_ROUTED0(ViewMsg_LoadBlockedPlugins)

// Tells the render view a prerendered page is about to be displayed.
IPC_MESSAGE_ROUTED0(ViewMsg_DisplayPrerenderedPage)

IPC_MESSAGE_ROUTED1(ViewMsg_RunFileChooserResponse,
                    std::vector<FilePath> /* selected files */)

// Used to instruct the RenderView to go into "view source" mode.
IPC_MESSAGE_ROUTED0(ViewMsg_EnableViewSourceMode)

// Get all savable resource links from current webpage, include main
// frame and sub-frame.
IPC_MESSAGE_ROUTED1(ViewMsg_GetAllSavableResourceLinksForCurrentPage,
                    GURL /* url of page which is needed to save */)

// Get html data by serializing all frames of current page with lists
// which contain all resource links that have local copy.
IPC_MESSAGE_ROUTED3(ViewMsg_GetSerializedHtmlDataForCurrentPageWithLocalLinks,
                    std::vector<GURL> /* urls that have local copy */,
                    std::vector<FilePath> /* paths of local copy */,
                    FilePath /* local directory path */)

// Requests application info for the page. The renderer responds back with
// ViewHostMsg_DidGetApplicationInfo.
IPC_MESSAGE_ROUTED1(ViewMsg_GetApplicationInfo, int32 /*page_id*/)

// Requests the renderer to download the specified favicon image encode it as
// PNG and send the PNG data back ala ViewHostMsg_DidDownloadFavIcon.
IPC_MESSAGE_ROUTED3(ViewMsg_DownloadFavIcon,
                    int /* identifier for the request */,
                    GURL /* URL of the image */,
                    int /* Size of the image. Normally 0, but set if you have
                           a preferred image size to request, such as when
                           downloading the favicon */)

// When a renderer sends a ViewHostMsg_Focus to the browser process,
// the browser has the option of sending a ViewMsg_CantFocus back to
// the renderer.
IPC_MESSAGE_ROUTED0(ViewMsg_CantFocus)

// Instructs the renderer to invoke the frame's shouldClose method, which
// runs the onbeforeunload event handler.  Expects the result to be returned
// via ViewHostMsg_ShouldClose.
IPC_MESSAGE_ROUTED0(ViewMsg_ShouldClose)

// Instructs the renderer to close the current page, including running the
// onunload event handler. See the struct in render_messages.h for more.
//
// Expects a ClosePage_ACK message when finished, where the parameters are
// echoed back.
IPC_MESSAGE_ROUTED1(ViewMsg_ClosePage,
                    ViewMsg_ClosePage_Params)

// Asks the renderer to send back stats on the WebCore cache broken down by
// resource types.
IPC_MESSAGE_CONTROL0(ViewMsg_GetCacheResourceStats)

// Asks the renderer to send back Histograms.
IPC_MESSAGE_CONTROL1(ViewMsg_GetRendererHistograms,
                     int /* sequence number of Renderer Histograms. */)

#if defined(USE_TCMALLOC)
// Asks the renderer to send back tcmalloc stats.
IPC_MESSAGE_CONTROL0(ViewMsg_GetRendererTcmalloc)
#endif

// Asks the renderer to send back V8 heap stats.
IPC_MESSAGE_CONTROL0(ViewMsg_GetV8HeapStats)

// Notifies the renderer about ui theme changes
IPC_MESSAGE_ROUTED0(ViewMsg_ThemeChanged)

// Notifies the renderer that a paint is to be generated for the rectangle
// passed in.
IPC_MESSAGE_ROUTED1(ViewMsg_Repaint,
                    gfx::Size /* The view size to be repainted */)

// Posts a message to the renderer.
IPC_MESSAGE_ROUTED3(ViewMsg_HandleMessageFromExternalHost,
                    std::string /* The message */,
                    std::string /* The origin */,
                    std::string /* The target*/)

// Sent to the renderer when a popup window should no longer count against
// the current popup count (either because it's not a popup or because it was
// a generated by a user action or because a constrained popup got turned
// into a full window).
IPC_MESSAGE_ROUTED0(ViewMsg_DisassociateFromPopupCount)

// Notifies the renderer of the appcache that has been selected for a
// a particular host. This is sent in reply to AppCacheMsg_SelectCache.
IPC_MESSAGE_CONTROL2(AppCacheMsg_CacheSelected,
                     int /* host_id */,
                     appcache::AppCacheInfo)

// Notifies the renderer of an AppCache status change.
IPC_MESSAGE_CONTROL2(AppCacheMsg_StatusChanged,
                     std::vector<int> /* host_ids */,
                     appcache::Status)

// Notifies the renderer of an AppCache event other than the
// progress event which has a seperate message.
IPC_MESSAGE_CONTROL2(AppCacheMsg_EventRaised,
                     std::vector<int> /* host_ids */,
                     appcache::EventID)

// Notifies the renderer of an AppCache progress event.
IPC_MESSAGE_CONTROL4(AppCacheMsg_ProgressEventRaised,
                     std::vector<int> /* host_ids */,
                     GURL /* url being processed */,
                     int /* total */,
                     int /* complete */)

// Notifies the renderer of an AppCache error event.
IPC_MESSAGE_CONTROL2(AppCacheMsg_ErrorEventRaised,
                     std::vector<int> /* host_ids */,
                     std::string /* error_message */)

// Notifies the renderer of an AppCache logging message.
IPC_MESSAGE_CONTROL3(AppCacheMsg_LogMessage,
                     int /* host_id */,
                     int /* log_level */,
                     std::string /* message */)

// Notifies the renderer of the fact that AppCache access was blocked.
IPC_MESSAGE_CONTROL2(AppCacheMsg_ContentBlocked,
                     int /* host_id */,
                     GURL /* manifest_url */)

// Sent by the Browser process to alert a window about whether a it should
// allow a scripted window.close(). The renderer assumes every new window is a
// blocked popup until notified otherwise.
IPC_MESSAGE_ROUTED1(ViewMsg_AllowScriptToClose,
                    bool /* script_can_close */)

// Sent by AudioRendererHost to renderer to request an audio packet.
IPC_MESSAGE_ROUTED2(ViewMsg_RequestAudioPacket,
                    int /* stream id */,
                    AudioBuffersState)

// Tell the renderer process that the audio stream has been created, renderer
// process would be given a ShareMemoryHandle that it should write to from
// then on.
IPC_MESSAGE_ROUTED3(ViewMsg_NotifyAudioStreamCreated,
                    int /* stream id */,
                    base::SharedMemoryHandle /* handle */,
                    uint32 /* length */)

// Tell the renderer process that a low latency audio stream has been created,
// renderer process would be given a SyncSocket that it should write to from
// then on.
#if defined(OS_WIN)
IPC_MESSAGE_ROUTED4(ViewMsg_NotifyLowLatencyAudioStreamCreated,
                    int /* stream id */,
                    base::SharedMemoryHandle /* handle */,
                    base::SyncSocket::Handle /* socket handle */,
                    uint32 /* length */)
#else
IPC_MESSAGE_ROUTED4(ViewMsg_NotifyLowLatencyAudioStreamCreated,
                    int /* stream id */,
                    base::SharedMemoryHandle /* handle */,
                    base::FileDescriptor /* socket handle */,
                    uint32 /* length */)
#endif

// Notification message sent from AudioRendererHost to renderer for state
// update after the renderer has requested a Create/Start/Close.
IPC_MESSAGE_ROUTED2(ViewMsg_NotifyAudioStreamStateChanged,
                    int /* stream id */,
                    ViewMsg_AudioStreamState_Params /* new state */)

IPC_MESSAGE_ROUTED2(ViewMsg_NotifyAudioStreamVolume,
                    int /* stream id */,
                    double /* volume */)

// Notification that a move or resize renderer's containing window has
// started.
IPC_MESSAGE_ROUTED0(ViewMsg_MoveOrResizeStarted)

// The browser sends this message in response to all extension api calls.
IPC_MESSAGE_ROUTED4(ViewMsg_ExtensionResponse,
                    int /* request_id */,
                    bool /* success */,
                    std::string /* response */,
                    std::string /* error */)

// This message is optionally routed.  If used as a control message, it
// will call a javascript function in every registered context in the
// target process.  If routed, it will be restricted to the contexts that
// are part of the target RenderView.
// If |extension_id| is non-empty, the function will be invoked only in
// contexts owned by the extension. |args| is a list of primitive Value types
// that are passed to the function.
IPC_MESSAGE_ROUTED4(ViewMsg_ExtensionMessageInvoke,
                    std::string /* extension_id */,
                    std::string /* function_name */,
                    ListValue /* args */,
                    GURL /* event URL */)

// Tell the renderer process all known extension function names.
IPC_MESSAGE_CONTROL1(ViewMsg_Extension_SetFunctionNames,
                     std::vector<std::string>)

// TODO(aa): SetAPIPermissions, SetHostPermissions, and possibly
// UpdatePageActions should be replaced with just sending additional data in
// ExtensionLoaded. See: crbug.com/70516.

// Tell the renderer process which permissions the given extension has. See
// Extension::Permissions for which elements correspond to which permissions.
IPC_MESSAGE_CONTROL2(ViewMsg_Extension_SetAPIPermissions,
                     std::string /* extension_id */,
                     std::set<std::string> /* permissions */)

// Tell the renderer process which host permissions the given extension has.
IPC_MESSAGE_CONTROL2(
    ViewMsg_Extension_SetHostPermissions,
    GURL /* source extension's origin */,
    std::vector<URLPattern> /* URLPatterns the extension can access */)

// Tell the renderer process all known page action ids for a particular
// extension.
IPC_MESSAGE_CONTROL2(ViewMsg_Extension_UpdatePageActions,
                     std::string /* extension_id */,
                     std::vector<std::string> /* page_action_ids */)

// Notifies the renderer that an extension was loaded in the browser.
IPC_MESSAGE_CONTROL1(ViewMsg_ExtensionLoaded, ViewMsg_ExtensionLoaded_Params);

// Notifies the renderer that an extension was unloaded in the browser.
IPC_MESSAGE_CONTROL1(ViewMsg_ExtensionUnloaded, std::string);

// Updates the scripting whitelist for extensions in the render process. This is
// only used for testing.
IPC_MESSAGE_CONTROL1(ViewMsg_Extension_SetScriptingWhitelist,
                     Extension::ScriptingWhitelist /* extenison ids */);

// Changes the text direction of the currently selected input field (if any).
IPC_MESSAGE_ROUTED1(ViewMsg_SetTextDirection,
                    WebKit::WebTextDirection /* direction */)

// Tells the renderer to clear the focused node (if any).
IPC_MESSAGE_ROUTED0(ViewMsg_ClearFocusedNode)

// Make the RenderView transparent and render it onto a custom background. The
// background will be tiled in both directions if it is not large enough.
IPC_MESSAGE_ROUTED1(ViewMsg_SetBackground,
                    SkBitmap /* background */)

// Reply to ViewHostMsg_RequestMove, ViewHostMsg_ShowView, and
// ViewHostMsg_ShowWidget to inform the renderer that the browser has
// processed the move.  The browser may have ignored the move, but it finished
// processing.  This is used because the renderer keeps a temporary cache of
// the widget position while these asynchronous operations are in progress.
IPC_MESSAGE_ROUTED0(ViewMsg_Move_ACK)

// Used to instruct the RenderView to send back updates to the preferred size.
IPC_MESSAGE_ROUTED1(ViewMsg_EnablePreferredSizeChangedMode, int /*flags*/)

IPC_MESSAGE_ROUTED4(ViewMsg_SearchBoxChange,
                    string16 /* value */,
                    bool /* verbatim */,
                    int /* selection_start */,
                    int /* selection_end */)
IPC_MESSAGE_ROUTED2(ViewMsg_SearchBoxSubmit,
                    string16 /* value */,
                    bool /* verbatim */)
IPC_MESSAGE_ROUTED0(ViewMsg_SearchBoxCancel)
IPC_MESSAGE_ROUTED1(ViewMsg_SearchBoxResize,
                    gfx::Rect /* search_box_bounds */)
IPC_MESSAGE_ROUTED4(ViewMsg_DetermineIfPageSupportsInstant,
                    string16 /* value*/,
                    bool /* verbatim */,
                    int /* selection_start */,
                    int /* selection_end */)

// Used to tell the renderer not to add scrollbars with height and
// width below a threshold.
IPC_MESSAGE_ROUTED1(ViewMsg_DisableScrollbarsForSmallWindows,
                    gfx::Size /* disable_scrollbar_size_limit */)

// Used to inform the renderer that the browser has displayed its
// requested notification.
IPC_MESSAGE_ROUTED1(ViewMsg_PostDisplayToNotificationObject,
                    int /* notification_id */)

// Used to inform the renderer that the browser has encountered an error
// trying to display a notification.
IPC_MESSAGE_ROUTED2(ViewMsg_PostErrorToNotificationObject,
                    int /* notification_id */,
                    string16 /* message */)

// Informs the renderer that the one if its notifications has closed.
IPC_MESSAGE_ROUTED2(ViewMsg_PostCloseToNotificationObject,
                    int /* notification_id */,
                    bool /* by_user */)

// Informs the renderer that one of its notifications was clicked on.
IPC_MESSAGE_ROUTED1(ViewMsg_PostClickToNotificationObject,
                    int /* notification_id */)

// Informs the renderer that the one if its notifications has closed.
IPC_MESSAGE_ROUTED1(ViewMsg_PermissionRequestDone,
                    int /* request_id */)

// Activate/deactivate the RenderView (i.e., set its controls' tint
// accordingly, etc.).
IPC_MESSAGE_ROUTED1(ViewMsg_SetActive,
                    bool /* active */)

#if defined(OS_MACOSX)
// Let the RenderView know its window has changed visibility.
IPC_MESSAGE_ROUTED1(ViewMsg_SetWindowVisibility,
                    bool /* visibile */)

// Let the RenderView know its window's frame has changed.
IPC_MESSAGE_ROUTED2(ViewMsg_WindowFrameChanged,
                    gfx::Rect /* window frame */,
                    gfx::Rect /* content view frame */)

// Tell the renderer that plugin IME has completed.
IPC_MESSAGE_ROUTED2(ViewMsg_PluginImeCompositionCompleted,
                    string16 /* text */,
                    int /* plugin_id */)
#endif

// Response message to ViewHostMsg_CreateShared/DedicatedWorker.
// Sent when the worker has started.
IPC_MESSAGE_ROUTED0(ViewMsg_WorkerCreated)

// Tell the renderer which browser window it's being attached to.
IPC_MESSAGE_ROUTED1(ViewMsg_UpdateBrowserWindowId,
                    int /* id of browser window */)

// Tell the renderer which type this view is.
IPC_MESSAGE_ROUTED1(ViewMsg_NotifyRenderViewType,
                    ViewType::Type /* view_type */)

// Notification that renderer should run some JavaScript code.
IPC_MESSAGE_ROUTED1(ViewMsg_ExecuteCode,
                    ViewMsg_ExecuteCode_Params)

// SpellChecker messages.

// Passes some initialization params to the renderer's spellchecker. This can
// be called directly after startup or in (async) response to a
// RequestDictionary ViewHost message.
IPC_MESSAGE_CONTROL4(ViewMsg_SpellChecker_Init,
                     IPC::PlatformFileForTransit /* bdict_file */,
                     std::vector<std::string> /* custom_dict_words */,
                     std::string /* language */,
                     bool /* auto spell correct */)

// A word has been added to the custom dictionary; update the local custom
// word list.
IPC_MESSAGE_CONTROL1(ViewMsg_SpellChecker_WordAdded,
                     std::string /* word */)

// Toggle the auto spell correct functionality.
IPC_MESSAGE_CONTROL1(ViewMsg_SpellChecker_EnableAutoSpellCorrect,
                     bool /* enable */)

// Executes custom context menu action that was provided from WebKit.
IPC_MESSAGE_ROUTED2(ViewMsg_CustomContextMenuAction,
                    webkit_glue::CustomContextMenuContext /* custom_context */,
                    unsigned /* action */)

// Tells the renderer to translate the page contents from one language to
// another.
IPC_MESSAGE_ROUTED4(ViewMsg_TranslatePage,
                    int /* page id */,
                    std::string, /* the script injected in the page */
                    std::string, /* BCP 47/RFC 5646 language code the page
                                    is in */
                    std::string /* BCP 47/RFC 5646 language code to translate
                                   to */)

// Tells the renderer to revert the text of translated page to its original
// contents.
IPC_MESSAGE_ROUTED1(ViewMsg_RevertTranslation,
                    int /* page id */)

// Reply in response to ViewHostMsg_Geolocation_RequestPermission.
IPC_MESSAGE_ROUTED2(ViewMsg_Geolocation_PermissionSet,
                    int /* bridge_id */,
                    bool /* is_allowed */)

// Sent after ViewHostMsg_Geolocation_StartUpdating iff the user has granted
// permission and we have a position available or an error occurs (such as
// permission denied, position unavailable, etc.)
IPC_MESSAGE_ROUTED1(ViewMsg_Geolocation_PositionUpdated,
                    Geoposition /* geoposition */)

// Sent on process startup to indicate whether this process is running in
// incognito mode.
IPC_MESSAGE_CONTROL1(ViewMsg_SetIsIncognitoProcess,
                     bool /* is_incognito_processs */)

// Enable accessibility in the renderer process.
IPC_MESSAGE_ROUTED0(ViewMsg_EnableAccessibility)

// Relay a request from assistive technology to set focus to a given node.
IPC_MESSAGE_ROUTED1(ViewMsg_SetAccessibilityFocus,
                    int /* object id */)

// Relay a request from assistive technology to perform the default action
// on a given node.
IPC_MESSAGE_ROUTED1(ViewMsg_AccessibilityDoDefaultAction,
                    int /* object id */)

// Tells the render view that a ViewHostMsg_AccessibilityNotifications
// message was processed and it can send addition notifications.
IPC_MESSAGE_ROUTED0(ViewMsg_AccessibilityNotifications_ACK)

// Notification that the device's orientation has changed.
IPC_MESSAGE_ROUTED1(ViewMsg_DeviceOrientationUpdated,
                    ViewMsg_DeviceOrientationUpdated_Params)

// The response to ViewHostMsg_AsyncOpenFile.
IPC_MESSAGE_ROUTED3(ViewMsg_AsyncOpenFile_ACK,
                    base::PlatformFileError /* error_code */,
                    IPC::PlatformFileForTransit /* file descriptor */,
                    int /* message_id */)

// A classification model for client-side phishing detection.
// The given file contains an encoded safe_browsing::ClientSideModel
// protocol buffer.
IPC_MESSAGE_CONTROL1(ViewMsg_SetPhishingModel,
                     IPC::PlatformFileForTransit /* model_file */)

// Request a DOM tree when a malware interstitial is shown.
IPC_MESSAGE_ROUTED0(ViewMsg_GetMalwareDOMDetails)

// Tells the renderer to begin phishing detection for the given toplevel URL
// which it has started loading.
IPC_MESSAGE_ROUTED1(ViewMsg_StartPhishingDetection, GURL)

// External popup menus.
IPC_MESSAGE_ROUTED1(ViewMsg_SelectPopupMenuItem,
                    int /* selected index, -1 means no selection */)

// Sent in response to a ViewHostMsg_ContextMenu to let the renderer know that
// the menu has been closed.
IPC_MESSAGE_ROUTED1(ViewMsg_ContextMenuClosed,
                    webkit_glue::CustomContextMenuContext /* custom_context */)

// Tells the renderer that the network state has changed and that
// window.navigator.onLine should be updated for all WebViews.
IPC_MESSAGE_ROUTED1(ViewMsg_NetworkStateChanged,
                    bool /* online */)

//-----------------------------------------------------------------------------
// TabContents messages
// These are messages sent from the renderer to the browser process.

// Sent by the renderer when it is creating a new window.  The browser creates
// a tab for it and responds with a ViewMsg_CreatingNew_ACK.  If route_id is
// MSG_ROUTING_NONE, the view couldn't be created.
IPC_SYNC_MESSAGE_CONTROL1_2(ViewHostMsg_CreateWindow,
                            ViewHostMsg_CreateWindow_Params,
                            int /* route_id */,
                            int64 /* cloned_session_storage_namespace_id */)

// Similar to ViewHostMsg_CreateWindow, except used for sub-widgets, like
// <select> dropdowns.  This message is sent to the TabContents that
// contains the widget being created.
IPC_SYNC_MESSAGE_CONTROL2_1(ViewHostMsg_CreateWidget,
                            int /* opener_id */,
                            WebKit::WebPopupType /* popup type */,
                            int /* route_id */)

// Similar to ViewHostMsg_CreateWidget except the widget is a full screen
// window.
IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_CreateFullscreenWidget,
                            int /* opener_id */,
                            int /* route_id */)

// These three messages are sent to the parent RenderViewHost to display the
// page/widget that was created by
// CreateWindow/CreateWidget/CreateFullscreenWidget. routing_id
// refers to the id that was returned from the Create message above.
// The initial_position parameter is a rectangle in screen coordinates.
//
// FUTURE: there will probably be flags here to control if the result is
// in a new window.
IPC_MESSAGE_ROUTED4(ViewHostMsg_ShowView,
                    int /* route_id */,
                    WindowOpenDisposition /* disposition */,
                    gfx::Rect /* initial_pos */,
                    bool /* opened_by_user_gesture */)

IPC_MESSAGE_ROUTED2(ViewHostMsg_ShowWidget,
                    int /* route_id */,
                    gfx::Rect /* initial_pos */)

// Message to show a full screen widget.
IPC_MESSAGE_ROUTED1(ViewHostMsg_ShowFullscreenWidget,
                    int /* route_id */)

// Message to show a popup menu using native cocoa controls (Mac only).
IPC_MESSAGE_ROUTED1(ViewHostMsg_ShowPopup,
                    ViewHostMsg_ShowPopup_Params)

// This message is sent after ViewHostMsg_ShowView to cause the RenderView
// to run in a modal fashion until it is closed.
IPC_SYNC_MESSAGE_ROUTED0_0(ViewHostMsg_RunModal)

IPC_MESSAGE_CONTROL1(ViewHostMsg_UpdatedCacheStats,
                     WebKit::WebCache::UsageStats /* stats */)

// Indicates the renderer is ready in response to a ViewMsg_New or
// a ViewMsg_CreatingNew_ACK.
IPC_MESSAGE_ROUTED0(ViewHostMsg_RenderViewReady)


// Indicates the renderer process is gone.  This actually is sent by the
// browser process to itself, but keeps the interface cleaner.
IPC_MESSAGE_ROUTED2(ViewHostMsg_RenderViewGone,
                    int, /* this really is base::TerminationStatus */
                    int /* exit_code */)

// Sent by the renderer process to request that the browser close the view.
// This corresponds to the window.close() API, and the browser may ignore
// this message.  Otherwise, the browser will generates a ViewMsg_Close
// message to close the view.
IPC_MESSAGE_ROUTED0(ViewHostMsg_Close)

// Sent by the renderer process to request that the browser move the view.
// This corresponds to the window.resizeTo() and window.moveTo() APIs, and
// the browser may ignore this message.
IPC_MESSAGE_ROUTED1(ViewHostMsg_RequestMove,
                    gfx::Rect /* position */)

// Notifies the browser that a frame in the view has changed. This message
// has a lot of parameters and is packed/unpacked by functions defined in
// render_messages.h.
IPC_MESSAGE_ROUTED1(ViewHostMsg_FrameNavigate,
                    ViewHostMsg_FrameNavigate_Params)

// Notifies the browser that we have session history information.
// page_id: unique ID that allows us to distinguish between history entries.
IPC_MESSAGE_ROUTED2(ViewHostMsg_UpdateState,
                    int32 /* page_id */,
                    std::string /* state */)

// Notifies the browser that a document has been loaded in a frame.
IPC_MESSAGE_ROUTED1(ViewHostMsg_DocumentLoadedInFrame,
                    int64 /* frame_id */)

// Notifies the browser that a frame finished loading.
IPC_MESSAGE_ROUTED1(ViewHostMsg_DidFinishLoad,
                    int64 /* frame_id */)

// Changes the title for the page in the UI when the page is navigated or the
// title changes.
// TODO(darin): use a UTF-8 string to reduce data size
IPC_MESSAGE_ROUTED2(ViewHostMsg_UpdateTitle, int32, std::wstring)

// Changes the icon url for the page in the UI.
IPC_MESSAGE_ROUTED2(ViewHostMsg_UpdateIconURL, int32, GURL)

// Change the encoding name of the page in UI when the page has detected
// proper encoding name.
IPC_MESSAGE_ROUTED1(ViewHostMsg_UpdateEncoding,
                    std::string /* new encoding name */)

// Notifies the browser that we want to show a destination url for a potential
// action (e.g. when the user is hovering over a link).
IPC_MESSAGE_ROUTED2(ViewHostMsg_UpdateTargetURL, int32, GURL)

// Sent when the renderer starts loading the page. This corresponds to
// WebKit's notion of the throbber starting. Note that sometimes you may get
// duplicates of these during a single load.
IPC_MESSAGE_ROUTED0(ViewHostMsg_DidStartLoading)

// Sent when the renderer is done loading a page. This corresponds to WebKit's
// notion of the throbber stopping.
IPC_MESSAGE_ROUTED0(ViewHostMsg_DidStopLoading)

// Sent when the renderer main frame has made progress loading.
IPC_MESSAGE_ROUTED1(ViewHostMsg_DidChangeLoadProgress,
                    double /* load_progress */)

// Sent when the document element is available for the toplevel frame.  This
// happens after the page starts loading, but before all resources are
// finished.
IPC_MESSAGE_ROUTED0(ViewHostMsg_DocumentAvailableInMainFrame)

// Sent when after the onload handler has been invoked for the document
// in the toplevel frame.
IPC_MESSAGE_ROUTED1(ViewHostMsg_DocumentOnLoadCompletedInMainFrame,
                    int32 /* page_id */)

// Sent when the renderer loads a resource from its memory cache.
// The security info is non empty if the resource was originally loaded over
// a secure connection.
// Note: May only be sent once per URL per frame per committed load.
IPC_MESSAGE_ROUTED2(ViewHostMsg_DidLoadResourceFromMemoryCache,
                    GURL /* url */,
                    std::string  /* security info */)

// Sent when the renderer displays insecure content in a secure page.
IPC_MESSAGE_ROUTED0(ViewHostMsg_DidDisplayInsecureContent)

// Sent when the renderer runs insecure content in a secure origin.
IPC_MESSAGE_ROUTED2(ViewHostMsg_DidRunInsecureContent,
                    std::string  /* security_origin */,
                    GURL         /* target URL */);

// Sent when the renderer starts a provisional load for a frame.
IPC_MESSAGE_ROUTED3(ViewHostMsg_DidStartProvisionalLoadForFrame,
                    int64 /* frame_id */,
                    bool /* true if it is the main frame */,
                    GURL /* url */)

// Sent when the renderer fails a provisional load with an error.
IPC_MESSAGE_ROUTED5(ViewHostMsg_DidFailProvisionalLoadWithError,
                    int64 /* frame_id */,
                    bool /* true if it is the main frame */,
                    int /* error_code */,
                    GURL /* url */,
                    bool /* true if the failure is the result of
                            navigating to a POST again and we're going to
                            show the POST interstitial */)

// Tells the render view that a ViewHostMsg_PaintAtSize message was
// processed, and the DIB is ready for use. |tag| has the same value that
// the tag sent along with ViewMsg_PaintAtSize.
IPC_MESSAGE_ROUTED2(ViewHostMsg_PaintAtSize_ACK,
                    int /* tag */,
                    gfx::Size /* size */)

// Sent to update part of the view.  In response to this message, the host
// generates a ViewMsg_UpdateRect_ACK message.
IPC_MESSAGE_ROUTED1(ViewHostMsg_UpdateRect,
                    ViewHostMsg_UpdateRect_Params)

// Sent by the renderer when accelerated compositing is enabled or disabled to
// notify the browser whether or not is should do painting.
IPC_MESSAGE_ROUTED1(ViewHostMsg_DidActivateAcceleratedCompositing,
                    bool /* true if the accelerated compositor is actve */)

// Acknowledges receipt of a ViewMsg_HandleInputEvent message.
// Payload is a WebInputEvent::Type which is the type of the event, followed
// by an optional WebInputEvent which is provided only if the event was not
// processed.
IPC_MESSAGE_ROUTED0(ViewHostMsg_HandleInputEvent_ACK)

IPC_MESSAGE_ROUTED0(ViewHostMsg_Focus)
IPC_MESSAGE_ROUTED0(ViewHostMsg_Blur)

// Message sent from renderer to the browser when focus changes inside the
// webpage. The parameter says whether the newly focused element needs
// keyboard input (true for textfields, text areas and content editable divs).
IPC_MESSAGE_ROUTED1(ViewHostMsg_FocusedNodeChanged,
    bool /* is_editable_node */)

// Returns the window location of the given window.
// TODO(shess): Provide a mapping from reply_msg->routing_id() to
// HWND so that we can eliminate the NativeViewId parameter.
IPC_SYNC_MESSAGE_ROUTED1_1(ViewHostMsg_GetWindowRect,
                           gfx::NativeViewId /* window */,
                           gfx::Rect /* Out: Window location */)

IPC_MESSAGE_ROUTED1(ViewHostMsg_SetCursor, WebCursor)
// Result of string search in the page.
// Response to ViewMsg_Find with the results of the requested find-in-page
// search, the number of matches found and the selection rect (in screen
// coordinates) for the string found. If |final_update| is false, it signals
// that this is not the last Find_Reply message - more will be sent as the
// scoping effort continues.
IPC_MESSAGE_ROUTED5(ViewHostMsg_Find_Reply,
                    int /* request_id */,
                    int /* number of matches */,
                    gfx::Rect /* selection_rect */,
                    int /* active_match_ordinal */,
                    bool /* final_update */)

// Used to set a cookie. The cookie is set asynchronously, but will be
// available to a subsequent ViewHostMsg_GetCookies request.
IPC_MESSAGE_ROUTED3(ViewHostMsg_SetCookie,
                    GURL /* url */,
                    GURL /* first_party_for_cookies */,
                    std::string /* cookie */)

// Used to get cookies for the given URL. This may block waiting for a
// previous SetCookie message to be processed.
IPC_SYNC_MESSAGE_ROUTED2_1(ViewHostMsg_GetCookies,
                           GURL /* url */,
                           GURL /* first_party_for_cookies */,
                           std::string /* cookies */)

// Used to get raw cookie information for the given URL. This may block
// waiting for a previous SetCookie message to be processed.
IPC_SYNC_MESSAGE_ROUTED2_1(ViewHostMsg_GetRawCookies,
                           GURL /* url */,
                           GURL /* first_party_for_cookies */,
                           std::vector<webkit_glue::WebCookie>
                               /* raw_cookies */)

// Used to delete cookie for the given URL and name
IPC_SYNC_MESSAGE_CONTROL2_0(ViewHostMsg_DeleteCookie,
                            GURL /* url */,
                            std::string /* cookie_name */)

// Used to check if cookies are enabled for the given URL. This may block
// waiting for a previous SetCookie message to be processed.
IPC_SYNC_MESSAGE_ROUTED2_1(ViewHostMsg_CookiesEnabled,
                           GURL /* url */,
                           GURL /* first_party_for_cookies */,
                           bool /* cookies_enabled */)

// Used to get the list of plugins
IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_GetPlugins,
    bool /* refresh*/,
    std::vector<webkit::npapi::WebPluginInfo> /* plugins */)

// Return information about a plugin for the given URL and MIME
// type. If there is no matching plugin, |found| is false.  If
// |enabled| in the WebPluginInfo struct is false, the plug-in is
// treated as if it was not installed at all.
//
// If |setting| is set to CONTENT_SETTING_BLOCK, the plug-in is
// blocked by the content settings for |policy_url|. It still
// appears in navigator.plugins in Javascript though, and can be
// loaded via click-to-play.
//
// If |setting| is set to CONTENT_SETTING_ALLOW, the domain is
// explicitly white-listed for the plug-in, or the user has chosen
// not to block nonsandboxed plugins.
//
// If |setting| is set to CONTENT_SETTING_DEFAULT, the plug-in is
// neither blocked nor white-listed, which means that it's allowed
// by default and can still be blocked if it's non-sandboxed.
//
// |actual_mime_type| is the actual mime type supported by the
// plugin found that match the URL given (one for each item in
// |info|).
IPC_SYNC_MESSAGE_CONTROL4_4(ViewHostMsg_GetPluginInfo,
                            int /* routing_id */,
                            GURL /* url */,
                            GURL /* policy_url */,
                            std::string /* mime_type */,
                            bool /* found */,
                            webkit::npapi::WebPluginInfo /* plugin info */,
                            ContentSetting /* setting */,
                            std::string /* actual_mime_type */)

// Requests spellcheck for a word.
IPC_SYNC_MESSAGE_ROUTED2_2(ViewHostMsg_SpellCheck,
                           string16 /* word to check */,
                           int /* document tag*/,
                           int /* misspell location */,
                           int /* misspell length */)

// Asks the browser for a unique document tag.
IPC_SYNC_MESSAGE_ROUTED0_1(ViewHostMsg_GetDocumentTag,
                           int /* the tag */)


// This message tells the spellchecker that a document, identified by an int
// tag, has been closed and all of the ignored words for that document can be
// forgotten.
IPC_MESSAGE_ROUTED1(ViewHostMsg_DocumentWithTagClosed,
                    int /* the tag */)

// Tells the browser to display or not display the SpellingPanel
IPC_MESSAGE_ROUTED1(ViewHostMsg_ShowSpellingPanel,
                    bool /* if true, then show it, otherwise hide it*/)

// Tells the browser to update the spelling panel with the given word.
IPC_MESSAGE_ROUTED1(ViewHostMsg_UpdateSpellingPanelWithMisspelledWord,
                    string16 /* the word to update the panel with */)

// Tells the browser that content in the current page was blocked due to the
// user's content settings.
IPC_MESSAGE_ROUTED2(ViewHostMsg_ContentBlocked,
                    ContentSettingsType, /* type of blocked content */
                    std::string /* resource identifier */)

// Tells the browser that  a specific Appcache manifest in the current page
// was accessed.
IPC_MESSAGE_ROUTED2(ViewHostMsg_AppCacheAccessed,
                    GURL /* manifest url */,
                    bool /* blocked by policy */)

// Tells the browser that a specific Web database in the current page was
// accessed.
IPC_MESSAGE_ROUTED5(ViewHostMsg_WebDatabaseAccessed,
                    GURL /* origin url */,
                    string16 /* database name */,
                    string16 /* database display name */,
                    unsigned long /* estimated size */,
                    bool /* blocked by policy */)

// Initiates a download based on user actions like 'ALT+click'.
IPC_MESSAGE_ROUTED2(ViewHostMsg_DownloadUrl,
                    GURL /* url */,
                    GURL /* referrer */)

// Used to go to the session history entry at the given offset (ie, -1 will
// return the "back" item).
IPC_MESSAGE_ROUTED1(ViewHostMsg_GoToEntryAtOffset,
                    int /* offset (from current) of history item to get */)

IPC_SYNC_MESSAGE_ROUTED4_2(ViewHostMsg_RunJavaScriptMessage,
                           std::wstring /* in - alert message */,
                           std::wstring /* in - default prompt */,
                           GURL         /* in - originating page URL */,
                           int          /* in - dialog flags */,
                           bool         /* out - success */,
                           std::wstring /* out - prompt field */)

// Provides the contents for the given page that was loaded recently.
IPC_MESSAGE_ROUTED5(ViewHostMsg_PageContents,
                    GURL         /* URL of the page */,
                    int32        /* page id */,
                    string16     /* page contents */,
                    std::string  /* page ISO639_1 language code */,
                    bool         /* whether the page can be translated */)

// Used to get the extension message bundle.
IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_GetExtensionMessageBundle,
                            std::string /* extension id */,
                            SubstitutionMap /* message bundle */)

// Specifies the URL as the first parameter (a wstring) and thumbnail as
// binary data as the second parameter.
IPC_MESSAGE_ROUTED3(ViewHostMsg_Thumbnail,
                    GURL /* url */,
                    ThumbnailScore /* score */,
                    SkBitmap /* bitmap */)

// Send a snapshot of the tab contents to the render host.
IPC_MESSAGE_ROUTED1(ViewHostMsg_Snapshot,
                    SkBitmap /* bitmap */)

// Notification that the url for the favicon of a site has been determined.
IPC_MESSAGE_ROUTED2(ViewHostMsg_UpdateFavIconURL,
                    int32 /* page_id */,
                    GURL /* url of the favicon */)

// Used to tell the parent that the user right clicked on an area of the
// content area, and a context menu should be shown for it. The params
// object contains information about the node(s) that were selected when the
// user right clicked.
IPC_MESSAGE_ROUTED1(ViewHostMsg_ContextMenu, ContextMenuParams)

// Requests that the given URL be opened in the specified manner.
IPC_MESSAGE_ROUTED3(ViewHostMsg_OpenURL,
                    GURL /* url */,
                    GURL /* referrer */,
                    WindowOpenDisposition /* disposition */)

// Notifies that the preferred size of the content changed.
IPC_MESSAGE_ROUTED1(ViewHostMsg_DidContentsPreferredSizeChange,
                    gfx::Size /* pref_size */)

// Following message is used to communicate the values received by the
// callback binding the JS to Cpp.
// An instance of browser that has an automation host listening to it can
// have a javascript send a native value (string, number, boolean) to the
// listener in Cpp. (DomAutomationController)
IPC_MESSAGE_ROUTED2(ViewHostMsg_DomOperationResponse,
                    std::string  /* json_string */,
                    int  /* automation_id */)

// A message from HTML-based UI.  When (trusted) Javascript calls
// send(message, args), this message is sent to the browser.
IPC_MESSAGE_ROUTED3(ViewHostMsg_WebUISend,
                    GURL /* source_url */,
                    std::string  /* message */,
                    std::string  /* args (as a JSON string) */)

// A message for an external host.
IPC_MESSAGE_ROUTED3(ViewHostMsg_ForwardMessageToExternalHost,
                    std::string  /* message */,
                    std::string  /* origin */,
                    std::string  /* target */)

// A renderer sends this to the browser process when it wants to
// create a plugin.  The browser will create the plugin process if
// necessary, and will return a handle to the channel on success.
// On error an empty string is returned.
IPC_SYNC_MESSAGE_CONTROL3_2(ViewHostMsg_OpenChannelToPlugin,
                            int /* routing_id */,
                            GURL /* url */,
                            std::string /* mime_type */,
                            IPC::ChannelHandle /* channel_handle */,
                            webkit::npapi::WebPluginInfo /* info */)

// A renderer sends this to the browser process when it wants to
// create a pepper plugin.  The browser will create the plugin process if
// necessary, and will return a handle to the channel on success.
// On error an empty string is returned.
IPC_SYNC_MESSAGE_CONTROL1_2(ViewHostMsg_OpenChannelToPepperPlugin,
                            FilePath /* path */,
                            base::ProcessHandle /* plugin_process_handle */,
                            IPC::ChannelHandle /* handle to channel */)

// A renderer sends this to the browser process when it wants to start
// a new instance of the Native Client process. The browser will launch
// the process and return a handle to an IMC channel.
IPC_SYNC_MESSAGE_CONTROL2_3(ViewHostMsg_LaunchNaCl,
                            std::wstring /* url for the NaCl module */,
                            int /* socket count */,
                            std::vector<nacl::FileDescriptor>
                                /* imc channel handles */,
                            base::ProcessHandle /* NaCl process handle */,
                            base::ProcessId /* NaCl process id */)

#if defined(USE_X11)
// A renderer sends this when it needs a browser-side widget for
// hosting a windowed plugin. id is the XID of the plugin window, for which
// the container is created.
IPC_SYNC_MESSAGE_ROUTED1_0(ViewHostMsg_CreatePluginContainer,
                           gfx::PluginWindowHandle /* id */)

// Destroy a plugin container previously created using CreatePluginContainer.
// id is the XID of the plugin window corresponding to the container that is
// to be destroyed.
IPC_SYNC_MESSAGE_ROUTED1_0(ViewHostMsg_DestroyPluginContainer,
                           gfx::PluginWindowHandle /* id */)
#endif

#if defined(OS_MACOSX)
// Request that the browser load a font into shared memory for us.
IPC_SYNC_MESSAGE_CONTROL1_2(ViewHostMsg_LoadFont,
                           FontDescriptor /* font to load */,
                           uint32 /* buffer size */,
                           base::SharedMemoryHandle /* font data */)
#endif

#if defined(OS_WIN)
// Request that the given font be loaded by the browser so it's cached by the
// OS. Please see ChildProcessHost::PreCacheFont for details.
IPC_SYNC_MESSAGE_CONTROL1_0(ViewHostMsg_PreCacheFont,
                            LOGFONT /* font data */)
#endif  // defined(OS_WIN)

// Returns WebScreenInfo corresponding to the view.
// TODO(shess): Provide a mapping from reply_msg->routing_id() to
// HWND so that we can eliminate the NativeViewId parameter.
IPC_SYNC_MESSAGE_ROUTED1_1(ViewHostMsg_GetScreenInfo,
                           gfx::NativeViewId /* view */,
                           WebKit::WebScreenInfo /* results */)

// Send the tooltip text for the current mouse position to the browser.
IPC_MESSAGE_ROUTED2(ViewHostMsg_SetTooltipText,
                    std::wstring /* tooltip text string */,
                    WebKit::WebTextDirection /* text direction hint */)

// Notification that the text selection has changed.
IPC_MESSAGE_ROUTED1(ViewHostMsg_SelectionChanged,
                    std::string /* currently selected text */)

// Asks the browser to display the file chooser.  The result is returned in a
// ViewHost_RunFileChooserResponse message.
IPC_MESSAGE_ROUTED1(ViewHostMsg_RunFileChooser,
                    ViewHostMsg_RunFileChooser_Params)

// Used to tell the parent the user started dragging in the content area. The
// WebDropData struct contains contextual information about the pieces of the
// page the user dragged. The parent uses this notification to initiate a
// drag session at the OS level.
IPC_MESSAGE_ROUTED4(ViewHostMsg_StartDragging,
                    WebDropData /* drop_data */,
                    WebKit::WebDragOperationsMask /* ops_allowed */,
                    SkBitmap /* image */,
                    gfx::Point /* image_offset */)

// The page wants to update the mouse cursor during a drag & drop operation.
// |is_drop_target| is true if the mouse is over a valid drop target.
IPC_MESSAGE_ROUTED1(ViewHostMsg_UpdateDragCursor,
                    WebKit::WebDragOperation /* drag_operation */)

// Tells the browser to move the focus to the next (previous if reverse is
// true) focusable element.
IPC_MESSAGE_ROUTED1(ViewHostMsg_TakeFocus, bool /* reverse */)

// Notification that the page has an OpenSearch description document
// associated with it.
IPC_MESSAGE_ROUTED3(ViewHostMsg_PageHasOSDD,
                    int32 /* page_id */,
                    GURL /* url of OS description document */,
                    ViewHostMsg_PageHasOSDD_Type)

// Find out if the given url's security origin is installed as a search
// provider.
IPC_SYNC_MESSAGE_ROUTED2_1(
    ViewHostMsg_GetSearchProviderInstallState,
    GURL /* page url */,
    GURL /* inquiry url */,
    ViewHostMsg_GetSearchProviderInstallState_Params /* install */)

// Required for updating text input state.
IPC_MESSAGE_ROUTED2(ViewHostMsg_ImeUpdateTextInputState,
                    WebKit::WebTextInputType, /* text_input_type */
                    gfx::Rect /* caret_rect */)

// Required for cancelling an ongoing input method composition.
IPC_MESSAGE_ROUTED0(ViewHostMsg_ImeCancelComposition)

// Tells the browser that the renderer is done calculating the number of
// rendered pages according to the specified settings.
IPC_MESSAGE_ROUTED2(ViewHostMsg_DidGetPrintedPagesCount,
                    int /* rendered document cookie */,
                    int /* number of rendered pages */)

// Sends back to the browser the rendered "printed page" that was requested by
// a ViewMsg_PrintPage message or from scripted printing. The memory handle in
// this message is already valid in the browser process.
IPC_MESSAGE_ROUTED1(ViewHostMsg_DidPrintPage,
                    ViewHostMsg_DidPrintPage_Params /* page content */)

// The renderer wants to know the default print settings.
IPC_SYNC_MESSAGE_ROUTED0_1(ViewHostMsg_GetDefaultPrintSettings,
                           ViewMsg_Print_Params /* default_settings */)

// The renderer wants to update the current print settings with new
// |job_settings|.
IPC_SYNC_MESSAGE_ROUTED2_1(ViewHostMsg_UpdatePrintSettings,
                           int /* document_cookie */,
                           DictionaryValue /* job_settings */,
                           ViewMsg_PrintPages_Params /* current_settings */)

// It's the renderer that controls the printing process when it is generated
// by javascript. This step is about showing UI to the user to select the
// final print settings. The output parameter is the same as
// ViewMsg_PrintPages which is executed implicitly.
IPC_SYNC_MESSAGE_ROUTED1_1(ViewHostMsg_ScriptedPrint,
                           ViewHostMsg_ScriptedPrint_Params,
                           ViewMsg_PrintPages_Params
                               /* settings chosen by the user*/)

// WebKit and JavaScript error messages to log to the console
// or debugger UI.
IPC_MESSAGE_ROUTED3(ViewHostMsg_AddMessageToConsole,
                    std::wstring, /* msg */
                    int32, /* line number */
                    std::wstring /* source id */)

// Stores new inspector setting in the profile.
IPC_MESSAGE_ROUTED2(ViewHostMsg_UpdateInspectorSetting,
                    std::string,  /* key */
                    std::string /* value */)

// Wraps an IPC message that's destined to the DevToolsClient on
// DevToolsAgent->browser hop.
IPC_MESSAGE_ROUTED1(ViewHostMsg_ForwardToDevToolsClient,
                    IPC::Message /* one of DevToolsClientMsg_XXX types */)

// Wraps an IPC message that's destined to the DevToolsAgent on
// DevToolsClient->browser hop.
IPC_MESSAGE_ROUTED1(ViewHostMsg_ForwardToDevToolsAgent,
                    IPC::Message /* one of DevToolsAgentMsg_XXX types */)

// Activates (brings to the front) corresponding dev tools window.
IPC_MESSAGE_ROUTED0(ViewHostMsg_ActivateDevToolsWindow)

// Closes dev tools window that is inspecting current render_view_host.
IPC_MESSAGE_ROUTED0(ViewHostMsg_CloseDevToolsWindow)

// Attaches dev tools window that is inspecting current render_view_host.
IPC_MESSAGE_ROUTED0(ViewHostMsg_RequestDockDevToolsWindow)

// Detaches dev tools window that is inspecting current render_view_host.
IPC_MESSAGE_ROUTED0(ViewHostMsg_RequestUndockDevToolsWindow)

// Updates runtime features store in devtools manager in order to support
// cross-navigation instrumentation.
IPC_MESSAGE_ROUTED2(ViewHostMsg_DevToolsRuntimePropertyChanged,
                    std::string /* name */,
                    std::string /* value */)

// Send back a string to be recorded by UserMetrics.
IPC_MESSAGE_CONTROL1(ViewHostMsg_UserMetricsRecordAction,
                     std::string /* action */)

// Send back histograms as vector of pickled-histogram strings.
IPC_MESSAGE_CONTROL2(ViewHostMsg_RendererHistograms,
                     int, /* sequence number of Renderer Histograms. */
                     std::vector<std::string>)

#if defined USE_TCMALLOC
// Send back tcmalloc stats output.
IPC_MESSAGE_CONTROL2(ViewHostMsg_RendererTcmalloc,
                     int          /* pid */,
                     std::string  /* tcmalloc debug output */)
#endif

// Sends back stats about the V8 heap.
IPC_MESSAGE_CONTROL2(ViewHostMsg_V8HeapStats,
                     int /* size of heap (allocated from the OS) */,
                     int /* bytes in use */)

// Request for a DNS prefetch of the names in the array.
// NameList is typedef'ed std::vector<std::string>
IPC_MESSAGE_CONTROL1(ViewHostMsg_DnsPrefetch,
                    std::vector<std::string> /* hostnames */)

// Notifies when default plugin updates status of the missing plugin.
IPC_MESSAGE_ROUTED1(ViewHostMsg_MissingPluginStatus,
                    int /* status */)

// Sent by the renderer process to indicate that a plugin instance has
// crashed.
IPC_MESSAGE_ROUTED1(ViewHostMsg_CrashedPlugin,
                    FilePath /* plugin_path */)

// Notifies when a plugin couldn't be loaded because it's outdated.
IPC_MESSAGE_ROUTED2(ViewHostMsg_BlockedOutdatedPlugin,
                    string16, /* name */
                    GURL      /* update_url */)

// Displays a JavaScript out-of-memory message in the infobar.
IPC_MESSAGE_ROUTED0(ViewHostMsg_JSOutOfMemory)

// Displays a box to confirm that the user wants to navigate away from the
// page. Replies true if yes, false otherwise, the reply string is ignored,
// but is included so that we can use OnJavaScriptMessageBoxClosed.
IPC_SYNC_MESSAGE_ROUTED2_2(ViewHostMsg_RunBeforeUnloadConfirm,
                           GURL,        /* in - originating frame URL */
                           std::wstring /* in - alert message */,
                           bool         /* out - success */,
                           std::wstring /* out - This is ignored.*/)

IPC_MESSAGE_ROUTED3(ViewHostMsg_SendCurrentPageAllSavableResourceLinks,
                    std::vector<GURL> /* all savable resource links */,
                    std::vector<GURL> /* all referrers of resource links */,
                    std::vector<GURL> /* all frame links */)

IPC_MESSAGE_ROUTED3(ViewHostMsg_SendSerializedHtmlData,
                    GURL /* frame's url */,
                    std::string /* data buffer */,
                    int32 /* complete status */)

IPC_SYNC_MESSAGE_ROUTED4_1(ViewHostMsg_ShowModalHTMLDialog,
                           GURL /* url */,
                           int /* width */,
                           int /* height */,
                           std::string /* json_arguments */,
                           std::string /* json_retval */)

IPC_MESSAGE_ROUTED2(ViewHostMsg_DidGetApplicationInfo,
                    int32 /* page_id */,
                    WebApplicationInfo)

// Sent by the renderer to implement chrome.app.installApplication().
IPC_MESSAGE_ROUTED1(ViewHostMsg_InstallApplication,
                    WebApplicationInfo)

// Provides the result from running OnMsgShouldClose.  |proceed| matches the
// return value of the the frame's shouldClose method (which includes the
// onbeforeunload handler): true if the user decided to proceed with leaving
// the page.
IPC_MESSAGE_ROUTED1(ViewHostMsg_ShouldClose_ACK,
                    bool /* proceed */)

// Indicates that the current page has been closed, after a ClosePage
// message. The parameters are just echoed from the ClosePage request.
IPC_MESSAGE_ROUTED1(ViewHostMsg_ClosePage_ACK,
                    ViewMsg_ClosePage_Params)

IPC_MESSAGE_ROUTED4(ViewHostMsg_DidDownloadFavIcon,
                    int /* Identifier of the request */,
                    GURL /* URL of the image */,
                    bool /* true if there was a network error */,
                    SkBitmap /* image_data */)

// Get the CPBrowsingContext associated with the renderer sending this
// message.
IPC_SYNC_MESSAGE_CONTROL0_1(ViewHostMsg_GetCPBrowsingContext,
                            uint32 /* context */)

IPC_MESSAGE_CONTROL1(ViewHostMsg_RevealFolderInOS,
                     FilePath /* path */)

// Sent when a provisional load on the main frame redirects.
IPC_MESSAGE_ROUTED3(ViewHostMsg_DidRedirectProvisionalLoad,
                    int /* page_id */,
                    GURL /* last url */,
                    GURL /* url redirected to */)

// Sent when the renderer changes the zoom level for a particular url, so the
// browser can update its records.  If remember is true, then url is used to
// update the zoom level for all pages in that site.  Otherwise, the render
// view's id is used so that only the menu is updated.
IPC_MESSAGE_ROUTED3(ViewHostMsg_DidZoomURL,
                    double /* zoom_level */,
                    bool /* remember */,
                    GURL /* url */)

#if defined(OS_WIN)
// Duplicates a shared memory handle from the renderer to the browser. Then
// the renderer can flush the handle.
IPC_SYNC_MESSAGE_ROUTED1_1(ViewHostMsg_DuplicateSection,
                           base::SharedMemoryHandle /* renderer handle */,
                           base::SharedMemoryHandle /* browser handle */)
#endif

#if defined(USE_X11)
// Asks the browser to create a temporary file for the renderer to fill
// in resulting NativeMetafile in printing.
IPC_SYNC_MESSAGE_CONTROL0_2(ViewHostMsg_AllocateTempFileForPrinting,
                            base::FileDescriptor /* temp file fd */,
                            int /* fd in browser*/)
IPC_MESSAGE_CONTROL1(ViewHostMsg_TempFileForPrintingWritten,
                     int /* fd in browser */)
#endif

// Asks the browser to create a block of shared memory for the renderer to
// fill in and pass back to the browser.
IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_AllocateSharedMemoryBuffer,
                           uint32 /* buffer size */,
                           base::SharedMemoryHandle /* browser handle */)

// Provide the browser process with information about the WebCore resource
// cache.
IPC_MESSAGE_CONTROL1(ViewHostMsg_ResourceTypeStats,
                     WebKit::WebCache::ResourceTypeStats)

// Notify the browser that this render process can or can't be suddenly
// terminated.
IPC_MESSAGE_CONTROL1(ViewHostMsg_SuddenTerminationChanged,
                     bool /* enabled */)

// Returns the window location of the window this widget is embeded.
// TODO(shess): Provide a mapping from reply_msg->routing_id() to
// HWND so that we can eliminate the NativeViewId parameter.
IPC_SYNC_MESSAGE_ROUTED1_1(ViewHostMsg_GetRootWindowRect,
                           gfx::NativeViewId /* window */,
                           gfx::Rect /* Out: Window location */)

// Informs the browser of a new appcache host.
IPC_MESSAGE_CONTROL1(AppCacheMsg_RegisterHost,
                     int /* host_id */)

// Informs the browser of an appcache host being destroyed.
IPC_MESSAGE_CONTROL1(AppCacheMsg_UnregisterHost,
                     int /* host_id */)

// Initiates the cache selection algorithm for the given host.
// This is sent prior to any subresource loads. An AppCacheMsg_CacheSelected
// message will be sent in response.
// 'host_id' indentifies a specific document or worker
// 'document_url' the url of the main resource
// 'appcache_document_was_loaded_from' the id of the appcache the main
//     resource was loaded from or kNoCacheId
// 'opt_manifest_url' the manifest url specified in the <html> tag if any
IPC_MESSAGE_CONTROL4(AppCacheMsg_SelectCache,
                     int /* host_id */,
                     GURL  /* document_url */,
                     int64 /* appcache_document_was_loaded_from */,
                     GURL  /* opt_manifest_url */)

// Initiates worker specific cache selection algorithm for the given host.
IPC_MESSAGE_CONTROL3(AppCacheMsg_SelectCacheForWorker,
                     int /* host_id */,
                     int /* parent_process_id */,
                     int /* parent_host_id */)
IPC_MESSAGE_CONTROL2(AppCacheMsg_SelectCacheForSharedWorker,
                     int /* host_id */,
                     int64 /* appcache_id */)

// Informs the browser of a 'foreign' entry in an appcache.
IPC_MESSAGE_CONTROL3(AppCacheMsg_MarkAsForeignEntry,
                     int /* host_id */,
                     GURL  /* document_url */,
                     int64 /* appcache_document_was_loaded_from */)

// Returns the status of the appcache associated with host_id.
IPC_SYNC_MESSAGE_CONTROL1_1(AppCacheMsg_GetStatus,
                            int /* host_id */,
                            appcache::Status)

// Initiates an update of the appcache associated with host_id.
IPC_SYNC_MESSAGE_CONTROL1_1(AppCacheMsg_StartUpdate,
                            int /* host_id */,
                            bool /* success */)

// Swaps a new pending appcache, if there is one, into use for host_id.
IPC_SYNC_MESSAGE_CONTROL1_1(AppCacheMsg_SwapCache,
                            int /* host_id */,
                            bool /* success */)

// Gets resource list from appcache synchronously.
IPC_SYNC_MESSAGE_CONTROL1_1(AppCacheMsg_GetResourceList,
                            int /* host_id in*/,
                            std::vector<appcache::AppCacheResourceInfo>
                            /* resources out */)

// Get the list of proxies to use for |url|, as a semicolon delimited list
// of "<TYPE> <HOST>:<PORT>" | "DIRECT". See also
// PluginProcessHostMsg_ResolveProxy which does the same thing.
IPC_SYNC_MESSAGE_CONTROL1_2(ViewHostMsg_ResolveProxy,
                            GURL /* url */,
                            int /* network error */,
                            std::string /* proxy list */)

// Request that got sent to browser for creating an audio output stream
IPC_MESSAGE_ROUTED3(ViewHostMsg_CreateAudioStream,
                    int /* stream_id */,
                    ViewHostMsg_Audio_CreateStream_Params,
                    bool /* low-latency */)

// Tell the browser the audio buffer prepared for stream
// (render_view_id, stream_id) is filled and is ready to be consumed.
IPC_MESSAGE_ROUTED2(ViewHostMsg_NotifyAudioPacketReady,
                    int /* stream_id */,
                    uint32 /* packet size */)

// Start buffering and play the audio stream specified by
// (render_view_id, stream_id).
IPC_MESSAGE_ROUTED1(ViewHostMsg_PlayAudioStream,
                    int /* stream_id */)

// Pause the audio stream specified by (render_view_id, stream_id).
IPC_MESSAGE_ROUTED1(ViewHostMsg_PauseAudioStream,
                    int /* stream_id */)

// Discard all buffered audio data for the specified audio stream.
IPC_MESSAGE_ROUTED1(ViewHostMsg_FlushAudioStream,
                    int /* stream_id */)

// Close an audio stream specified by (render_view_id, stream_id).
IPC_MESSAGE_ROUTED1(ViewHostMsg_CloseAudioStream,
                    int /* stream_id */)

// Get audio volume of the stream specified by (render_view_id, stream_id).
IPC_MESSAGE_ROUTED1(ViewHostMsg_GetAudioVolume,
                    int /* stream_id */)

// Set audio volume of the stream specified by (render_view_id, stream_id).
// TODO(hclam): change this to vector if we have channel numbers other than 2.
IPC_MESSAGE_ROUTED2(ViewHostMsg_SetAudioVolume,
                    int /* stream_id */,
                    double /* volume */)

// A renderer sends this message when an extension process starts an API
// request. The browser will always respond with a ViewMsg_ExtensionResponse.
IPC_MESSAGE_ROUTED1(ViewHostMsg_ExtensionRequest,
                    ViewHostMsg_DomMessage_Params)

// Notify the browser that the given extension added a listener to an event.
IPC_MESSAGE_CONTROL2(ViewHostMsg_ExtensionAddListener,
                     std::string /* extension_id */,
                     std::string /* name */)

// Notify the browser that the given extension removed a listener from an
// event.
IPC_MESSAGE_CONTROL2(ViewHostMsg_ExtensionRemoveListener,
                     std::string /* extension_id */,
                     std::string /* name */)

// Message sent from renderer to the browser to update the state of a command.
// The |command| parameter is a RenderViewCommand. The |checked_state| parameter
// is a CommandCheckedState.
IPC_MESSAGE_ROUTED3(ViewHostMsg_CommandStateChanged,
                    int /* command */,
                    bool /* is_enabled */,
                    int /* checked_state */)

#if defined(OS_MACOSX)
// On OSX, we cannot allocated shared memory from within the sandbox, so
// this call exists for the renderer to ask the browser to allocate memory
// on its behalf. We return a file descriptor to the POSIX shared memory.
// If the |cache_in_browser| flag is |true|, then a copy of the shmem is kept
// by the browser, and it is the caller's repsonsibility to send a
// ViewHostMsg_FreeTransportDIB message in order to release the cached shmem.
// In all cases, the caller is responsible for deleting the resulting
// TransportDIB.
IPC_SYNC_MESSAGE_CONTROL2_1(ViewHostMsg_AllocTransportDIB,
                            size_t, /* bytes requested */
                            bool, /* cache in the browser */
                            TransportDIB::Handle /* DIB */)

// Since the browser keeps handles to the allocated transport DIBs, this
// message is sent to tell the browser that it may release them when the
// renderer is finished with them.
IPC_MESSAGE_CONTROL1(ViewHostMsg_FreeTransportDIB,
                     TransportDIB::Id /* DIB id */)

// Informs the browser that a plugin has gained or lost focus.
IPC_MESSAGE_ROUTED2(ViewHostMsg_PluginFocusChanged,
                    bool, /* focused */
                    int /* plugin_id */)

// Instructs the browser to start plugin IME.
IPC_MESSAGE_ROUTED0(ViewHostMsg_StartPluginIme)

//---------------------------------------------------------------------------
// Messages related to accelerated plugins

// This is sent from the renderer to the browser to allocate a fake
// PluginWindowHandle on the browser side which is used to identify
// the plugin to the browser later when backing store is allocated
// or reallocated. |opaque| indicates whether the plugin's output is
// considered to be opaque, as opposed to translucent. This message
// is reused for rendering the accelerated compositor's output.
// |root| indicates whether the output is supposed to cover the
// entire window.
IPC_SYNC_MESSAGE_ROUTED2_1(ViewHostMsg_AllocateFakePluginWindowHandle,
                           bool /* opaque */,
                           bool /* root */,
                           gfx::PluginWindowHandle /* id */)

// Destroys a fake window handle previously allocated using
// AllocateFakePluginWindowHandle.
IPC_MESSAGE_ROUTED1(ViewHostMsg_DestroyFakePluginWindowHandle,
                    gfx::PluginWindowHandle /* id */)

// This message, used on Mac OS X 10.5 and earlier (no IOSurface support),
// is sent from the renderer to the browser on behalf of the plug-in
// to indicate that a new backing store was allocated for that plug-in
// instance.
IPC_MESSAGE_ROUTED4(ViewHostMsg_AcceleratedSurfaceSetTransportDIB,
                    gfx::PluginWindowHandle /* window */,
                    int32 /* width */,
                    int32 /* height */,
                    TransportDIB::Handle /* handle for the DIB */)

// This message, used on Mac OS X 10.6 and later (where IOSurface is
// supported), is sent from the renderer to the browser on behalf of the
// plug-in to indicate that a new backing store was allocated for that
// plug-in instance.
//
// NOTE: the original intent was to pass a mach port as the IOSurface
// identifier but it looks like that will be a lot of work. For now we pass an
// ID from IOSurfaceGetID.
IPC_MESSAGE_ROUTED4(ViewHostMsg_AcceleratedSurfaceSetIOSurface,
                    gfx::PluginWindowHandle /* window */,
                    int32 /* width */,
                    int32 /* height */,
                    uint64 /* surface_id */)

// This message notifies the browser process that the plug-in
// swapped the buffers associated with the given "window", which
// should cause the browser to redraw the various plug-ins'
// contents.
IPC_MESSAGE_ROUTED2(ViewHostMsg_AcceleratedSurfaceBuffersSwapped,
                    gfx::PluginWindowHandle /* window */,
                    uint64 /* surface_id */)
#endif

// A renderer sends this to the browser process when it wants to create a
// worker.  The browser will create the worker process if necessary, and
// will return the route id on success.  On error returns MSG_ROUTING_NONE.
IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_CreateWorker,
                            ViewHostMsg_CreateWorker_Params,
                            int /* route_id */)

// This message is sent to the browser to see if an instance of this shared
// worker already exists. If so, it returns exists == true. If a
// non-empty name is passed, also validates that the url matches the url of
// the existing worker. If a matching worker is found, the passed-in
// document_id is associated with that worker, to ensure that the worker
// stays alive until the document is detached.
// The route_id returned can be used to forward messages to the worker via
// ForwardToWorker if it exists, otherwise it should be passed in to any
// future call to CreateWorker to avoid creating duplicate workers.
IPC_SYNC_MESSAGE_CONTROL1_3(ViewHostMsg_LookupSharedWorker,
                            ViewHostMsg_CreateWorker_Params,
                            bool /* exists */,
                            int /* route_id */,
                            bool /* url_mismatch */)

// A renderer sends this to the browser process when a document has been
// detached. The browser will use this to constrain the lifecycle of worker
// processes (SharedWorkers are shut down when their last associated document
// is detached).
IPC_MESSAGE_CONTROL1(ViewHostMsg_DocumentDetached,
                     uint64 /* document_id */)

// A message sent to the browser on behalf of a renderer which wants to show
// a desktop notification.
IPC_MESSAGE_ROUTED1(ViewHostMsg_ShowDesktopNotification,
                    ViewHostMsg_ShowNotification_Params)
IPC_MESSAGE_ROUTED1(ViewHostMsg_CancelDesktopNotification,
                    int /* notification_id */)
IPC_MESSAGE_ROUTED2(ViewHostMsg_RequestNotificationPermission,
                    GURL /* origin */,
                    int /* callback_context */)
IPC_SYNC_MESSAGE_ROUTED1_1(ViewHostMsg_CheckNotificationPermission,
                           GURL /* source page */,
                           int /* permission_result */)

// Sent if the worker object has sent a ViewHostMsg_CreateDedicatedWorker
// message and not received a ViewMsg_WorkerCreated reply, but in the
// mean time it's destroyed.  This tells the browser to not create the queued
// worker.
IPC_MESSAGE_CONTROL1(ViewHostMsg_CancelCreateDedicatedWorker,
                     int /* route_id */)

// Wraps an IPC message that's destined to the worker on the renderer->browser
// hop.
IPC_MESSAGE_CONTROL1(ViewHostMsg_ForwardToWorker,
                     IPC::Message /* message */)

// Open a channel to all listening contexts owned by the extension with
// the given ID.  This always returns a valid port ID which can be used for
// sending messages.  If an error occurred, the opener will be notified
// asynchronously.
IPC_SYNC_MESSAGE_CONTROL4_1(ViewHostMsg_OpenChannelToExtension,
                            int /* routing_id */,
                            std::string /* source_extension_id */,
                            std::string /* target_extension_id */,
                            std::string /* channel_name */,
                            int /* port_id */)

// Get a port handle to the given tab.  The handle can be used for sending
// messages to the extension.
IPC_SYNC_MESSAGE_CONTROL4_1(ViewHostMsg_OpenChannelToTab,
                            int /* routing_id */,
                            int /* tab_id */,
                            std::string /* extension_id */,
                            std::string /* channel_name */,
                            int /* port_id */)

// Send a message to an extension process.  The handle is the value returned
// by ViewHostMsg_OpenChannelTo*.
IPC_MESSAGE_ROUTED2(ViewHostMsg_ExtensionPostMessage,
                    int /* port_id */,
                    std::string /* message */)

// Send a message to an extension process.  The handle is the value returned
// by ViewHostMsg_OpenChannelTo*.
IPC_MESSAGE_CONTROL1(ViewHostMsg_ExtensionCloseChannel,
                     int /* port_id */)

// Sent to notify the browser about renderer accessibility notifications.
// The browser responds with a ViewMsg_AccessibilityNotifications_ACK.
IPC_MESSAGE_ROUTED1(
    ViewHostMsg_AccessibilityNotifications,
    std::vector<ViewHostMsg_AccessibilityNotification_Params>)

// Send part of the DOM to the browser, to be used in a malware report.
IPC_MESSAGE_ROUTED1(ViewHostMsg_MalwareDOMDetails,
                    ViewHostMsg_MalwareDOMDetails_Params)

// Message sent from the renderer to the browser to request that the browser
// close all sockets.  Used for debugging/testing.
IPC_MESSAGE_CONTROL0(ViewHostMsg_CloseCurrentConnections)

// Message sent from the renderer to the browser to request that the browser
// enable or disable the cache.  Used for debugging/testing.
IPC_MESSAGE_CONTROL1(ViewHostMsg_SetCacheMode,
                     bool /* enabled */)

// Message sent from the renderer to the browser to request that the browser
// clear the cache.  Used for debugging/testing.
// |preserve_ssl_host_info| controls whether clearing the cache will preserve
// persisted SSL information stored in the cache.
// |result| is the returned status from the operation.
IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_ClearCache,
                            bool /* preserve_ssl_host_info */,
                            int  /* result */)

// Message sent from the renderer to the browser to request that the browser
// enable or disable spdy.  Used for debugging/testing/benchmarking.
IPC_MESSAGE_CONTROL1(ViewHostMsg_EnableSpdy,
                     bool /* enable */)

// Message sent from the renderer to the browser to request that the browser
// cache |data| associated with |url|.
IPC_MESSAGE_CONTROL3(ViewHostMsg_DidGenerateCacheableMetadata,
                     GURL /* url */,
                     double /* expected_response_time */,
                     std::vector<char> /* data */)

// Opens a file asynchronously. The response returns a file descriptor
// and an error code from base/platform_file.h.
IPC_MESSAGE_ROUTED3(ViewHostMsg_AsyncOpenFile,
                    FilePath /* file path */,
                    int /* flags */,
                    int /* message_id */)

// Sent by the renderer process to acknowledge receipt of a
// ViewMsg_CSSInsertRequest message and css has been inserted into the frame.
IPC_MESSAGE_ROUTED0(ViewHostMsg_OnCSSInserted)

// Notifies the browser of the language (ISO 639_1 code language, such as fr,
// en, zh...) of the current page.
IPC_MESSAGE_ROUTED1(ViewHostMsg_PageLanguageDetermined,
                    std::string /* the language */)

// Notifies the browser that a page has been translated.
IPC_MESSAGE_ROUTED4(ViewHostMsg_PageTranslated,
                    int,                  /* page id */
                    std::string           /* the original language */,
                    std::string           /* the translated language */,
                    TranslateErrors::Type /* the error type if available */)

//---------------------------------------------------------------------------
// Request for cryptographic operation messages:
// These are messages from the renderer to the browser to perform a
// cryptographic operation.

// Asks the browser process to generate a keypair for grabbing a client
// certificate from a CA (<keygen> tag), and returns the signed public
// key and challenge string.
IPC_SYNC_MESSAGE_CONTROL3_1(ViewHostMsg_Keygen,
                            uint32 /* key size index */,
                            std::string /* challenge string */,
                            GURL /* URL of requestor */,
                            std::string /* signed public key and challenge */)

// The renderer has tried to spell check a word, but couldn't because no
// dictionary was available to load. Request that the browser find an
// appropriate dictionary and return it.
IPC_MESSAGE_CONTROL0(ViewHostMsg_SpellChecker_RequestDictionary)

IPC_SYNC_MESSAGE_CONTROL2_1(ViewHostMsg_SpellChecker_PlatformCheckSpelling,
                            string16 /* word */,
                            int /* document tag */,
                            bool /* correct */)

IPC_SYNC_MESSAGE_CONTROL1_1(
    ViewHostMsg_SpellChecker_PlatformFillSuggestionList,
    string16 /* word */,
    std::vector<string16> /* suggestions */)

IPC_MESSAGE_CONTROL4(ViewHostMsg_SpellChecker_PlatformRequestTextCheck,
                     int /* route_id for response */,
                     int /* request identifier given by WebKit */,
                     int /* document tag */,
                     string16 /* sentence */)

//---------------------------------------------------------------------------
// Geolocation services messages

// The |render_view_id| and |bridge_id| representing |host| is requesting
// permission to access geolocation position.
// This will be replied by ViewMsg_Geolocation_PermissionSet.
IPC_MESSAGE_CONTROL3(ViewHostMsg_Geolocation_RequestPermission,
                     int /* render_view_id */,
                     int /* bridge_id */,
                     GURL /* GURL of the frame requesting geolocation */)

// The |render_view_id| and |bridge_id| representing |GURL| is cancelling its
// previous permission request to access geolocation position.
IPC_MESSAGE_CONTROL3(ViewHostMsg_Geolocation_CancelPermissionRequest,
                     int /* render_view_id */,
                     int /* bridge_id */,
                     GURL /* GURL of the frame */)

// The |render_view_id| requests Geolocation service to start updating.
// This is an asynchronous call, and the browser process may eventually reply
// with the updated geoposition, or an error (access denied, location
// unavailable, etc.)
IPC_MESSAGE_CONTROL3(ViewHostMsg_Geolocation_StartUpdating,
                     int /* render_view_id */,
                     GURL /* GURL of the frame requesting geolocation */,
                     bool /* enable_high_accuracy */)

// The |render_view_id| requests Geolocation service to stop updating.
// Note that the geolocation service may continue to fetch geolocation data
// for other origins.
IPC_MESSAGE_CONTROL1(ViewHostMsg_Geolocation_StopUpdating,
                     int /* render_view_id */)

// Updates the minimum/maximum allowed zoom percent for this tab from the
// default values.  If |remember| is true, then the zoom setting is applied to
// other pages in the site and is saved, otherwise it only applies to this
// tab.
IPC_MESSAGE_ROUTED3(ViewHostMsg_UpdateZoomLimits,
                    int /* minimum_percent */,
                    int /* maximum_percent */,
                    bool /* remember */)

//---------------------------------------------------------------------------
// Device orientation services messages:

// A RenderView requests to start receiving device orientation updates.
IPC_MESSAGE_CONTROL1(ViewHostMsg_DeviceOrientation_StartUpdating,
                     int /* render_view_id */)

// A RenderView requests to stop receiving device orientation updates.
IPC_MESSAGE_CONTROL1(ViewHostMsg_DeviceOrientation_StopUpdating,
                     int /* render_view_id */)

//---------------------------------------------------------------------------
// Blob messages:

// Registers a blob URL referring to the specified blob data.
IPC_MESSAGE_CONTROL2(ViewHostMsg_RegisterBlobUrl,
                     GURL /* url */,
                     scoped_refptr<webkit_blob::BlobData> /* blob_data */)

// Registers a blob URL referring to the blob data identified by the specified
// source URL.
IPC_MESSAGE_CONTROL2(ViewHostMsg_RegisterBlobUrlFrom,
                     GURL /* url */,
                     GURL /* src_url */)

// Unregister a blob URL.
IPC_MESSAGE_CONTROL1(ViewHostMsg_UnregisterBlobUrl, GURL /* url */)

// Suggest results -----------------------------------------------------------

IPC_MESSAGE_ROUTED3(ViewHostMsg_SetSuggestions,
                    int32 /* page_id */,
                    std::vector<std::string> /* suggestions */,
                    InstantCompleteBehavior)

IPC_MESSAGE_ROUTED2(ViewHostMsg_InstantSupportDetermined,
                    int32 /* page_id */,
                    bool  /* result */)

// Response from ViewMsg_ScriptEvalRequest. The ID is the parameter supplied
// to ViewMsg_ScriptEvalRequest. The result has the value returned by the
// script as it's only element, one of Null, Boolean, Integer, Real, Date, or
// String.
IPC_MESSAGE_ROUTED2(ViewHostMsg_ScriptEvalResponse,
                    int  /* id */,
                    ListValue  /* result */)

// Updates the content restrictions, i.e. to disable print/copy.
IPC_MESSAGE_ROUTED1(ViewHostMsg_UpdateContentRestrictions,
                    int /* restrictions */)

// The currently displayed PDF has an unsupported feature.
IPC_MESSAGE_ROUTED0(ViewHostMsg_PDFHasUnsupportedFeature)

// JavaScript related messages -----------------------------------------------

// Notify the JavaScript engine in the render to change its parameters
// while performing stress testing.
IPC_MESSAGE_ROUTED2(ViewMsg_JavaScriptStressTestControl,
                    int /* cmd */,
                    int /* param */)

// Register a new handler for URL requests with the given scheme.
IPC_MESSAGE_ROUTED3(ViewHostMsg_RegisterProtocolHandler,
                    std::string /* scheme */,
                    GURL /* url */,
                    string16 /* title */)
