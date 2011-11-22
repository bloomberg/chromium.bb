// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for page rendering.
// Multiply-included message file, hence no include guard.

#include "base/process.h"
#include "base/shared_memory.h"
#include "content/common/content_export.h"
#include "content/common/css_colors.h"
#include "content/common/edit_command.h"
#include "content/common/navigation_gesture.h"
#include "content/common/view_message_enums.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/file_chooser_params.h"
#include "content/public/common/frame_navigate_params.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/webkit_param_traits.h"
#include "content/public/common/window_container_type.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "media/base/media_log_event.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCompositionUnderline.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFindOptions.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMediaPlayerAction.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupType.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/range/range.h"
#include "ui/gfx/rect.h"
#include "webkit/glue/context_menu.h"
#include "webkit/glue/webcookie.h"
#include "webkit/glue/webmenuitem.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/webaccessibility.h"
#include "webkit/plugins/npapi/webplugin.h"

#if defined(OS_MACOSX)
#include "content/common/mac/font_descriptor.h"
#endif

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START ViewMsgStart

IPC_ENUM_TRAITS(CSSColors::CSSColorName)
IPC_ENUM_TRAITS(NavigationGesture)
IPC_ENUM_TRAITS(ViewHostMsg_AccEvent::Value)
IPC_ENUM_TRAITS(ViewMsg_Navigate_Type::Value)
IPC_ENUM_TRAITS(ViewMsg_StopFinding_Params::Action)
IPC_ENUM_TRAITS(WebKit::WebContextMenuData::MediaType)
IPC_ENUM_TRAITS(WebKit::WebMediaPlayerAction::Type)
IPC_ENUM_TRAITS(WebKit::WebPopupType)
IPC_ENUM_TRAITS(WebKit::WebTextDirection)
IPC_ENUM_TRAITS(WebMenuItem::Type)
IPC_ENUM_TRAITS(WindowContainerType)
IPC_ENUM_TRAITS(content::FileChooserParams::Mode)
IPC_ENUM_TRAITS(content::RendererPreferencesHintingEnum)
IPC_ENUM_TRAITS(content::RendererPreferencesSubpixelRenderingEnum)
IPC_ENUM_TRAITS(content::PageZoom)
IPC_ENUM_TRAITS(media::MediaLogEvent::Type)
IPC_ENUM_TRAITS(ui::TextInputType)
IPC_ENUM_TRAITS(webkit_glue::WebAccessibility::BoolAttribute)
IPC_ENUM_TRAITS(webkit_glue::WebAccessibility::FloatAttribute)
IPC_ENUM_TRAITS(webkit_glue::WebAccessibility::IntAttribute)
IPC_ENUM_TRAITS(webkit_glue::WebAccessibility::Role)
IPC_ENUM_TRAITS(webkit_glue::WebAccessibility::State)
IPC_ENUM_TRAITS(webkit_glue::WebAccessibility::StringAttribute)

IPC_STRUCT_TRAITS_BEGIN(ContextMenuParams)
  IPC_STRUCT_TRAITS_MEMBER(media_type)
  IPC_STRUCT_TRAITS_MEMBER(x)
  IPC_STRUCT_TRAITS_MEMBER(y)
  IPC_STRUCT_TRAITS_MEMBER(link_url)
  IPC_STRUCT_TRAITS_MEMBER(unfiltered_link_url)
  IPC_STRUCT_TRAITS_MEMBER(src_url)
  IPC_STRUCT_TRAITS_MEMBER(is_image_blocked)
  IPC_STRUCT_TRAITS_MEMBER(page_url)
  IPC_STRUCT_TRAITS_MEMBER(keyword_url)
  IPC_STRUCT_TRAITS_MEMBER(frame_url)
  IPC_STRUCT_TRAITS_MEMBER(frame_content_state)
  IPC_STRUCT_TRAITS_MEMBER(media_flags)
  IPC_STRUCT_TRAITS_MEMBER(selection_text)
  IPC_STRUCT_TRAITS_MEMBER(misspelled_word)
  IPC_STRUCT_TRAITS_MEMBER(dictionary_suggestions)
  IPC_STRUCT_TRAITS_MEMBER(speech_input_enabled)
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

IPC_STRUCT_TRAITS_BEGIN(EditCommand)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(value)
IPC_STRUCT_TRAITS_END()

#if defined(OS_MACOSX)
IPC_STRUCT_TRAITS_BEGIN(FontDescriptor)
  IPC_STRUCT_TRAITS_MEMBER(font_name)
  IPC_STRUCT_TRAITS_MEMBER(font_point_size)
IPC_STRUCT_TRAITS_END()
#endif

IPC_STRUCT_TRAITS_BEGIN(ViewMsg_StopFinding_Params)
  IPC_STRUCT_TRAITS_MEMBER(action)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(WebKit::WebCompositionUnderline)
  IPC_STRUCT_TRAITS_MEMBER(startOffset)
  IPC_STRUCT_TRAITS_MEMBER(endOffset)
  IPC_STRUCT_TRAITS_MEMBER(color)
  IPC_STRUCT_TRAITS_MEMBER(thick)
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

