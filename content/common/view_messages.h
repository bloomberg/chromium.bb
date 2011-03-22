// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for page rendering.
// Multiply-included message file, hence no include guard.

#include "content/common/common_param_traits.h"
#include "content/common/css_colors.h"
#include "content/common/edit_command.h"
#include "content/common/page_transition_types.h"
#include "content/common/page_zoom.h"
#include "content/common/renderer_preferences.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFindOptions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaPlayerAction.h"
#include "ui/gfx/rect.h"
#include "webkit/glue/context_menu.h"
#include "webkit/glue/webmenuitem.h"
#include "webkit/glue/webpreferences.h"

// Define enums used in this file inside an include-guard.
#ifndef CONTENT_COMMON_VIEW_MESSAGES_H_
#define CONTENT_COMMON_VIEW_MESSAGES_H_

class ViewMsg_Navigate_Type {
 public:
  enum Value {
    // Reload the page.
    RELOAD,

    // Reload the page, ignoring any cache entries.
    RELOAD_IGNORING_CACHE,

    // The navigation is the result of session restore and should honor the
    // page's cache policy while restoring form state. This is set to true if
    // restoring a tab/session from the previous session and the previous
    // session did not crash. If this is not set and the page was restored then
    // the page's cache policy is ignored and we load from the cache.
    RESTORE,

    // Speculatively prerendering the page.
    PRERENDER,

    // Navigation type not categorized by the other types.
    NORMAL
  };
};

// The user has completed a find-in-page; this type defines what actions the
// renderer should take next.
struct ViewMsg_StopFinding_Params {
  enum Action {
    kClearSelection,
    kKeepSelection,
    kActivateSelection
  };

  ViewMsg_StopFinding_Params() : action(kClearSelection) {}

  // The action that should be taken when the find is completed.
  Action action;
};

#endif  // CONTENT_COMMON_VIEW_MESSAGES_H_

#define IPC_MESSAGE_START ViewMsgStart

IPC_ENUM_TRAITS(CSSColors::CSSColorName)
IPC_ENUM_TRAITS(PageZoom::Function)
IPC_ENUM_TRAITS(RendererPreferencesHintingEnum)
IPC_ENUM_TRAITS(RendererPreferencesSubpixelRenderingEnum)
IPC_ENUM_TRAITS(ViewMsg_Navigate_Type::Value)
IPC_ENUM_TRAITS(ViewMsg_StopFinding_Params::Action)
IPC_ENUM_TRAITS(WebKit::WebContextMenuData::MediaType)
IPC_ENUM_TRAITS(WebKit::WebMediaPlayerAction::Type)
IPC_ENUM_TRAITS(WebMenuItem::Type)