IPC_STRUCT_TRAITS_BEGIN(WebKit::WebRect)
  IPC_STRUCT_TRAITS_MEMBER(x)
  IPC_STRUCT_TRAITS_MEMBER(y)
  IPC_STRUCT_TRAITS_MEMBER(width)
  IPC_STRUCT_TRAITS_MEMBER(height)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(WebKit::WebScreenInfo)
  IPC_STRUCT_TRAITS_MEMBER(depth)
  IPC_STRUCT_TRAITS_MEMBER(depthPerComponent)
  IPC_STRUCT_TRAITS_MEMBER(isMonochrome)
  IPC_STRUCT_TRAITS_MEMBER(rect)
  IPC_STRUCT_TRAITS_MEMBER(availableRect)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(WebPreferences)
  IPC_STRUCT_TRAITS_MEMBER(standard_font_family)
  IPC_STRUCT_TRAITS_MEMBER(fixed_font_family)
  IPC_STRUCT_TRAITS_MEMBER(serif_font_family)
  IPC_STRUCT_TRAITS_MEMBER(sans_serif_font_family)
  IPC_STRUCT_TRAITS_MEMBER(cursive_font_family)
  IPC_STRUCT_TRAITS_MEMBER(fantasy_font_family)
  IPC_STRUCT_TRAITS_MEMBER(standard_font_family_map)
  IPC_STRUCT_TRAITS_MEMBER(fixed_font_family_map)
  IPC_STRUCT_TRAITS_MEMBER(serif_font_family_map)
  IPC_STRUCT_TRAITS_MEMBER(sans_serif_font_family_map)
  IPC_STRUCT_TRAITS_MEMBER(cursive_font_family_map)
  IPC_STRUCT_TRAITS_MEMBER(fantasy_font_family_map)
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
  IPC_STRUCT_TRAITS_MEMBER(dns_prefetching_enabled)
  IPC_STRUCT_TRAITS_MEMBER(local_storage_enabled)
  IPC_STRUCT_TRAITS_MEMBER(databases_enabled)
  IPC_STRUCT_TRAITS_MEMBER(application_cache_enabled)
  IPC_STRUCT_TRAITS_MEMBER(tabs_to_links)
  IPC_STRUCT_TRAITS_MEMBER(hyperlink_auditing_enabled)
  IPC_STRUCT_TRAITS_MEMBER(is_online)
  IPC_STRUCT_TRAITS_MEMBER(user_style_sheet_enabled)
  IPC_STRUCT_TRAITS_MEMBER(user_style_sheet_location)
  IPC_STRUCT_TRAITS_MEMBER(author_and_user_styles_enabled)
  IPC_STRUCT_TRAITS_MEMBER(frame_flattening_enabled)
  IPC_STRUCT_TRAITS_MEMBER(allow_universal_access_from_file_urls)
  IPC_STRUCT_TRAITS_MEMBER(allow_file_access_from_file_urls)
  IPC_STRUCT_TRAITS_MEMBER(webaudio_enabled)
  IPC_STRUCT_TRAITS_MEMBER(experimental_webgl_enabled)
  IPC_STRUCT_TRAITS_MEMBER(gl_multisampling_enabled)
  IPC_STRUCT_TRAITS_MEMBER(privileged_webgl_extensions_enabled)
  IPC_STRUCT_TRAITS_MEMBER(show_composited_layer_borders)
  IPC_STRUCT_TRAITS_MEMBER(show_composited_layer_tree)
  IPC_STRUCT_TRAITS_MEMBER(show_fps_counter)
  IPC_STRUCT_TRAITS_MEMBER(accelerated_compositing_enabled)
  IPC_STRUCT_TRAITS_MEMBER(threaded_compositing_enabled)
  IPC_STRUCT_TRAITS_MEMBER(force_compositing_mode)
  IPC_STRUCT_TRAITS_MEMBER(allow_webui_compositing)
  IPC_STRUCT_TRAITS_MEMBER(composite_to_texture_enabled)
  IPC_STRUCT_TRAITS_MEMBER(fixed_position_compositing_enabled)
  IPC_STRUCT_TRAITS_MEMBER(accelerated_2d_canvas_enabled)
  IPC_STRUCT_TRAITS_MEMBER(accelerated_drawing_enabled)
  IPC_STRUCT_TRAITS_MEMBER(accelerated_plugins_enabled)
  IPC_STRUCT_TRAITS_MEMBER(accelerated_layers_enabled)
  IPC_STRUCT_TRAITS_MEMBER(accelerated_video_enabled)
  IPC_STRUCT_TRAITS_MEMBER(memory_info_enabled)
  IPC_STRUCT_TRAITS_MEMBER(interactive_form_validation_enabled)
  IPC_STRUCT_TRAITS_MEMBER(fullscreen_enabled)
  IPC_STRUCT_TRAITS_MEMBER(allow_displaying_insecure_content)
  IPC_STRUCT_TRAITS_MEMBER(allow_running_insecure_content)
  IPC_STRUCT_TRAITS_MEMBER(enable_scroll_animator)
  IPC_STRUCT_TRAITS_MEMBER(visual_word_movement_enabled)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(WebMenuItem)
  IPC_STRUCT_TRAITS_MEMBER(label)
  IPC_STRUCT_TRAITS_MEMBER(toolTip)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(action)
  IPC_STRUCT_TRAITS_MEMBER(rtl)
  IPC_STRUCT_TRAITS_MEMBER(has_directional_override)
  IPC_STRUCT_TRAITS_MEMBER(enabled)
  IPC_STRUCT_TRAITS_MEMBER(checked)
  IPC_STRUCT_TRAITS_MEMBER(submenu)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::FileChooserParams)
  IPC_STRUCT_TRAITS_MEMBER(mode)
  IPC_STRUCT_TRAITS_MEMBER(title)
  IPC_STRUCT_TRAITS_MEMBER(default_file_name)
  IPC_STRUCT_TRAITS_MEMBER(accept_types)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::FrameNavigateParams)
  IPC_STRUCT_TRAITS_MEMBER(page_id)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(referrer)
  IPC_STRUCT_TRAITS_MEMBER(transition)
  IPC_STRUCT_TRAITS_MEMBER(redirects)
  IPC_STRUCT_TRAITS_MEMBER(should_update_history)
  IPC_STRUCT_TRAITS_MEMBER(searchable_form_url)
  IPC_STRUCT_TRAITS_MEMBER(searchable_form_encoding)
  IPC_STRUCT_TRAITS_MEMBER(password_form)
  IPC_STRUCT_TRAITS_MEMBER(contents_mime_type)
  IPC_STRUCT_TRAITS_MEMBER(socket_address)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::RendererPreferences)
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
  IPC_STRUCT_TRAITS_MEMBER(enable_referrers)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(webkit_glue::CustomContextMenuContext)
  IPC_STRUCT_TRAITS_MEMBER(is_pepper_menu)
  IPC_STRUCT_TRAITS_MEMBER(request_id)
  IPC_STRUCT_TRAITS_MEMBER(render_widget_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(webkit_glue::WebAccessibility)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(value)
  IPC_STRUCT_TRAITS_MEMBER(role)
  IPC_STRUCT_TRAITS_MEMBER(state)
  IPC_STRUCT_TRAITS_MEMBER(location)
  IPC_STRUCT_TRAITS_MEMBER(string_attributes)
  IPC_STRUCT_TRAITS_MEMBER(int_attributes)
  IPC_STRUCT_TRAITS_MEMBER(float_attributes)
  IPC_STRUCT_TRAITS_MEMBER(bool_attributes)
  IPC_STRUCT_TRAITS_MEMBER(children)
  IPC_STRUCT_TRAITS_MEMBER(indirect_child_ids)
  IPC_STRUCT_TRAITS_MEMBER(html_attributes)
  IPC_STRUCT_TRAITS_MEMBER(line_breaks)
  IPC_STRUCT_TRAITS_MEMBER(cell_ids)
  IPC_STRUCT_TRAITS_MEMBER(unique_cell_ids)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(webkit_glue::WebCookie)
  IPC_STRUCT_TRAITS_MEMBER(name)
  IPC_STRUCT_TRAITS_MEMBER(value)
  IPC_STRUCT_TRAITS_MEMBER(domain)
  IPC_STRUCT_TRAITS_MEMBER(path)
  IPC_STRUCT_TRAITS_MEMBER(expires)
  IPC_STRUCT_TRAITS_MEMBER(http_only)
  IPC_STRUCT_TRAITS_MEMBER(secure)
  IPC_STRUCT_TRAITS_MEMBER(session)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(webkit::npapi::WebPluginGeometry)
  IPC_STRUCT_TRAITS_MEMBER(window)
  IPC_STRUCT_TRAITS_MEMBER(window_rect)
  IPC_STRUCT_TRAITS_MEMBER(clip_rect)
  IPC_STRUCT_TRAITS_MEMBER(cutout_rects)
  IPC_STRUCT_TRAITS_MEMBER(rects_valid)
  IPC_STRUCT_TRAITS_MEMBER(visible)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::MediaLogEvent)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(params)
  IPC_STRUCT_TRAITS_MEMBER(time)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_BEGIN(ViewHostMsg_CreateWindow_Params)
  // Routing ID of the view initiating the open.
  IPC_STRUCT_MEMBER(int, opener_id)

  // True if this open request came in the context of a user gesture.
  IPC_STRUCT_MEMBER(bool, user_gesture)

  // Type of window requested.
  IPC_STRUCT_MEMBER(WindowContainerType, window_container_type)

  // The session storage namespace ID this view should use.
  IPC_STRUCT_MEMBER(int64, session_storage_namespace_id)

  // The name of the resulting frame that should be created (empty if none
  // has been specified).
  IPC_STRUCT_MEMBER(string16, frame_name)

  // The frame identifier of the frame initiating the open.
  IPC_STRUCT_MEMBER(int64, opener_frame_id)

  // The URL of the frame initiating the open.
  IPC_STRUCT_MEMBER(GURL, opener_url)

  // The security origin of the frame initiating the open.
  IPC_STRUCT_MEMBER(std::string, opener_security_origin)

  // The URL that will be loaded in the new window (empty if none has been
  // sepcified).
  IPC_STRUCT_MEMBER(GURL, target_url)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(ViewHostMsg_CreateWorker_Params)
  // URL for the worker script.
  IPC_STRUCT_MEMBER(GURL, url)

  // Name for a SharedWorker, otherwise empty string.
  IPC_STRUCT_MEMBER(string16, name)

  // The ID of the parent document (unique within parent renderer).
  IPC_STRUCT_MEMBER(unsigned long long, document_id)

  // RenderView routing id used to send messages back to the parent.
  IPC_STRUCT_MEMBER(int, render_view_route_id)

  // The route ID to associate with the worker. If MSG_ROUTING_NONE is passed,
  // a new unique ID is created and assigned to the worker.
  IPC_STRUCT_MEMBER(int, route_id)

  // The ID of the appcache the main shared worker script resource was loaded
  // from, only valid for shared workers.
  IPC_STRUCT_MEMBER(int64, script_resource_appcache_id)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(ViewHostMsg_AccessibilityNotification_Params)
  // Type of notification.
  IPC_STRUCT_MEMBER(ViewHostMsg_AccEvent::Value, notification_type)

  // ID of the node that the notification applies to.
  IPC_STRUCT_MEMBER(int, id)

  // The accessibility node tree.
  IPC_STRUCT_MEMBER(webkit_glue::WebAccessibility, acc_tree)

  // Whether children are included in this tree, otherwise it's just an
  // update to this one node and existing children are left in place.
  IPC_STRUCT_MEMBER(bool, includes_children)
IPC_STRUCT_END()

// Parameters structure for ViewHostMsg_FrameNavigate, which has too many data
// parameters to be reasonably put in a predefined IPC message.
IPC_STRUCT_BEGIN_WITH_PARENT(ViewHostMsg_FrameNavigate_Params,
                             content::FrameNavigateParams)
  IPC_STRUCT_TRAITS_PARENT(content::FrameNavigateParams)
  // The frame ID for this navigation. The frame ID uniquely identifies the
  // frame the navigation happened in for a given renderer.
  IPC_STRUCT_MEMBER(int64, frame_id)

  // Information regarding the security of the connection (empty if the
  // connection was not secure).
  IPC_STRUCT_MEMBER(std::string, security_info)

  // The gesture that initiated this navigation.
  IPC_STRUCT_MEMBER(NavigationGesture, gesture)

  // True if this was a post request.
  IPC_STRUCT_MEMBER(bool, is_post)

  // Whether the frame navigation resulted in no change to the documents within
  // the page. For example, the navigation may have just resulted in scrolling
  // to a named anchor.
  IPC_STRUCT_MEMBER(bool, was_within_same_page)

  // The status code of the HTTP request.
  IPC_STRUCT_MEMBER(int, http_status_code)

  // True if the connection was proxied.  In this case, socket_address
  // will represent the address of the proxy, rather than the remote host.
  IPC_STRUCT_MEMBER(bool, was_fetched_via_proxy)

  // Serialized history item state to store in the navigation entry.
  IPC_STRUCT_MEMBER(std::string, content_state)
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