IPC_STRUCT_TRAITS_BEGIN(EditCommand)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(value)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(RendererPreferences)
  IPC_STRUCT_TRAITS_MEMBER(can_accept_load_drops)
  IPC_STRUCT_TRAITS_MEMBER(should_antialias_text)
  IPC_STRUCT_TRAITS_MEMBER(hinting)
  IPC_STRUCT_TRAITS_MEMBER(subpixel_rendering)
  IPC_STRUCT_TRAITS_MEMBER(focus_ring_color)
  IPC_STRUCT_TRAITS_MEMBER(thumb_active_color)
  IPC_STRUCT_TRAITS_MEMBER(thumb_inactive_color)
  IPC_STRUCT_TRAITS_MEMBER(track_color)
  IPC_STRUCT_TRAITS_MEMBER(active_selection_bg_color)
  IPC_STRUCT_TRAITS_MEMBER(active_selection_fg_color)
  IPC_STRUCT_TRAITS_MEMBER(inactive_selection_bg_color)
  IPC_STRUCT_TRAITS_MEMBER(inactive_selection_fg_color)
  IPC_STRUCT_TRAITS_MEMBER(browser_handles_top_level_requests)
  IPC_STRUCT_TRAITS_MEMBER(caret_blink_interval)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ViewMsg_StopFinding_Params)
  IPC_STRUCT_TRAITS_MEMBER(action)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(WebKit::WebFindOptions)
  IPC_STRUCT_TRAITS_MEMBER(forward)
  IPC_STRUCT_TRAITS_MEMBER(matchCase)
  IPC_STRUCT_TRAITS_MEMBER(findNext)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(WebKit::WebMediaPlayerAction)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(enable)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(WebPreferences)
  IPC_STRUCT_TRAITS_MEMBER(standard_font_family)
  IPC_STRUCT_TRAITS_MEMBER(fixed_font_family)
  IPC_STRUCT_TRAITS_MEMBER(serif_font_family)
  IPC_STRUCT_TRAITS_MEMBER(sans_serif_font_family)
  IPC_STRUCT_TRAITS_MEMBER(cursive_font_family)
  IPC_STRUCT_TRAITS_MEMBER(fantasy_font_family)
  IPC_STRUCT_TRAITS_MEMBER(default_font_size)
  IPC_STRUCT_TRAITS_MEMBER(default_fixed_font_size)
  IPC_STRUCT_TRAITS_MEMBER(minimum_font_size)
  IPC_STRUCT_TRAITS_MEMBER(minimum_logical_font_size)
  IPC_STRUCT_TRAITS_MEMBER(default_encoding)
  IPC_STRUCT_TRAITS_MEMBER(javascript_enabled)
  IPC_STRUCT_TRAITS_MEMBER(web_security_enabled)
  IPC_STRUCT_TRAITS_MEMBER(javascript_can_open_windows_automatically)
  IPC_STRUCT_TRAITS_MEMBER(loads_images_automatically)
  IPC_STRUCT_TRAITS_MEMBER(plugins_enabled)
  IPC_STRUCT_TRAITS_MEMBER(dom_paste_enabled)
  IPC_STRUCT_TRAITS_MEMBER(developer_extras_enabled)
  IPC_STRUCT_TRAITS_MEMBER(inspector_settings)
  IPC_STRUCT_TRAITS_MEMBER(site_specific_quirks_enabled)
  IPC_STRUCT_TRAITS_MEMBER(shrinks_standalone_images_to_fit)
  IPC_STRUCT_TRAITS_MEMBER(uses_universal_detector)
  IPC_STRUCT_TRAITS_MEMBER(text_areas_are_resizable)
  IPC_STRUCT_TRAITS_MEMBER(java_enabled)
  IPC_STRUCT_TRAITS_MEMBER(allow_scripts_to_close_windows)
  IPC_STRUCT_TRAITS_MEMBER(uses_page_cache)
  IPC_STRUCT_TRAITS_MEMBER(remote_fonts_enabled)
  IPC_STRUCT_TRAITS_MEMBER(javascript_can_access_clipboard)
  IPC_STRUCT_TRAITS_MEMBER(xss_auditor_enabled)
  IPC_STRUCT_TRAITS_MEMBER(local_storage_enabled)
  IPC_STRUCT_TRAITS_MEMBER(databases_enabled)
  IPC_STRUCT_TRAITS_MEMBER(application_cache_enabled)
  IPC_STRUCT_TRAITS_MEMBER(tabs_to_links)
  IPC_STRUCT_TRAITS_MEMBER(hyperlink_auditing_enabled)
  IPC_STRUCT_TRAITS_MEMBER(user_style_sheet_enabled)
  IPC_STRUCT_TRAITS_MEMBER(user_style_sheet_location)
  IPC_STRUCT_TRAITS_MEMBER(author_and_user_styles_enabled)
  IPC_STRUCT_TRAITS_MEMBER(frame_flattening_enabled)
  IPC_STRUCT_TRAITS_MEMBER(allow_universal_access_from_file_urls)
  IPC_STRUCT_TRAITS_MEMBER(allow_file_access_from_file_urls)
  IPC_STRUCT_TRAITS_MEMBER(webaudio_enabled)
  IPC_STRUCT_TRAITS_MEMBER(experimental_webgl_enabled)
  IPC_STRUCT_TRAITS_MEMBER(gl_multisampling_enabled)
  IPC_STRUCT_TRAITS_MEMBER(show_composited_layer_borders)
  IPC_STRUCT_TRAITS_MEMBER(show_composited_layer_tree)
  IPC_STRUCT_TRAITS_MEMBER(show_fps_counter)
  IPC_STRUCT_TRAITS_MEMBER(accelerated_compositing_enabled)
  IPC_STRUCT_TRAITS_MEMBER(force_compositing_mode)
  IPC_STRUCT_TRAITS_MEMBER(composite_to_texture_enabled)
  IPC_STRUCT_TRAITS_MEMBER(accelerated_2d_canvas_enabled)
  IPC_STRUCT_TRAITS_MEMBER(accelerated_plugins_enabled)
  IPC_STRUCT_TRAITS_MEMBER(accelerated_layers_enabled)
  IPC_STRUCT_TRAITS_MEMBER(accelerated_video_enabled)
  IPC_STRUCT_TRAITS_MEMBER(memory_info_enabled)
  IPC_STRUCT_TRAITS_MEMBER(interactive_form_validation_enabled)
  IPC_STRUCT_TRAITS_MEMBER(fullscreen_enabled)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(WebMenuItem)
  IPC_STRUCT_TRAITS_MEMBER(label)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(action)
  IPC_STRUCT_TRAITS_MEMBER(rtl)
  IPC_STRUCT_TRAITS_MEMBER(has_directional_override)
  IPC_STRUCT_TRAITS_MEMBER(enabled)
  IPC_STRUCT_TRAITS_MEMBER(checked)
  IPC_STRUCT_TRAITS_MEMBER(submenu)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(webkit_glue::CustomContextMenuContext)
  IPC_STRUCT_TRAITS_MEMBER(is_pepper_menu)
  IPC_STRUCT_TRAITS_MEMBER(request_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ContextMenuParams)
  IPC_STRUCT_TRAITS_MEMBER(media_type)
  IPC_STRUCT_TRAITS_MEMBER(x)
  IPC_STRUCT_TRAITS_MEMBER(y)
  IPC_STRUCT_TRAITS_MEMBER(link_url)
  IPC_STRUCT_TRAITS_MEMBER(unfiltered_link_url)
  IPC_STRUCT_TRAITS_MEMBER(src_url)
  IPC_STRUCT_TRAITS_MEMBER(is_image_blocked)
  IPC_STRUCT_TRAITS_MEMBER(page_url)
  IPC_STRUCT_TRAITS_MEMBER(frame_url)
  IPC_STRUCT_TRAITS_MEMBER(frame_content_state)
  IPC_STRUCT_TRAITS_MEMBER(media_flags)
  IPC_STRUCT_TRAITS_MEMBER(selection_text)
  IPC_STRUCT_TRAITS_MEMBER(misspelled_word)
  IPC_STRUCT_TRAITS_MEMBER(dictionary_suggestions)
  IPC_STRUCT_TRAITS_MEMBER(spellcheck_enabled)
  IPC_STRUCT_TRAITS_MEMBER(is_editable)