IPC_STRUCT_BEGIN(ViewHostMsg_UpdateRect_Params)
  // The bitmap to be painted into the view at the locations specified by
  // update_rects.
  IPC_STRUCT_MEMBER(TransportDIB::Id, bitmap)

  // The position and size of the bitmap.
  IPC_STRUCT_MEMBER(gfx::Rect, bitmap_rect)

  // The scroll offset.  Only one of these can be non-zero, and if they are
  // both zero, then it means there is no scrolling and the scroll_rect is
  // ignored.
  IPC_STRUCT_MEMBER(int, dx)
  IPC_STRUCT_MEMBER(int, dy)

  // The rectangular region to scroll.
  IPC_STRUCT_MEMBER(gfx::Rect, scroll_rect)

  // The scroll offset of the render view.
  IPC_STRUCT_MEMBER(gfx::Point, scroll_offset)

  // The regions of the bitmap (in view coords) that contain updated pixels.
  // In the case of scrolling, this includes the scroll damage rect.
  IPC_STRUCT_MEMBER(std::vector<gfx::Rect>, copy_rects)

  // The size of the RenderView when this message was generated.  This is
  // included so the host knows how large the view is from the perspective of
  // the renderer process.  This is necessary in case a resize operation is in
  // progress.
  IPC_STRUCT_MEMBER(gfx::Size, view_size)

  // The area of the RenderView reserved for resize corner when this message
  // was generated.  Reported for the same reason as view_size is.
  IPC_STRUCT_MEMBER(gfx::Rect, resizer_rect)

  // New window locations for plugin child windows.
  IPC_STRUCT_MEMBER(std::vector<webkit::npapi::WebPluginGeometry>,
                    plugin_window_moves)

  // The following describes the various bits that may be set in flags:
  //
  //   ViewHostMsg_UpdateRect_Flags::IS_RESIZE_ACK
  //     Indicates that this is a response to a ViewMsg_Resize message.
  //
  //   ViewHostMsg_UpdateRect_Flags::IS_RESTORE_ACK
  //     Indicates that this is a response to a ViewMsg_WasRestored message.
  //
  //   ViewHostMsg_UpdateRect_Flags::IS_REPAINT_ACK
  //     Indicates that this is a response to a ViewMsg_Repaint message.
  //
  // If flags is zero, then this message corresponds to an unsolicited paint
  // request by the render view.  Any of the above bits may be set in flags,
  // which would indicate that this paint message is an ACK for multiple
  // request messages.
  IPC_STRUCT_MEMBER(int, flags)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(ViewHostMsg_DidFailProvisionalLoadWithError_Params)
  // The frame ID for the failure report.
  IPC_STRUCT_MEMBER(int64, frame_id)
  // True if this is the top-most frame.
  IPC_STRUCT_MEMBER(bool, is_main_frame)
  // Error code as reported in the DidFailProvisionalLoad callback.
  IPC_STRUCT_MEMBER(int, error_code)
  // An error message generated from the error_code. This can be an empty
  // string if we were unable to find a meaningful description.
  IPC_STRUCT_MEMBER(string16, error_description)
  // The URL that the error is reported for.
  IPC_STRUCT_MEMBER(GURL, url)
  // True if the failure is the result of navigating to a POST again
  // and we're going to show the POST interstitial.
  IPC_STRUCT_MEMBER(bool, showing_repost_interstitial)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(ViewMsg_SwapOut_Params)
  // The identifier of the RenderProcessHost for the currently closing view.
  //
  // These first two parameters are technically redundant since they are
  // needed only when processing the ACK message, and the processor
  // theoretically knows both the process and route ID. However, this is
  // difficult to figure out with our current implementation, so this
  // information is duplicated here.
  IPC_STRUCT_MEMBER(int, closing_process_id)

  // The route identifier for the currently closing RenderView.
  IPC_STRUCT_MEMBER(int, closing_route_id)

  // The identifier of the RenderProcessHost for the new view attempting to
  // replace the closing one above.
  IPC_STRUCT_MEMBER(int, new_render_process_host_id)

  // The identifier of the *request* the new view made that is causing the
  // cross-site transition. This is *not* a route_id, but the request that we
  // will resume once the ACK from the closing view has been received.
  IPC_STRUCT_MEMBER(int, new_request_id)
IPC_STRUCT_END()

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
  IPC_STRUCT_MEMBER(content::PageTransition, transition)

  // Opaque history state (received by ViewHostMsg_UpdateState).
  IPC_STRUCT_MEMBER(std::string, state)

  // Type of navigation.
  IPC_STRUCT_MEMBER(ViewMsg_Navigate_Type::Value, navigation_type)

  // The time the request was created
  IPC_STRUCT_MEMBER(base::Time, request_time)

  // Extra headers (separated by \n) to send during the request.
  IPC_STRUCT_MEMBER(std::string, extra_headers)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(ViewMsg_New_Params)
  // The parent window's id.
  IPC_STRUCT_MEMBER(gfx::NativeViewId, parent_window)

  // Renderer-wide preferences.
  IPC_STRUCT_MEMBER(content::RendererPreferences, renderer_preferences)

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

// Sent to the RenderView when a new tab is swapped into an existing
// tab and the histories need to be merged. The existing tab has a history of
// |merged_history_length| which precedes the history of the new tab. All
// page_ids >= |minimum_page_id| in the new tab are appended to the history.
//
// For example, suppose the history of page_ids in the new tab's RenderView
// is [4 7 8]. This is merged into an existing tab with 3 history items, and
// all pages in the new tab with page_id >= 7 are to be preserved.
// The resulting page history is [-1 -1 -1 7 8].
IPC_MESSAGE_ROUTED2(ViewMsg_SetHistoryLengthAndPrune,
                    int, /* merge_history_length */
                    int32 /* minimum_page_id */)

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

// Asks the browser for the default audio hardware buffer-size.
IPC_SYNC_MESSAGE_CONTROL0_1(ViewHostMsg_GetHardwareBufferSize,
                            uint32 /* buffer_size */)

// Asks the browser for the default audio input hardware sample-rate.
IPC_SYNC_MESSAGE_CONTROL0_1(ViewHostMsg_GetHardwareInputSampleRate,
                            double /* sample_rate */)

// Asks the browser for the default audio hardware sample-rate.
IPC_SYNC_MESSAGE_CONTROL0_1(ViewHostMsg_GetHardwareSampleRate,
                            double /* sample_rate */)

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
                    content::RendererPreferences)

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
IPC_MESSAGE_ROUTED3(ViewMsg_Resize,
                    gfx::Size /* new_size */,
                    gfx::Rect /* resizer_rect */,
                    bool /* is_fullscreen */)

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

// Sent to inform the view that it was swapped out.  This allows the process to
// exit if no other views are using it.
IPC_MESSAGE_ROUTED0(ViewMsg_WasSwappedOut)

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

// Tells the renderer to scroll the currently focused node into rect only if
// the currently focused node is a Text node (textfield, text area or content
// editable divs).
IPC_MESSAGE_ROUTED1(ViewMsg_ScrollFocusedEditableNodeIntoRect, gfx::Rect)

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
IPC_MESSAGE_ROUTED0(ViewMsg_PasteAndMatchStyle)
// Replaces the selected region or a word around the cursor with the
// specified string.
IPC_MESSAGE_ROUTED1(ViewMsg_Replace,
                    string16)
IPC_MESSAGE_ROUTED0(ViewMsg_Delete)
IPC_MESSAGE_ROUTED0(ViewMsg_SelectAll)

// Requests the renderer to select the region between two points.
IPC_MESSAGE_ROUTED2(ViewMsg_SelectRange,
                    gfx::Point /* start */,
                    gfx::Point /* end */)

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
IPC_MESSAGE_ROUTED2(ViewMsg_CSSInsertRequest,
                    string16,  /* frame_xpath */
                    std::string  /* css string */)

// External popup menus.
IPC_MESSAGE_ROUTED1(ViewMsg_SelectPopupMenuItem,
                    int /* selected index, -1 means no selection */)