#if defined(OS_MACOSX)
  IPC_STRUCT_TRAITS_MEMBER(writing_direction_default)
  IPC_STRUCT_TRAITS_MEMBER(writing_direction_left_to_right)
  IPC_STRUCT_TRAITS_MEMBER(writing_direction_right_to_left)
#endif  // OS_MACOSX
  IPC_STRUCT_TRAITS_MEMBER(edit_flags)
  IPC_STRUCT_TRAITS_MEMBER(security_info)
  IPC_STRUCT_TRAITS_MEMBER(frame_charset)
  IPC_STRUCT_TRAITS_MEMBER(custom_context)
  IPC_STRUCT_TRAITS_MEMBER(custom_items)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_BEGIN(ViewMsg_Navigate_Params)
  // The page_id for this navigation, or -1 if it is a new navigation.  Back,
  // Forward, and Reload navigations should have a valid page_id.  If the load
  // succeeds, then this page_id will be reflected in the resultant
  // ViewHostMsg_FrameNavigate message.
  IPC_STRUCT_MEMBER(int32, page_id)

  // If page_id is -1, then pending_history_list_offset will also be -1.
  // Otherwise, it contains the offset into the history list corresponding to
  // the current navigation.
  IPC_STRUCT_MEMBER(int, pending_history_list_offset)

  // Informs the RenderView of where its current page contents reside in
  // session history and the total size of the session history list.
  IPC_STRUCT_MEMBER(int, current_history_list_offset)
  IPC_STRUCT_MEMBER(int, current_history_list_length)

  // The URL to load.
  IPC_STRUCT_MEMBER(GURL, url)

  // The URL to send in the "Referer" header field. Can be empty if there is
  // no referrer.
  // TODO: consider folding this into extra_headers.
  IPC_STRUCT_MEMBER(GURL, referrer)

  // The type of transition.
  IPC_STRUCT_MEMBER(PageTransition::Type, transition)

  // Opaque history state (received by ViewHostMsg_UpdateState).
  IPC_STRUCT_MEMBER(std::string, state)

  // Type of navigation.
  IPC_STRUCT_MEMBER(ViewMsg_Navigate_Type::Value, navigation_type)

  // The time the request was created
  IPC_STRUCT_MEMBER(base::Time, request_time)

  // Extra headers (separated by \n) to send during the request.
  IPC_STRUCT_MEMBER(std::string, extra_headers)