// Change the zoom level for the current main frame.  If the level actually
// changes, a ViewHostMsg_DidZoomURL message will be sent back to the browser
// telling it what url got zoomed and what its current zoom level is.
IPC_MESSAGE_ROUTED1(ViewMsg_Zoom,
                    content::PageZoom /* function */)

// Set the zoom level for the current main frame.  If the level actually
// changes, a ViewHostMsg_DidZoomURL message will be sent back to the browser
// telling it what url got zoomed and what its current zoom level is.
IPC_MESSAGE_ROUTED1(ViewMsg_SetZoomLevel,
                    double /* zoom_level */)

// Zooms the page by the factor defined in the renderer.
IPC_MESSAGE_ROUTED3(ViewMsg_ZoomFactor,
                    content::PageZoom,
                    int /* zoom center_x */,
                    int /* zoom center_y */)

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

// Change encoding of page in the renderer.
IPC_MESSAGE_ROUTED1(ViewMsg_SetPageEncoding,
                    std::string /*new encoding name*/)

// Reset encoding of page in the renderer back to default.
IPC_MESSAGE_ROUTED0(ViewMsg_ResetPageEncodingToDefault)

// Requests the renderer to reserve a range of page ids.
IPC_MESSAGE_ROUTED1(ViewMsg_ReservePageIDRange,
                    int  /* size_of_range */)

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
IPC_MESSAGE_ROUTED2(ViewMsg_ImeConfirmComposition,
                    string16 /* text */,
                    ui::Range /* replacement_range */)

// Used to notify the render-view that we have received a target URL. Used
// to prevent target URLs spamming the browser.
IPC_MESSAGE_ROUTED0(ViewMsg_UpdateTargetURL_ACK)


// Sets the alternate error page URL (link doctor) for the renderer process.
IPC_MESSAGE_ROUTED1(ViewMsg_SetAltErrorPageURL,
                    GURL)

IPC_MESSAGE_ROUTED1(ViewMsg_RunFileChooserResponse,
                    std::vector<FilePath> /* selected files */)

// Provides the results of directory enumeration.
IPC_MESSAGE_ROUTED2(ViewMsg_EnumerateDirectoryResponse,
                    int /* request_id */,
                    std::vector<FilePath> /* files_in_directory */)

// When a renderer sends a ViewHostMsg_Focus to the browser process,
// the browser has the option of sending a ViewMsg_CantFocus back to
// the renderer.
IPC_MESSAGE_ROUTED0(ViewMsg_CantFocus)

// Instructs the renderer to invoke the frame's shouldClose method, which
// runs the onbeforeunload event handler.  Expects the result to be returned
// via ViewHostMsg_ShouldClose.
IPC_MESSAGE_ROUTED0(ViewMsg_ShouldClose)

// Instructs the renderer to swap out for a cross-site transition, including
// running the unload event handler. See the struct above for more details.
//
// Expects a SwapOut_ACK message when finished, where the parameters are
// echoed back.
IPC_MESSAGE_ROUTED1(ViewMsg_SwapOut,
                    ViewMsg_SwapOut_Params)

// Instructs the renderer to close the current page, including running the
// onunload event handler.
//
// Expects a ClosePage_ACK message when finished.
IPC_MESSAGE_ROUTED0(ViewMsg_ClosePage)

// Notifies the renderer about ui theme changes
IPC_MESSAGE_ROUTED0(ViewMsg_ThemeChanged)

// Notifies the renderer that a paint is to be generated for the rectangle
// passed in.
IPC_MESSAGE_ROUTED1(ViewMsg_Repaint,
                    gfx::Size /* The view size to be repainted */)

// Notification that a move or resize renderer's containing window has
// started.
IPC_MESSAGE_ROUTED0(ViewMsg_MoveOrResizeStarted)

// Reply to ViewHostMsg_RequestMove, ViewHostMsg_ShowView, and
// ViewHostMsg_ShowWidget to inform the renderer that the browser has
// processed the move.  The browser may have ignored the move, but it finished
// processing.  This is used because the renderer keeps a temporary cache of
// the widget position while these asynchronous operations are in progress.
IPC_MESSAGE_ROUTED0(ViewMsg_Move_ACK)

// Used to instruct the RenderView to send back updates to the preferred size.
IPC_MESSAGE_ROUTED0(ViewMsg_EnablePreferredSizeChangedMode)

// Changes the text direction of the currently selected input field (if any).
IPC_MESSAGE_ROUTED1(ViewMsg_SetTextDirection,
                    WebKit::WebTextDirection /* direction */)

// Tells the renderer to clear the focused node (if any).
IPC_MESSAGE_ROUTED0(ViewMsg_ClearFocusedNode)

// Make the RenderView transparent and render it onto a custom background. The
// background will be tiled in both directions if it is not large enough.
IPC_MESSAGE_ROUTED1(ViewMsg_SetBackground,
                    SkBitmap /* background */)

// Used to tell the renderer not to add scrollbars with height and
// width below a threshold.
IPC_MESSAGE_ROUTED1(ViewMsg_DisableScrollbarsForSmallWindows,
                    gfx::Size /* disable_scrollbar_size_limit */)

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

// Message sent from the browser to the renderer when the user starts or stops
// resizing the view.
IPC_MESSAGE_ROUTED1(ViewMsg_SetInLiveResize,
                    bool /* enable */)

// Tell the renderer that plugin IME has completed.
IPC_MESSAGE_ROUTED2(ViewMsg_PluginImeCompositionCompleted,
                    string16 /* text */,
                    int /* plugin_id */)
#endif

// Response message to ViewHostMsg_CreateShared/DedicatedWorker.
// Sent when the worker has started.
IPC_MESSAGE_ROUTED0(ViewMsg_WorkerCreated)

// The response to ViewHostMsg_AsyncOpenFile.
IPC_MESSAGE_ROUTED3(ViewMsg_AsyncOpenFile_ACK,
                    base::PlatformFileError /* error_code */,
                    IPC::PlatformFileForTransit /* file descriptor */,
                    int /* message_id */)

// Tells the renderer that the network state has changed and that
// window.navigator.onLine should be updated for all WebViews.
IPC_MESSAGE_CONTROL1(ViewMsg_NetworkStateChanged,
                     bool /* online */)

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

// Reply to ViewHostMsg_OpenChannelToPpapiBroker
// Tells the renderer that the channel to the broker has been created.
IPC_MESSAGE_ROUTED3(ViewMsg_PpapiBrokerChannelCreated,
                    int /* request_id */,
                    base::ProcessHandle /* broker_process_handle */,
                    IPC::ChannelHandle /* handle */)