IPC_STRUCT_END()

// This message is used for supporting popup menus on Mac OS X using native
// Cocoa controls. The renderer sends us this message which we use to populate
// the popup menu.
IPC_STRUCT_BEGIN(ViewHostMsg_ShowPopup_Params)
  // Position on the screen.
  IPC_STRUCT_MEMBER(gfx::Rect, bounds)

  // The height of each item in the menu.
  IPC_STRUCT_MEMBER(int, item_height)

  // The size of the font to use for those items.
  IPC_STRUCT_MEMBER(double, item_font_size)

  // The currently selected (displayed) item in the menu.
  IPC_STRUCT_MEMBER(int, selected_item)

  // The entire list of items in the popup menu.
  IPC_STRUCT_MEMBER(std::vector<WebMenuItem>, popup_items)

  // Whether items should be right-aligned.
  IPC_STRUCT_MEMBER(bool, right_aligned)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(ViewMsg_New_Params)
  // The parent window's id.
  IPC_STRUCT_MEMBER(gfx::NativeViewId, parent_window)

  // Renderer-wide preferences.
  IPC_STRUCT_MEMBER(RendererPreferences, renderer_preferences)

  // Preferences for this view.
  IPC_STRUCT_MEMBER(WebPreferences, web_preferences)

  // The ID of the view to be created.
  IPC_STRUCT_MEMBER(int32, view_id)

  // The session storage namespace ID this view should use.
  IPC_STRUCT_MEMBER(int64, session_storage_namespace_id)

  // The name of the frame associated with this view (or empty if none).
  IPC_STRUCT_MEMBER(string16, frame_name)
IPC_STRUCT_END()

// Messages sent from the browser to the renderer.

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

// Reply in response to ViewHostMsg_ShowView or ViewHostMsg_ShowWidget.
// similar to the new command, but used when the renderer created a view
// first, and we need to update it.
IPC_MESSAGE_ROUTED1(ViewMsg_CreatingNew_ACK,
                    gfx::NativeViewId /* parent_hwnd */)

// Sends updated preferences to the renderer.
IPC_MESSAGE_ROUTED1(ViewMsg_SetRendererPrefs,
                    RendererPreferences)

// This passes a set of webkit preferences down to the renderer.
IPC_MESSAGE_ROUTED1(ViewMsg_UpdateWebPreferences,
                    WebPreferences)

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
IPC_MESSAGE_ROUTED1(ViewMsg_SetFocus,
                    bool /* enable */)