// Tells the renderer to empty its plugin list cache, optional reloading
// pages containing plugins.
IPC_MESSAGE_CONTROL1(ViewMsg_PurgePluginListCache,
                     bool /* reload_pages */)

// Sent to the renderer when a popup window should no longer count against
// the current popup count (either because it's not a popup or because it was
// a generated by a user action).
IPC_MESSAGE_ROUTED0(ViewMsg_DisassociateFromPopupCount)

// Used to instruct the RenderView to go into "view source" mode.
IPC_MESSAGE_ROUTED0(ViewMsg_EnableViewSourceMode)

// Instructs the renderer to save the current page to MHTML.
IPC_MESSAGE_ROUTED2(ViewMsg_SavePageAsMHTML,
                    int /* job_id */,
                    IPC::PlatformFileForTransit /* file handle */)

// Exit fullscreen.
IPC_MESSAGE_ROUTED0(ViewMsg_ExitFullscreen)

// Temporary message to diagnose an unexpected condition in TabContents.
IPC_MESSAGE_CONTROL1(ViewMsg_TempCrashWithData,
                     GURL /* data */)


// Messages sent from the renderer to the browser.

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

// Tells the render side that a ViewHostMsg_LockMouse message has been
// processed. |succeeded| indicates whether the mouse has been successfully
// locked or not.
IPC_MESSAGE_ROUTED1(ViewMsg_LockMouse_ACK,
                    bool /* succeeded */)
// Tells the render side that the mouse has been unlocked.
IPC_MESSAGE_ROUTED0(ViewMsg_MouseLockLost)

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

// This message is sent after ViewHostMsg_ShowView to cause the RenderView
// to run in a modal fashion until it is closed.
IPC_SYNC_MESSAGE_ROUTED0_0(ViewHostMsg_RunModal)

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
                    base::ListValue  /* result */)

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

// Provides the result from running OnMsgShouldClose.  |proceed| matches the
// return value of the the frame's shouldClose method (which includes the
// onbeforeunload handler): true if the user decided to proceed with leaving
// the page.
IPC_MESSAGE_ROUTED1(ViewHostMsg_ShouldClose_ACK,
                    bool /* proceed */)

// Indicates that the current renderer has swapped out, after a SwapOut
// message. The parameters are just echoed from the SwapOut request.
IPC_MESSAGE_ROUTED1(ViewHostMsg_SwapOut_ACK,
                    ViewMsg_SwapOut_Params)

// Indicates that the current page has been closed, after a ClosePage
// message.
IPC_MESSAGE_ROUTED0(ViewHostMsg_ClosePage_ACK)

// Notifies the browser that media has started/stopped playing.
IPC_MESSAGE_ROUTED4(ViewHostMsg_MediaNotification,
                    int64 /* player_cookie, distinguishes instances */,
                    bool /* has_video */,
                    bool /* has_audio */,
                    bool /* is_playing */)

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
IPC_MESSAGE_ROUTED3(ViewHostMsg_UpdateTitle,
                    int32 /* page_id */,
                    string16 /* title */,
                    WebKit::WebTextDirection /* title direction */)

// Changes the icon url for the page in the UI.
IPC_MESSAGE_ROUTED2(ViewHostMsg_UpdateIconURL,
                    int32,
                    GURL)

// Change the encoding name of the page in UI when the page has detected
// proper encoding name.
IPC_MESSAGE_ROUTED1(ViewHostMsg_UpdateEncoding,
                    std::string /* new encoding name */)

// Notifies the browser that we want to show a destination url for a potential
// action (e.g. when the user is hovering over a link).
IPC_MESSAGE_ROUTED2(ViewHostMsg_UpdateTargetURL,
                    int32,
                    GURL)

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
IPC_MESSAGE_ROUTED4(ViewHostMsg_DidLoadResourceFromMemoryCache,
                    GURL /* url */,
                    std::string  /* security info */,
                    std::string  /* http method */,
                    ResourceType::Type /* resource type */)

// Sent when the renderer displays insecure content in a secure page.
IPC_MESSAGE_ROUTED0(ViewHostMsg_DidDisplayInsecureContent)

// Sent when the renderer runs insecure content in a secure origin.
IPC_MESSAGE_ROUTED2(ViewHostMsg_DidRunInsecureContent,
                    std::string  /* security_origin */,
                    GURL         /* target URL */)

// Sent when the renderer starts a provisional load for a frame.
IPC_MESSAGE_ROUTED4(ViewHostMsg_DidStartProvisionalLoadForFrame,
                    int64 /* frame_id */,
                    bool /* true if it is the main frame */,
                    GURL /* opener url if present, else empty */,
                    GURL /* url */)

// Sent when the renderer fails a provisional load with an error.
IPC_MESSAGE_ROUTED1(ViewHostMsg_DidFailProvisionalLoadWithError,
                    ViewHostMsg_DidFailProvisionalLoadWithError_Params)


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
IPC_MESSAGE_ROUTED2(ViewHostMsg_HandleInputEvent_ACK,
                    WebKit::WebInputEvent::Type,
                    bool /* processed */)

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

IPC_MESSAGE_ROUTED1(ViewHostMsg_SetCursor,
                    WebCursor)

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
IPC_SYNC_MESSAGE_CONTROL2_1(ViewHostMsg_GetRawCookies,
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
IPC_SYNC_MESSAGE_CONTROL2_1(ViewHostMsg_CookiesEnabled,
                            GURL /* url */,
                            GURL /* first_party_for_cookies */,
                            bool /* cookies_enabled */)

// Used to get the list of plugins
IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_GetPlugins,
    bool /* refresh*/,
    std::vector<webkit::WebPluginInfo> /* plugins */)

// Return information about a plugin for the given URL and MIME
// type. If there is no matching plugin, |found| is false.
// |actual_mime_type| is the actual mime type supported by the
// found plugin.
IPC_SYNC_MESSAGE_CONTROL4_3(ViewHostMsg_GetPluginInfo,
                            int /* routing_id */,
                            GURL /* url */,
                            GURL /* page_url */,
                            std::string /* mime_type */,
                            bool /* found */,
                            webkit::WebPluginInfo /* plugin info */,
                            std::string /* actual_mime_type */)

// A renderer sends this to the browser process when it wants to
// create a plugin.  The browser will create the plugin process if
// necessary, and will return a handle to the channel on success.
// On error an empty string is returned.
IPC_SYNC_MESSAGE_CONTROL4_2(ViewHostMsg_OpenChannelToPlugin,
                            int /* routing_id */,
                            GURL /* url */,
                            GURL /* page_url */,
                            std::string /* mime_type */,
                            IPC::ChannelHandle /* channel_handle */,
                            webkit::WebPluginInfo /* info */)