// Tells the renderer to focus the first (last if reverse is true) focusable
// node.
IPC_MESSAGE_ROUTED1(ViewMsg_SetInitialFocus,
                    bool /* reverse */)

// Tells the renderer to scroll the currently focused node into view only if
// the currently focused node is a Text node (textfield, text area or content
// editable divs).
IPC_MESSAGE_ROUTED0(ViewMsg_ScrollFocusedEditableNodeIntoView)

// Executes custom context menu action that was provided from WebKit.
IPC_MESSAGE_ROUTED2(ViewMsg_CustomContextMenuAction,
                    webkit_glue::CustomContextMenuContext /* custom_context */,
                    unsigned /* action */)

// Sent in response to a ViewHostMsg_ContextMenu to let the renderer know that
// the menu has been closed.
IPC_MESSAGE_ROUTED1(ViewMsg_ContextMenuClosed,
                    webkit_glue::CustomContextMenuContext /* custom_context */)

// Tells the renderer to perform the specified navigation, interrupting any
// existing navigation.
IPC_MESSAGE_ROUTED1(ViewMsg_Navigate, ViewMsg_Navigate_Params)

IPC_MESSAGE_ROUTED0(ViewMsg_Stop)

// Tells the renderer to reload the current focused frame
IPC_MESSAGE_ROUTED0(ViewMsg_ReloadFrame)

// Sent when the user wants to search for a word on the page (find in page).
IPC_MESSAGE_ROUTED3(ViewMsg_Find,
                    int /* request_id */,
                    string16 /* search_text */,
                    WebKit::WebFindOptions)

// This message notifies the renderer that the user has closed the FindInPage
// window (and what action to take regarding the selection).
IPC_MESSAGE_ROUTED1(ViewMsg_StopFinding,
                    ViewMsg_StopFinding_Params /* action */)

// Used to notify the render-view that the browser has received a reply for
// the Find operation and is interested in receiving the next one. This is
// used to prevent the renderer from spamming the browser process with
// results.
IPC_MESSAGE_ROUTED0(ViewMsg_FindReplyACK)

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
IPC_MESSAGE_ROUTED1(ViewMsg_Replace,
                    string16)
IPC_MESSAGE_ROUTED0(ViewMsg_Delete)
IPC_MESSAGE_ROUTED0(ViewMsg_SelectAll)

// Copies the image at location x, y to the clipboard (if there indeed is an
// image at that location).
IPC_MESSAGE_ROUTED2(ViewMsg_CopyImageAt,
                    int /* x */,
                    int /* y */)

// Tells the renderer to perform the given action on the media player
// located at the given point.
IPC_MESSAGE_ROUTED2(ViewMsg_MediaPlayerActionAt,
                    gfx::Point, /* location */
                    WebKit::WebMediaPlayerAction)

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

// External popup menus.
IPC_MESSAGE_ROUTED1(ViewMsg_SelectPopupMenuItem,
                    int /* selected index, -1 means no selection */)

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



// Messages sent from the renderer to the browser.

// Used to tell the parent that the user right clicked on an area of the
// content area, and a context menu should be shown for it. The params
// object contains information about the node(s) that were selected when the
// user right clicked.
IPC_MESSAGE_ROUTED1(ViewHostMsg_ContextMenu, ContextMenuParams)

// Message to show a popup menu using native cocoa controls (Mac only).
IPC_MESSAGE_ROUTED1(ViewHostMsg_ShowPopup,
                    ViewHostMsg_ShowPopup_Params)

// Response from ViewMsg_ScriptEvalRequest. The ID is the parameter supplied
// to ViewMsg_ScriptEvalRequest. The result has the value returned by the
// script as it's only element, one of Null, Boolean, Integer, Real, Date, or
// String.
IPC_MESSAGE_ROUTED2(ViewHostMsg_ScriptEvalResponse,
                    int  /* id */,
                    ListValue  /* result */)

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