// Get the list of proxies to use for |url|, as a semicolon delimited list
// of "<TYPE> <HOST>:<PORT>" | "DIRECT".
IPC_SYNC_MESSAGE_CONTROL1_2(ViewHostMsg_ResolveProxy,
                            GURL /* url */,
                            bool /* result */,
                            std::string /* proxy list */)

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

// Wraps an IPC message that's destined to the worker on the renderer->browser
// hop.
IPC_MESSAGE_CONTROL1(ViewHostMsg_ForwardToWorker,
                     IPC::Message /* message */)

// Tells the browser that a specific Appcache manifest in the current page
// was accessed.
IPC_MESSAGE_ROUTED2(ViewHostMsg_AppCacheAccessed,
                    GURL /* manifest url */,
                    bool /* blocked by policy */)

// Initiates a download based on user actions like 'ALT+click'.
IPC_MESSAGE_ROUTED3(ViewHostMsg_DownloadUrl,
                    GURL     /* url */,
                    GURL     /* referrer */,
                    string16 /* suggested_name */)

// Used to go to the session history entry at the given offset (ie, -1 will
// return the "back" item).
IPC_MESSAGE_ROUTED1(ViewHostMsg_GoToEntryAtOffset,
                    int /* offset (from current) of history item to get */)

IPC_SYNC_MESSAGE_ROUTED4_2(ViewHostMsg_RunJavaScriptMessage,
                           string16     /* in - alert message */,
                           string16     /* in - default prompt */,
                           GURL         /* in - originating page URL */,
                           int          /* in - dialog flags */,
                           bool         /* out - success */,
                           string16     /* out - user_input field */)

// Requests that the given URL be opened in the specified manner.
IPC_MESSAGE_ROUTED4(ViewHostMsg_OpenURL,
                    GURL /* url */,
                    GURL /* referrer */,
                    WindowOpenDisposition /* disposition */,
                    int64 /* frame id */)

// Notifies that the preferred size of the content changed.
IPC_MESSAGE_ROUTED1(ViewHostMsg_DidContentsPreferredSizeChange,
                    gfx::Size /* pref_size */)

// Notifies that the scrollbars-visible state of the content changed.
IPC_MESSAGE_ROUTED2(ViewHostMsg_DidChangeScrollOffsetPinningForMainFrame,
                    bool /* has_horizontal_scrollbar */,
                    bool /* has_vertical_scrollbar */)

// Notifies that the pinned-to-side state of the content changed.
IPC_MESSAGE_ROUTED2(ViewHostMsg_DidChangeScrollbarsForMainFrame,
                    bool /* pinned_to_left */,
                    bool /* pinned_to_right */)

// Notifies that the number of JavaScript scroll handlers changed.
IPC_MESSAGE_ROUTED1(ViewHostMsg_DidChangeNumWheelEvents,
                    int /* count */)

// A message from HTML-based UI.  When (trusted) Javascript calls
// send(message, args), this message is sent to the browser.
IPC_MESSAGE_ROUTED3(ViewHostMsg_WebUISend,
                    GURL /* source_url */,
                    std::string  /* message */,
                    base::ListValue /* args */)

// A renderer sends this to the browser process when it wants to
// create a ppapi plugin.  The browser will create the plugin process if
// necessary, and will return a handle to the channel on success.
// On error an empty string is returned.
IPC_SYNC_MESSAGE_CONTROL1_2(ViewHostMsg_OpenChannelToPepperPlugin,
                            FilePath /* path */,
                            base::ProcessHandle /* plugin_process_handle */,
                            IPC::ChannelHandle /* handle to channel */)

// A renderer sends this to the browser process when it wants to
// create a ppapi broker.  The browser will create the broker process
// if necessary, and will return a handle to the channel on success.
// On error an empty string is returned.
// The browser will respond with ViewMsg_PpapiBrokerChannelCreated.
IPC_MESSAGE_CONTROL3(ViewHostMsg_OpenChannelToPpapiBroker,
                     int /* routing_id */,
                     int /* request_id */,
                     FilePath /* path */)

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
IPC_SYNC_MESSAGE_CONTROL1_3(ViewHostMsg_LoadFont,
                           FontDescriptor /* font to load */,
                           uint32 /* buffer size */,
                           base::SharedMemoryHandle /* font data */,
                           uint32 /* font id */)
#endif

// Returns WebScreenInfo corresponding to the view.
// TODO(shess): Provide a mapping from reply_msg->routing_id() to
// HWND so that we can eliminate the NativeViewId parameter.
IPC_SYNC_MESSAGE_ROUTED1_1(ViewHostMsg_GetScreenInfo,
                           gfx::NativeViewId /* view */,
                           WebKit::WebScreenInfo /* results */)

// Send the tooltip text for the current mouse position to the browser.
IPC_MESSAGE_ROUTED2(ViewHostMsg_SetTooltipText,
                    string16 /* tooltip text string */,
                    WebKit::WebTextDirection /* text direction hint */)

// Notification that the text selection has changed.
// Note: The secound parameter is the character based offset of the string16
// text in the document.
IPC_MESSAGE_ROUTED3(ViewHostMsg_SelectionChanged,
                    string16 /* text covers the selection range */,
                    size_t /* the offset of the text in the document */,
                    ui::Range /* selection range in the document */)

// Notification that the selection bounds have changed.
IPC_MESSAGE_ROUTED2(ViewHostMsg_SelectionBoundsChanged,
                    gfx::Rect /* start rect */,
                    gfx::Rect /* end rect */)

// Asks the browser to display the file chooser.  The result is returned in a
// ViewHost_RunFileChooserResponse message.
IPC_MESSAGE_ROUTED1(ViewHostMsg_RunFileChooser,
                    content::FileChooserParams)

// Asks the browser to enumerate a directory.  This is equivalent to running
// the file chooser in directory-enumeration mode and having the user select
// the given directory.  The result is returned in a
// ViewMsg_EnumerateDirectoryResponse message.
IPC_MESSAGE_ROUTED2(ViewHostMsg_EnumerateDirectory,
                    int /* request_id */,
                    FilePath /* file_path */)

// Tells the browser to move the focus to the next (previous if reverse is
// true) focusable element.
IPC_MESSAGE_ROUTED1(ViewHostMsg_TakeFocus,
                    bool /* reverse */)

// Returns the window location of the window this widget is embeded.
// TODO(shess): Provide a mapping from reply_msg->routing_id() to
// HWND so that we can eliminate the NativeViewId parameter.
IPC_SYNC_MESSAGE_ROUTED1_1(ViewHostMsg_GetRootWindowRect,
                           gfx::NativeViewId /* window */,
                           gfx::Rect /* Out: Window location */)

// Required for updating text input state.
IPC_MESSAGE_ROUTED2(ViewHostMsg_TextInputStateChanged,
                    ui::TextInputType, /* text_input_type */
                    bool /* can_compose_inline */)

// Message sent when the IME text composition range changes.
IPC_MESSAGE_ROUTED1(ViewHostMsg_ImeCompositionRangeChanged,
                    ui::Range /* composition range */)

// Required for cancelling an ongoing input method composition.
IPC_MESSAGE_ROUTED0(ViewHostMsg_ImeCancelComposition)

// WebKit and JavaScript error messages to log to the console
// or debugger UI.
IPC_MESSAGE_ROUTED4(ViewHostMsg_AddMessageToConsole,
                    int32, /* log level */
                    string16, /* msg */
                    int32, /* line number */
                    string16 /* source id */)

// Sent by the renderer process to indicate that a plugin instance has
// crashed.
IPC_MESSAGE_ROUTED1(ViewHostMsg_CrashedPlugin,
                    FilePath /* plugin_path */)

// Displays a box to confirm that the user wants to navigate away from the
// page. Replies true if yes, false otherwise, the reply string is ignored,
// but is included so that we can use OnJavaScriptMessageBoxClosed.
IPC_SYNC_MESSAGE_ROUTED2_2(ViewHostMsg_RunBeforeUnloadConfirm,
                           GURL,     /* in - originating frame URL */
                           string16  /* in - alert message */,
                           bool      /* out - success */,
                           string16  /* out - This is ignored.*/)

// Sent when the renderer process is done processing a DataReceived
// message.
IPC_MESSAGE_ROUTED1(ViewHostMsg_DataReceived_ACK,
                    int /* request_id */)

IPC_MESSAGE_CONTROL1(ViewHostMsg_RevealFolderInOS,
                     FilePath /* path */)

// Sent when a provisional load on the main frame redirects.
IPC_MESSAGE_ROUTED4(ViewHostMsg_DidRedirectProvisionalLoad,
                    int /* page_id */,
                    GURL /* opener url if present, else empty */,
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

// Updates the minimum/maximum allowed zoom percent for this tab from the
// default values.  If |remember| is true, then the zoom setting is applied to
// other pages in the site and is saved, otherwise it only applies to this
// tab.
IPC_MESSAGE_ROUTED3(ViewHostMsg_UpdateZoomLimits,
                    int /* minimum_percent */,
                    int /* maximum_percent */,
                    bool /* remember */)

// Notify the browser that this render process can or can't be suddenly
// terminated.
IPC_MESSAGE_CONTROL1(ViewHostMsg_SuddenTerminationChanged,
                     bool /* enabled */)

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

// Sent to notify the browser about renderer accessibility notifications.
// The browser responds with a ViewMsg_AccessibilityNotifications_ACK.
IPC_MESSAGE_ROUTED1(
    ViewHostMsg_AccessibilityNotifications,
    std::vector<ViewHostMsg_AccessibilityNotification_Params>)

// Opens a file asynchronously. The response returns a file descriptor
// and an error code from base/platform_file.h.
IPC_MESSAGE_ROUTED3(ViewHostMsg_AsyncOpenFile,
                    FilePath /* file path */,
                    int /* flags */,
                    int /* message_id */)

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

// Message sent from the renderer to the browser to request that the browser
// cache |data| associated with |url|.
IPC_MESSAGE_CONTROL3(ViewHostMsg_DidGenerateCacheableMetadata,
                     GURL /* url */,
                     double /* expected_response_time */,
                     std::vector<char> /* data */)

// Updates the content restrictions, i.e. to disable print/copy.
IPC_MESSAGE_ROUTED1(ViewHostMsg_UpdateContentRestrictions,
                    int /* restrictions */)

// Brings up SaveAs... dialog to save specified URL.
IPC_MESSAGE_ROUTED1(ViewHostMsg_SaveURLAs,
                    GURL /* url */)

// Displays a JavaScript out-of-memory message in the infobar.
IPC_MESSAGE_ROUTED0(ViewHostMsg_JSOutOfMemory)

// Register a new handler for URL requests with the given scheme.
IPC_MESSAGE_ROUTED3(ViewHostMsg_RegisterProtocolHandler,
                    std::string /* scheme */,
                    GURL /* url */,
                    string16 /* title */)

// Stores new inspector setting in the profile.
// TODO(jam): this should be in the chrome module
IPC_MESSAGE_ROUTED2(ViewHostMsg_UpdateInspectorSetting,
                    std::string,  /* key */
                    std::string /* value */)

// Puts the browser into "tab fullscreen" mode for the sending renderer.
// See the comment in chrome/browser/ui/browser.h for more details.
IPC_MESSAGE_ROUTED1(ViewHostMsg_ToggleFullscreen,
                    bool /* enter_fullscreen */)

// Send back a string to be recorded by UserMetrics.
IPC_MESSAGE_CONTROL1(ViewHostMsg_UserMetricsRecordAction,
                     std::string /* action */)

// Notifies the browser that the page was or was not saved as MHTML.
IPC_MESSAGE_CONTROL2(ViewHostMsg_SavedPageAsMHTML,
                     int /* job_id */,
                     int64 /* size of the MHTML file, -1 if error */)

IPC_MESSAGE_ROUTED3(ViewHostMsg_SendCurrentPageAllSavableResourceLinks,
                    std::vector<GURL> /* all savable resource links */,
                    std::vector<GURL> /* all referrers of resource links */,
                    std::vector<GURL> /* all frame links */)

IPC_MESSAGE_ROUTED3(ViewHostMsg_SendSerializedHtmlData,
                    GURL /* frame's url */,
                    std::string /* data buffer */,
                    int32 /* complete status */)

// Notifies the browser of an event occurring in the media pipeline.
IPC_MESSAGE_CONTROL1(ViewHostMsg_MediaLogEvent,
                     media::MediaLogEvent /* event */)

// Requests to lock the mouse. Will result in a ViewMsg_LockMouse_ACK message
// being sent back.
IPC_MESSAGE_ROUTED0(ViewHostMsg_LockMouse)

// Requests to unlock the mouse. A ViewMsg_MouseLockLost message will be sent
// whenever the mouse is unlocked (which may or may not be caused by
// ViewHostMsg_UnlockMouse).
IPC_MESSAGE_ROUTED0(ViewHostMsg_UnlockMouse)
