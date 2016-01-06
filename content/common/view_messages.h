// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for page rendering.
// Multiply-included message file, hence no include guard.

#include <stddef.h>
#include <stdint.h>

#include "base/memory/shared_memory.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "cc/output/begin_frame_args.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/resources/shared_bitmap.h"
#include "content/common/content_export.h"
#include "content/common/content_param_traits.h"
#include "content/common/date_time_suggestion.h"
#include "content/common/frame_replication_state.h"
#include "content/common/media/media_param_traits.h"
#include "content/common/navigation_gesture.h"
#include "content/common/view_message_enums.h"
#include "content/common/webplugin_geometry.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/favicon_url.h"
#include "content/public/common/file_chooser_file_info.h"
#include "content/public/common/file_chooser_params.h"
#include "content/public/common/menu_item.h"
#include "content/public/common/message_port_types.h"
#include "content/public/common/page_state.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/referrer.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/stop_find_action.h"
#include "content/public/common/three_d_api_types.h"
#include "content/public/common/window_container_type.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "media/audio/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "media/base/media_log_event.h"
#include "net/base/network_change_notifier.h"
#include "third_party/WebKit/public/platform/WebDisplayMode.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebFloatRect.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationType.h"
#include "third_party/WebKit/public/web/WebDeviceEmulationParams.h"
#include "third_party/WebKit/public/web/WebFindOptions.h"
#include "third_party/WebKit/public/web/WebMediaPlayerAction.h"
#include "third_party/WebKit/public/web/WebPluginAction.h"
#include "third_party/WebKit/public/web/WebPopupType.h"
#include "third_party/WebKit/public/web/WebSharedWorkerCreationContextType.h"
#include "third_party/WebKit/public/web/WebSharedWorkerCreationErrors.h"
#include "third_party/WebKit/public/web/WebTextDirection.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gfx/range/range.h"

#if defined(OS_MACOSX)
#include "third_party/WebKit/public/platform/WebScrollbarButtonsPlacement.h"
#include "third_party/WebKit/public/web/mac/WebScrollbarTheme.h"
#endif

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START ViewMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebDeviceEmulationParams::ScreenPosition,
                          blink::WebDeviceEmulationParams::ScreenPositionLast)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebMediaPlayerAction::Type,
                          blink::WebMediaPlayerAction::Type::TypeLast)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebPluginAction::Type,
                          blink::WebPluginAction::Type::TypeLast)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebPopupType,
                          blink::WebPopupType::WebPopupTypeLast)
// TODO(dcheng): Update WebScreenOrientationType to have a "Last" enum member.
IPC_ENUM_TRAITS_MIN_MAX_VALUE(blink::WebScreenOrientationType,
                              blink::WebScreenOrientationUndefined,
                              blink::WebScreenOrientationLandscapeSecondary)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebWorkerCreationError,
                          blink::WebWorkerCreationErrorLast)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebTextDirection,
                          blink::WebTextDirection::WebTextDirectionLast)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebDisplayMode,
                          blink::WebDisplayMode::WebDisplayModeLast)
IPC_ENUM_TRAITS_MAX_VALUE(WindowContainerType, WINDOW_CONTAINER_TYPE_MAX_VALUE)
IPC_ENUM_TRAITS(content::FaviconURL::IconType)
IPC_ENUM_TRAITS(content::FileChooserParams::Mode)
IPC_ENUM_TRAITS(content::MenuItem::Type)
IPC_ENUM_TRAITS_MAX_VALUE(content::NavigationGesture,
                          content::NavigationGestureLast)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(content::PageZoom,
                              content::PageZoom::PAGE_ZOOM_OUT,
                              content::PageZoom::PAGE_ZOOM_IN)
IPC_ENUM_TRAITS_MAX_VALUE(gfx::FontRenderParams::Hinting,
                          gfx::FontRenderParams::HINTING_MAX)
IPC_ENUM_TRAITS_MAX_VALUE(gfx::FontRenderParams::SubpixelRendering,
                          gfx::FontRenderParams::SUBPIXEL_RENDERING_MAX)
IPC_ENUM_TRAITS_MAX_VALUE(content::TapMultipleTargetsStrategy,
                          content::TAP_MULTIPLE_TARGETS_STRATEGY_MAX)
IPC_ENUM_TRAITS_MAX_VALUE(content::StopFindAction,
                          content::STOP_FIND_ACTION_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(content::ThreeDAPIType,
                          content::THREE_D_API_TYPE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(media::MediaLogEvent::Type,
                          media::MediaLogEvent::TYPE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(ui::TextInputMode, ui::TEXT_INPUT_MODE_MAX)
IPC_ENUM_TRAITS_MAX_VALUE(ui::TextInputType, ui::TEXT_INPUT_TYPE_MAX)

#if defined(OS_MACOSX)
IPC_ENUM_TRAITS_MAX_VALUE(
    blink::WebScrollbarButtonsPlacement,
    blink::WebScrollbarButtonsPlacement::WebScrollbarButtonsPlacementLast)

IPC_ENUM_TRAITS_MAX_VALUE(blink::ScrollerStyle, blink::ScrollerStyleOverlay)
#endif

IPC_STRUCT_TRAITS_BEGIN(blink::WebFindOptions)
  IPC_STRUCT_TRAITS_MEMBER(forward)
  IPC_STRUCT_TRAITS_MEMBER(matchCase)
  IPC_STRUCT_TRAITS_MEMBER(findNext)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::WebMediaPlayerAction)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(enable)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::WebPluginAction)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(enable)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::WebFloatPoint)
  IPC_STRUCT_TRAITS_MEMBER(x)
  IPC_STRUCT_TRAITS_MEMBER(y)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::WebFloatRect)
  IPC_STRUCT_TRAITS_MEMBER(x)
  IPC_STRUCT_TRAITS_MEMBER(y)
  IPC_STRUCT_TRAITS_MEMBER(width)
  IPC_STRUCT_TRAITS_MEMBER(height)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::WebSize)
  IPC_STRUCT_TRAITS_MEMBER(width)
  IPC_STRUCT_TRAITS_MEMBER(height)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::WebDeviceEmulationParams)
  IPC_STRUCT_TRAITS_MEMBER(screenPosition)
  IPC_STRUCT_TRAITS_MEMBER(screenSize)
  IPC_STRUCT_TRAITS_MEMBER(viewPosition)
  IPC_STRUCT_TRAITS_MEMBER(deviceScaleFactor)
  IPC_STRUCT_TRAITS_MEMBER(viewSize)
  IPC_STRUCT_TRAITS_MEMBER(fitToView)
  IPC_STRUCT_TRAITS_MEMBER(offset)
  IPC_STRUCT_TRAITS_MEMBER(scale)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::WebScreenInfo)
  IPC_STRUCT_TRAITS_MEMBER(deviceScaleFactor)
  IPC_STRUCT_TRAITS_MEMBER(depth)
  IPC_STRUCT_TRAITS_MEMBER(depthPerComponent)
  IPC_STRUCT_TRAITS_MEMBER(isMonochrome)
  IPC_STRUCT_TRAITS_MEMBER(rect)
  IPC_STRUCT_TRAITS_MEMBER(availableRect)
  IPC_STRUCT_TRAITS_MEMBER(orientationType)
  IPC_STRUCT_TRAITS_MEMBER(orientationAngle)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::MenuItem)
  IPC_STRUCT_TRAITS_MEMBER(label)
  IPC_STRUCT_TRAITS_MEMBER(tool_tip)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(action)
  IPC_STRUCT_TRAITS_MEMBER(rtl)
  IPC_STRUCT_TRAITS_MEMBER(has_directional_override)
  IPC_STRUCT_TRAITS_MEMBER(enabled)
  IPC_STRUCT_TRAITS_MEMBER(checked)
  IPC_STRUCT_TRAITS_MEMBER(submenu)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::DateTimeSuggestion)
  IPC_STRUCT_TRAITS_MEMBER(value)
  IPC_STRUCT_TRAITS_MEMBER(localized_value)
  IPC_STRUCT_TRAITS_MEMBER(label)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::FaviconURL)
  IPC_STRUCT_TRAITS_MEMBER(icon_url)
  IPC_STRUCT_TRAITS_MEMBER(icon_type)
  IPC_STRUCT_TRAITS_MEMBER(icon_sizes)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::FileChooserFileInfo)
  IPC_STRUCT_TRAITS_MEMBER(file_path)
  IPC_STRUCT_TRAITS_MEMBER(display_name)
  IPC_STRUCT_TRAITS_MEMBER(file_system_url)
  IPC_STRUCT_TRAITS_MEMBER(modification_time)
  IPC_STRUCT_TRAITS_MEMBER(length)
  IPC_STRUCT_TRAITS_MEMBER(is_directory)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::FileChooserParams)
  IPC_STRUCT_TRAITS_MEMBER(mode)
  IPC_STRUCT_TRAITS_MEMBER(title)
  IPC_STRUCT_TRAITS_MEMBER(default_file_name)
  IPC_STRUCT_TRAITS_MEMBER(accept_types)
  IPC_STRUCT_TRAITS_MEMBER(need_local_path)
#if defined(OS_ANDROID)
  IPC_STRUCT_TRAITS_MEMBER(capture)
#endif
  IPC_STRUCT_TRAITS_MEMBER(requestor)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::RendererPreferences)
  IPC_STRUCT_TRAITS_MEMBER(can_accept_load_drops)
  IPC_STRUCT_TRAITS_MEMBER(should_antialias_text)
  IPC_STRUCT_TRAITS_MEMBER(hinting)
  IPC_STRUCT_TRAITS_MEMBER(use_autohinter)
  IPC_STRUCT_TRAITS_MEMBER(use_bitmaps)
  IPC_STRUCT_TRAITS_MEMBER(subpixel_rendering)
  IPC_STRUCT_TRAITS_MEMBER(use_subpixel_positioning)
  IPC_STRUCT_TRAITS_MEMBER(focus_ring_color)
  IPC_STRUCT_TRAITS_MEMBER(thumb_active_color)
  IPC_STRUCT_TRAITS_MEMBER(thumb_inactive_color)
  IPC_STRUCT_TRAITS_MEMBER(track_color)
  IPC_STRUCT_TRAITS_MEMBER(active_selection_bg_color)
  IPC_STRUCT_TRAITS_MEMBER(active_selection_fg_color)
  IPC_STRUCT_TRAITS_MEMBER(inactive_selection_bg_color)
  IPC_STRUCT_TRAITS_MEMBER(inactive_selection_fg_color)
  IPC_STRUCT_TRAITS_MEMBER(browser_handles_all_top_level_requests)
  IPC_STRUCT_TRAITS_MEMBER(caret_blink_interval)
  IPC_STRUCT_TRAITS_MEMBER(use_custom_colors)
  IPC_STRUCT_TRAITS_MEMBER(enable_referrers)
  IPC_STRUCT_TRAITS_MEMBER(enable_do_not_track)
  IPC_STRUCT_TRAITS_MEMBER(webrtc_ip_handling_policy)
  IPC_STRUCT_TRAITS_MEMBER(default_zoom_level)
  IPC_STRUCT_TRAITS_MEMBER(user_agent_override)
  IPC_STRUCT_TRAITS_MEMBER(accept_languages)
  IPC_STRUCT_TRAITS_MEMBER(report_frame_name_changes)
  IPC_STRUCT_TRAITS_MEMBER(tap_multiple_targets_strategy)
  IPC_STRUCT_TRAITS_MEMBER(disable_client_blocked_error_page)
  IPC_STRUCT_TRAITS_MEMBER(plugin_fullscreen_allowed)
  IPC_STRUCT_TRAITS_MEMBER(use_video_overlay_for_embedded_encrypted_video)
  IPC_STRUCT_TRAITS_MEMBER(use_view_overlay_for_all_video)
  IPC_STRUCT_TRAITS_MEMBER(network_contry_iso)
#if defined(OS_WIN)
  IPC_STRUCT_TRAITS_MEMBER(caption_font_family_name)
  IPC_STRUCT_TRAITS_MEMBER(caption_font_height)
  IPC_STRUCT_TRAITS_MEMBER(small_caption_font_family_name)
  IPC_STRUCT_TRAITS_MEMBER(small_caption_font_height)
  IPC_STRUCT_TRAITS_MEMBER(menu_font_family_name)
  IPC_STRUCT_TRAITS_MEMBER(menu_font_height)
  IPC_STRUCT_TRAITS_MEMBER(status_font_family_name)
  IPC_STRUCT_TRAITS_MEMBER(status_font_height)
  IPC_STRUCT_TRAITS_MEMBER(message_font_family_name)
  IPC_STRUCT_TRAITS_MEMBER(message_font_height)
  IPC_STRUCT_TRAITS_MEMBER(vertical_scroll_bar_width_in_dips)
  IPC_STRUCT_TRAITS_MEMBER(horizontal_scroll_bar_height_in_dips)
  IPC_STRUCT_TRAITS_MEMBER(arrow_bitmap_height_vertical_scroll_bar_in_dips)
  IPC_STRUCT_TRAITS_MEMBER(arrow_bitmap_width_horizontal_scroll_bar_in_dips)
#endif
  IPC_STRUCT_TRAITS_MEMBER(default_font_size)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::WebPluginGeometry)
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
  IPC_STRUCT_MEMBER(int64_t, session_storage_namespace_id)

  // The name of the resulting frame that should be created (empty if none
  // has been specified). UTF8 encoded string.
  IPC_STRUCT_MEMBER(std::string, frame_name)

  // The routing id of the frame initiating the open.
  IPC_STRUCT_MEMBER(int, opener_render_frame_id)

  // The URL of the frame initiating the open.
  IPC_STRUCT_MEMBER(GURL, opener_url)

  // The URL of the top frame containing the opener.
  IPC_STRUCT_MEMBER(GURL, opener_top_level_frame_url)

  // The security origin of the frame initiating the open.
  IPC_STRUCT_MEMBER(GURL, opener_security_origin)

  // Whether the opener will be suppressed in the new window, in which case
  // scripting the new window is not allowed.
  IPC_STRUCT_MEMBER(bool, opener_suppressed)

  // Whether the window should be opened in the foreground, background, etc.
  IPC_STRUCT_MEMBER(WindowOpenDisposition, disposition)

  // The URL that will be loaded in the new window (empty if none has been
  // sepcified).
  IPC_STRUCT_MEMBER(GURL, target_url)

  // The referrer that will be used to load |target_url| (empty if none has
  // been specified).
  IPC_STRUCT_MEMBER(content::Referrer, referrer)

  // The window features to use for the new view.
  IPC_STRUCT_MEMBER(blink::WebWindowFeatures, features)

  // The additional window features to use for the new view. We pass these
  // separately from |features| above because we cannot serialize WebStrings
  // over IPC.
  IPC_STRUCT_MEMBER(std::vector<base::string16>, additional_features)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(ViewHostMsg_CreateWindow_Reply)
  // The ID of the view to be created. If the ID is MSG_ROUTING_NONE, then the
  // view couldn't be created.
  IPC_STRUCT_MEMBER(int32_t, route_id, MSG_ROUTING_NONE)

  // The ID of the main frame hosted in the view.
  IPC_STRUCT_MEMBER(int32_t, main_frame_route_id, MSG_ROUTING_NONE)

  // The ID of the widget for the main frame.
  IPC_STRUCT_MEMBER(int32_t, main_frame_widget_route_id, MSG_ROUTING_NONE)

  // TODO(dcheng): No clue. This is kind of duplicated from ViewMsg_New_Params.
  IPC_STRUCT_MEMBER(int64_t, cloned_session_storage_namespace_id)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(ViewHostMsg_CreateWorker_Params)
  // URL for the worker script.
  IPC_STRUCT_MEMBER(GURL, url)

  // Name for a SharedWorker, otherwise empty string.
  IPC_STRUCT_MEMBER(base::string16, name)

  // Security policy used in the worker.
  IPC_STRUCT_MEMBER(base::string16, content_security_policy)

  // Security policy type used in the worker.
  IPC_STRUCT_MEMBER(blink::WebContentSecurityPolicyType, security_policy_type)

  // The ID of the parent document (unique within parent renderer).
  IPC_STRUCT_MEMBER(unsigned long long, document_id)

  // RenderFrame routing id used to send messages back to the parent.
  IPC_STRUCT_MEMBER(int, render_frame_route_id)

  // The type (secure or nonsecure) of the context that created the worker.
  IPC_STRUCT_MEMBER(blink::WebSharedWorkerCreationContextType,
                    creation_context_type)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(ViewHostMsg_CreateWorker_Reply)
  // The route id for the created worker.
  IPC_STRUCT_MEMBER(int, route_id)

  // The error that occurred, if the browser failed to create the
  // worker.
  IPC_STRUCT_MEMBER(blink::WebWorkerCreationError, error)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(ViewHostMsg_DateTimeDialogValue_Params)
  IPC_STRUCT_MEMBER(ui::TextInputType, dialog_type)
  IPC_STRUCT_MEMBER(double, dialog_value)
  IPC_STRUCT_MEMBER(double, minimum)
  IPC_STRUCT_MEMBER(double, maximum)
  IPC_STRUCT_MEMBER(double, step)
  IPC_STRUCT_MEMBER(std::vector<content::DateTimeSuggestion>, suggestions)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(ViewHostMsg_SelectionBounds_Params)
  IPC_STRUCT_MEMBER(gfx::Rect, anchor_rect)
  IPC_STRUCT_MEMBER(blink::WebTextDirection, anchor_dir)
  IPC_STRUCT_MEMBER(gfx::Rect, focus_rect)
  IPC_STRUCT_MEMBER(blink::WebTextDirection, focus_dir)
  IPC_STRUCT_MEMBER(bool, is_anchor_first)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(ViewHostMsg_TextInputState_Params)
  // The type of input field
  IPC_STRUCT_MEMBER(ui::TextInputType, type)

  // The mode of input field
  IPC_STRUCT_MEMBER(ui::TextInputMode, mode)

  // The flags of the input field (autocorrect, autocomplete, etc.)
  IPC_STRUCT_MEMBER(int, flags)

  // The value of the input field
  IPC_STRUCT_MEMBER(std::string, value)

  // The cursor position of the current selection start, or the caret position
  // if nothing is selected
  IPC_STRUCT_MEMBER(int, selection_start)

  // The cursor position of the current selection end, or the caret position
  // if nothing is selected
  IPC_STRUCT_MEMBER(int, selection_end)

  // The start position of the current composition, or -1 if there is none
  IPC_STRUCT_MEMBER(int, composition_start)

  // The end position of the current composition, or -1 if there is none
  IPC_STRUCT_MEMBER(int, composition_end)

  // Whether or not inline composition can be performed for the current input.
  IPC_STRUCT_MEMBER(bool, can_compose_inline)

  // Whether or not the IME should be shown as a result of this update. Even if
  // true, the IME will only be shown if the type is appropriate (e.g. not
  // TEXT_INPUT_TYPE_NONE).
  IPC_STRUCT_MEMBER(bool, show_ime_if_needed)

  // Whether this change is originated from non-IME (e.g. Javascript, Autofill).
  IPC_STRUCT_MEMBER(bool, is_non_ime_change)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(ViewHostMsg_UpdateRect_Params)
  // The size of the RenderView when this message was generated.  This is
  // included so the host knows how large the view is from the perspective of
  // the renderer process.  This is necessary in case a resize operation is in
  // progress. If auto-resize is enabled, this should update the corresponding
  // view size.
  IPC_STRUCT_MEMBER(gfx::Size, view_size)

  // New window locations for plugin child windows.
  IPC_STRUCT_MEMBER(std::vector<content::WebPluginGeometry>,
                    plugin_window_moves)

  // The following describes the various bits that may be set in flags:
  //
  //   ViewHostMsg_UpdateRect_Flags::IS_RESIZE_ACK
  //     Indicates that this is a response to a ViewMsg_Resize message.
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

IPC_STRUCT_BEGIN(ViewMsg_Resize_Params)
  // Information about the screen (dpi, depth, etc..).
  IPC_STRUCT_MEMBER(blink::WebScreenInfo, screen_info)
  // The size of the renderer.
  IPC_STRUCT_MEMBER(gfx::Size, new_size)
  // The size of the view's backing surface in non-DPI-adjusted pixels.
  IPC_STRUCT_MEMBER(gfx::Size, physical_backing_size)
  // Whether or not Blink's viewport size should be shrunk by the height of the
  // URL-bar (always false on platforms where URL-bar hiding isn't supported).
  IPC_STRUCT_MEMBER(bool, top_controls_shrink_blink_size)
  // The height of the top controls (always 0 on platforms where URL-bar hiding
  // isn't supported).
  IPC_STRUCT_MEMBER(float, top_controls_height)
  // The size of the visible viewport, which may be smaller than the view if the
  // view is partially occluded (e.g. by a virtual keyboard).  The size is in
  // DPI-adjusted pixels.
  IPC_STRUCT_MEMBER(gfx::Size, visible_viewport_size)
  // The resizer rect.
  IPC_STRUCT_MEMBER(gfx::Rect, resizer_rect)
  // Indicates whether tab-initiated fullscreen was granted.
  IPC_STRUCT_MEMBER(bool, is_fullscreen_granted)
  // The display mode.
  IPC_STRUCT_MEMBER(blink::WebDisplayMode, display_mode)
  // If set, requests the renderer to reply with a ViewHostMsg_UpdateRect
  // with the ViewHostMsg_UpdateRect_Flags::IS_RESIZE_ACK bit set in flags.
  IPC_STRUCT_MEMBER(bool, needs_resize_ack)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(ViewMsg_New_Params)
  // Renderer-wide preferences.
  IPC_STRUCT_MEMBER(content::RendererPreferences, renderer_preferences)

  // Preferences for this view.
  IPC_STRUCT_MEMBER(content::WebPreferences, web_preferences)

  // The ID of the view to be created.
  IPC_STRUCT_MEMBER(int32_t, view_id, MSG_ROUTING_NONE)

  // The ID of the main frame hosted in the view.
  IPC_STRUCT_MEMBER(int32_t, main_frame_routing_id, MSG_ROUTING_NONE)

  // The ID of the widget for the main frame.
  IPC_STRUCT_MEMBER(int32_t, main_frame_widget_routing_id, MSG_ROUTING_NONE)

  // The session storage namespace ID this view should use.
  IPC_STRUCT_MEMBER(int64_t, session_storage_namespace_id)

  // The route ID of the opener RenderFrame or RenderFrameProxy, if we need to
  // set one (MSG_ROUTING_NONE otherwise).
  IPC_STRUCT_MEMBER(int, opener_frame_route_id, MSG_ROUTING_NONE)

  // Whether the RenderView should initially be swapped out.
  IPC_STRUCT_MEMBER(bool, swapped_out)

  // Carries replicated information, such as frame name and sandbox flags, for
  // this view's main frame, which will be a proxy in |swapped_out|
  // views when in --site-per-process mode, or a RenderFrame in all other
  // cases.
  IPC_STRUCT_MEMBER(content::FrameReplicationState, replicated_frame_state)

  // The ID of the proxy object for the main frame in this view. It is only
  // used if |swapped_out| is true.
  IPC_STRUCT_MEMBER(int32_t, proxy_routing_id, MSG_ROUTING_NONE)

  // Whether the RenderView should initially be hidden.
  IPC_STRUCT_MEMBER(bool, hidden)

  // Whether the RenderView will never be visible.
  IPC_STRUCT_MEMBER(bool, never_visible)

  // Whether the window associated with this view was created with an opener.
  IPC_STRUCT_MEMBER(bool, window_was_created_with_opener)

  // The initial page ID to use for this view, which must be larger than any
  // existing navigation that might be loaded in the view.  Page IDs are unique
  // to a view and are only updated by the renderer after this initial value.
  IPC_STRUCT_MEMBER(int32_t, next_page_id)

  // The initial renderer size.
  IPC_STRUCT_MEMBER(ViewMsg_Resize_Params, initial_size)

  // Whether to enable auto-resize.
  IPC_STRUCT_MEMBER(bool, enable_auto_resize)

  // The minimum size to layout the page if auto-resize is enabled.
  IPC_STRUCT_MEMBER(gfx::Size, min_size)

  // The maximum size to layout the page if auto-resize is enabled.
  IPC_STRUCT_MEMBER(gfx::Size, max_size)
IPC_STRUCT_END()

#if defined(OS_MACOSX)
IPC_STRUCT_BEGIN(ViewMsg_UpdateScrollbarTheme_Params)
  IPC_STRUCT_MEMBER(float, initial_button_delay)
  IPC_STRUCT_MEMBER(float, autoscroll_button_delay)
  IPC_STRUCT_MEMBER(bool, jump_on_track_click)
  IPC_STRUCT_MEMBER(blink::ScrollerStyle, preferred_scroller_style)
  IPC_STRUCT_MEMBER(bool, redraw)
  IPC_STRUCT_MEMBER(bool, scroll_animation_enabled)
  IPC_STRUCT_MEMBER(blink::WebScrollbarButtonsPlacement, button_placement)
IPC_STRUCT_END()
#endif

// Messages sent from the browser to the renderer.

#if defined(OS_ANDROID)
// Tells the renderer to cancel an opened date/time dialog.
IPC_MESSAGE_ROUTED0(ViewMsg_CancelDateTimeDialog)

// Replaces a date time input field.
IPC_MESSAGE_ROUTED1(ViewMsg_ReplaceDateTime,
                    double /* dialog_value */)

#endif

// Tells the render side that a ViewHostMsg_LockMouse message has been
// processed. |succeeded| indicates whether the mouse has been successfully
// locked or not.
IPC_MESSAGE_ROUTED1(ViewMsg_LockMouse_ACK,
                    bool /* succeeded */)
// Tells the render side that the mouse has been unlocked.
IPC_MESSAGE_ROUTED0(ViewMsg_MouseLockLost)

// Sent by the browser when the parameters for vsync alignment have changed.
IPC_MESSAGE_ROUTED2(ViewMsg_UpdateVSyncParameters,
                    base::TimeTicks /* timebase */,
                    base::TimeDelta /* interval */)

// Sent when the history is altered outside of navigation. The history list was
// reset to |history_length| length, and the offset was reset to be
// |history_offset|.
IPC_MESSAGE_ROUTED2(ViewMsg_SetHistoryOffsetAndLength,
                    int /* history_offset */,
                    int /* history_length */)

// Tells the renderer to create a new view.
// This message is slightly different, the view it takes (via
// ViewMsg_New_Params) is the view to create, the message itself is sent as a
// non-view control message.
IPC_MESSAGE_CONTROL1(ViewMsg_New,
                     ViewMsg_New_Params)

// Sends updated preferences to the renderer.
IPC_MESSAGE_ROUTED1(ViewMsg_SetRendererPrefs,
                    content::RendererPreferences)

// This passes a set of webkit preferences down to the renderer.
IPC_MESSAGE_ROUTED1(ViewMsg_UpdateWebPreferences,
                    content::WebPreferences)

// Informs the renderer that the timezone has changed along with a new
// timezone ID.
IPC_MESSAGE_CONTROL1(ViewMsg_TimezoneChange, std::string)

// Tells the render view to close.
// Expects a Close_ACK message when finished.
IPC_MESSAGE_ROUTED0(ViewMsg_Close)

// Tells the render view to change its size.  A ViewHostMsg_UpdateRect message
// is generated in response provided new_size is not empty and not equal to
// the view's current size.  The generated ViewHostMsg_UpdateRect message will
// have the IS_RESIZE_ACK flag set. It also receives the resizer rect so that
// we don't have to fetch it every time WebKit asks for it.
IPC_MESSAGE_ROUTED1(ViewMsg_Resize,
                    ViewMsg_Resize_Params /* params */)

// Enables device emulation. See WebDeviceEmulationParams for description.
IPC_MESSAGE_ROUTED1(ViewMsg_EnableDeviceEmulation,
                    blink::WebDeviceEmulationParams /* params */)

// Disables device emulation, enabled previously by EnableDeviceEmulation.
IPC_MESSAGE_ROUTED0(ViewMsg_DisableDeviceEmulation)

// Sent to inform the renderer of its screen device color profile. An empty
// profile tells the renderer use the default sRGB color profile.
IPC_MESSAGE_ROUTED1(ViewMsg_ColorProfile,
                    std::vector<char> /* color profile */)

// Tells the render view that the resize rect has changed.
IPC_MESSAGE_ROUTED1(ViewMsg_ChangeResizeRect,
                    gfx::Rect /* resizer_rect */)

// Sent to inform the view that it was hidden.  This allows it to reduce its
// resource utilization.
IPC_MESSAGE_ROUTED0(ViewMsg_WasHidden)

// Tells the render view that it is no longer hidden (see WasHidden), and the
// render view is expected to respond with a full repaint if needs_repainting
// is true. If needs_repainting is false, then this message does not trigger a
// message in response.
IPC_MESSAGE_ROUTED2(ViewMsg_WasShown,
                    bool /* needs_repainting */,
                    ui::LatencyInfo /* latency_info */)

// Tells the renderer to focus the first (last if reverse is true) focusable
// node.
IPC_MESSAGE_ROUTED1(ViewMsg_SetInitialFocus,
                    bool /* reverse */)

// Sent to inform the renderer to invoke a context menu.
// The parameter specifies the location in the render view's coordinates.
IPC_MESSAGE_ROUTED2(ViewMsg_ShowContextMenu,
                    ui::MenuSourceType,
                    gfx::Point /* location where menu should be shown */)

// Sent when the user wants to search for a word on the page (find in page).
IPC_MESSAGE_ROUTED3(ViewMsg_Find,
                    int /* request_id */,
                    base::string16 /* search_text */,
                    blink::WebFindOptions)

// This message notifies the renderer that the user has closed the FindInPage
// window (and what action to take regarding the selection).
IPC_MESSAGE_ROUTED1(ViewMsg_StopFinding,
                    content::StopFindAction /* action */)

// Copies the image at location x, y to the clipboard (if there indeed is an
// image at that location).
IPC_MESSAGE_ROUTED2(ViewMsg_CopyImageAt,
                    int /* x */,
                    int /* y */)

// Saves the image at location x, y to the disk (if there indeed is an
// image at that location).
IPC_MESSAGE_ROUTED2(ViewMsg_SaveImageAt,
                    int /* x */,
                    int /* y */)

// Tells the renderer to perform the given action on the media player
// located at the given point.
IPC_MESSAGE_ROUTED2(ViewMsg_MediaPlayerActionAt,
                    gfx::Point, /* location */
                    blink::WebMediaPlayerAction)

// Tells the renderer to perform the given action on the plugin located at
// the given point.
IPC_MESSAGE_ROUTED2(ViewMsg_PluginActionAt,
                    gfx::Point, /* location */
                    blink::WebPluginAction)

// Sets the page scale for the current main frame to the given page scale.
IPC_MESSAGE_ROUTED1(ViewMsg_SetPageScale, float /* page_scale_factor */)

// Change the zoom level for the current main frame.  If the level actually
// changes, a ViewHostMsg_DidZoomURL message will be sent back to the browser
// telling it what url got zoomed and what its current zoom level is.
IPC_MESSAGE_ROUTED1(ViewMsg_Zoom,
                    content::PageZoom /* function */)

// Set the zoom level for a particular url that the renderer is in the
// process of loading.  This will be stored, to be used if the load commits
// and ignored otherwise.
IPC_MESSAGE_ROUTED2(ViewMsg_SetZoomLevelForLoadingURL,
                    GURL /* url */,
                    double /* zoom_level */)

// Set the zoom level for a particular url, so all render views
// displaying this url can update their zoom levels to match.
// If scheme is empty, then only host is used for matching.
IPC_MESSAGE_CONTROL3(ViewMsg_SetZoomLevelForCurrentURL,
                     std::string /* scheme */,
                     std::string /* host */,
                     double /* zoom_level */)

// Set the zoom level for a particular render view.
IPC_MESSAGE_ROUTED2(ViewMsg_SetZoomLevelForView,
                    bool /* uses_temporary_zoom_level */,
                    double /* zoom_level */)

// Change encoding of page in the renderer.
IPC_MESSAGE_ROUTED1(ViewMsg_SetPageEncoding,
                    std::string /*new encoding name*/)

// Reset encoding of page in the renderer back to default.
IPC_MESSAGE_ROUTED0(ViewMsg_ResetPageEncodingToDefault)

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

// Used to notify the render-view that we have received a target URL. Used
// to prevent target URLs spamming the browser.
IPC_MESSAGE_ROUTED0(ViewMsg_UpdateTargetURL_ACK)

IPC_MESSAGE_ROUTED1(ViewMsg_RunFileChooserResponse,
                    std::vector<content::FileChooserFileInfo>)

// Provides the results of directory enumeration.
IPC_MESSAGE_ROUTED2(ViewMsg_EnumerateDirectoryResponse,
                    int /* request_id */,
                    std::vector<base::FilePath> /* files_in_directory */)

// Tells the renderer to suppress any further modal dialogs until it receives a
// corresponding ViewMsg_SwapOut message.  This ensures that no
// PageGroupLoadDeferrer is on the stack for SwapOut.
IPC_MESSAGE_ROUTED0(ViewMsg_SuppressDialogsUntilSwapOut)

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

IPC_MESSAGE_ROUTED2(ViewMsg_UpdateScreenRects,
                    gfx::Rect /* view_screen_rect */,
                    gfx::Rect /* window_screen_rect */)

// Reply to ViewHostMsg_RequestMove, ViewHostMsg_ShowView, and
// ViewHostMsg_ShowWidget to inform the renderer that the browser has
// processed the move.  The browser may have ignored the move, but it finished
// processing.  This is used because the renderer keeps a temporary cache of
// the widget position while these asynchronous operations are in progress.
IPC_MESSAGE_ROUTED0(ViewMsg_Move_ACK)

// Used to instruct the RenderView to send back updates to the preferred size.
IPC_MESSAGE_ROUTED0(ViewMsg_EnablePreferredSizeChangedMode)

// Used to instruct the RenderView to automatically resize and send back
// updates for the new size.
IPC_MESSAGE_ROUTED2(ViewMsg_EnableAutoResize,
                    gfx::Size /* min_size */,
                    gfx::Size /* max_size */)

// Used to instruct the RenderView to disalbe automatically resize.
IPC_MESSAGE_ROUTED1(ViewMsg_DisableAutoResize,
                    gfx::Size /* new_size */)

// Changes the text direction of the currently selected input field (if any).
IPC_MESSAGE_ROUTED1(ViewMsg_SetTextDirection,
                    blink::WebTextDirection /* direction */)

// Tells the renderer to clear the focused element (if any).
IPC_MESSAGE_ROUTED0(ViewMsg_ClearFocusedElement)

// Make the RenderView background transparent or opaque.
IPC_MESSAGE_ROUTED1(ViewMsg_SetBackgroundOpaque, bool /* opaque */)

// Used to tell the renderer not to add scrollbars with height and
// width below a threshold.
IPC_MESSAGE_ROUTED1(ViewMsg_DisableScrollbarsForSmallWindows,
                    gfx::Size /* disable_scrollbar_size_limit */)

// Activate/deactivate the RenderView (i.e., set its controls' tint
// accordingly, etc.).
IPC_MESSAGE_ROUTED1(ViewMsg_SetActive,
                    bool /* active */)

// Response message to ViewHostMsg_CreateWorker.
// Sent when the worker has started.
IPC_MESSAGE_ROUTED0(ViewMsg_WorkerCreated)

// Sent when the worker failed to load the worker script.
// In normal cases, this message is sent after ViewMsg_WorkerCreated is sent.
// But if the shared worker of the same URL already exists and it has failed
// to load the script, when the renderer send ViewHostMsg_CreateWorker before
// the shared worker is killed only ViewMsg_WorkerScriptLoadFailed is sent.
IPC_MESSAGE_ROUTED0(ViewMsg_WorkerScriptLoadFailed)

// Sent when the worker has connected.
// This message is sent only if the worker successfully loaded the script.
IPC_MESSAGE_ROUTED0(ViewMsg_WorkerConnected)

// Tells the renderer that the network type has changed so that navigator.onLine
// and navigator.connection can be updated.
IPC_MESSAGE_CONTROL2(ViewMsg_NetworkConnectionChanged,
                     net::NetworkChangeNotifier::ConnectionType /* type */,
                     double /* max bandwidth mbps */)

#if defined(ENABLE_PLUGINS)
// Reply to ViewHostMsg_OpenChannelToPpapiBroker
// Tells the renderer that the channel to the broker has been created.
IPC_MESSAGE_ROUTED2(ViewMsg_PpapiBrokerChannelCreated,
                    base::ProcessId /* broker_pid */,
                    IPC::ChannelHandle /* handle */)

// Reply to ViewHostMsg_RequestPpapiBrokerPermission.
// Tells the renderer whether permission to access to PPAPI broker was granted
// or not.
IPC_MESSAGE_ROUTED1(ViewMsg_PpapiBrokerPermissionResult,
                    bool /* result */)

// Tells the renderer to empty its plugin list cache, optional reloading
// pages containing plugins.
IPC_MESSAGE_CONTROL1(ViewMsg_PurgePluginListCache,
                     bool /* reload_pages */)
#endif

// Used to instruct the RenderView to go into "view source" mode.
IPC_MESSAGE_ROUTED0(ViewMsg_EnableViewSourceMode)

// An acknowledge to ViewHostMsg_MultipleTargetsTouched to notify the renderer
// process to release the magnified image.
IPC_MESSAGE_ROUTED1(ViewMsg_ReleaseDisambiguationPopupBitmap,
                    cc::SharedBitmapId /* id */)

// Fetches complete rendered content of a web page as plain text.
IPC_MESSAGE_ROUTED0(ViewMsg_GetRenderedText)

#if defined(OS_MACOSX)
// Notification of a change in scrollbar appearance and/or behavior.
IPC_MESSAGE_CONTROL1(ViewMsg_UpdateScrollbarTheme,
                     ViewMsg_UpdateScrollbarTheme_Params /* params */)

// Notification that the OS X Aqua color preferences changed.
IPC_MESSAGE_CONTROL3(ViewMsg_SystemColorsChanged,
                     int /* AppleAquaColorVariant */,
                     std::string /* AppleHighlightedTextColor */,
                     std::string /* AppleHighlightColor */)
#endif

#if defined(OS_ANDROID)
// Tells the renderer to suspend/resume the webkit timers.
IPC_MESSAGE_CONTROL1(ViewMsg_SetWebKitSharedTimersSuspended,
                     bool /* suspend */)

// Sent when the browser wants the bounding boxes of the current find matches.
//
// If match rects are already cached on the browser side, |current_version|
// should be the version number from the ViewHostMsg_FindMatchRects_Reply
// they came in, so the renderer can tell if it needs to send updated rects.
// Otherwise just pass -1 to always receive the list of rects.
//
// There must be an active search string (it is probably most useful to call
// this immediately after a ViewHostMsg_Find_Reply message arrives with
// final_update set to true).
IPC_MESSAGE_ROUTED1(ViewMsg_FindMatchRects,
                    int /* current_version */)

// Notifies the renderer whether hiding/showing the top controls is enabled
// and whether or not to animate to the proper state.
IPC_MESSAGE_ROUTED3(ViewMsg_UpdateTopControlsState,
                    bool /* enable_hiding */,
                    bool /* enable_showing */,
                    bool /* animate */)

IPC_MESSAGE_ROUTED0(ViewMsg_ShowImeIfNeeded)

// Extracts the data at the given rect, returning it through the
// ViewHostMsg_SmartClipDataExtracted IPC.
IPC_MESSAGE_ROUTED1(ViewMsg_ExtractSmartClipData,
                    gfx::Rect /* rect */)

#elif defined(OS_MACOSX)
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
                    base::string16 /* text */,
                    int /* plugin_id */)
#endif

// Sent by the browser as a reply to ViewHostMsg_SwapCompositorFrame.
IPC_MESSAGE_ROUTED2(ViewMsg_SwapCompositorFrameAck,
                    uint32_t /* output_surface_id */,
                    cc::CompositorFrameAck /* ack */)

// Sent by browser to tell renderer compositor that some resources that were
// given to the browser in a swap are not being used anymore.
IPC_MESSAGE_ROUTED2(ViewMsg_ReclaimCompositorResources,
                    uint32_t /* output_surface_id */,
                    cc::CompositorFrameAck /* ack */)

// Sent by browser to give renderer compositor a new namespace ID for any
// SurfaceSequences it has to create.
IPC_MESSAGE_ROUTED1(ViewMsg_SetSurfaceIdNamespace,
                    uint32_t /* surface_id_namespace */)

IPC_MESSAGE_ROUTED0(ViewMsg_SelectWordAroundCaret)

// Sent by the browser to ask the renderer to redraw.
// If |request_id| is not zero, it is added to the forced frame's latency info
// as ui::WINDOW_SNAPSHOT_FRAME_NUMBER_COMPONENT.
IPC_MESSAGE_ROUTED1(ViewMsg_ForceRedraw,
                    int /* request_id */)

// Sent by the browser when the renderer should generate a new frame.
IPC_MESSAGE_ROUTED1(ViewMsg_BeginFrame,
                    cc::BeginFrameArgs /* args */)

// Sent by the browser to deliver a compositor proto to the renderer.
IPC_MESSAGE_ROUTED1(ViewMsg_HandleCompositorProto,
                    std::vector<uint8_t> /* proto */)

// -----------------------------------------------------------------------------
// Messages sent from the renderer to the browser.

// Sent by renderer to request a ViewMsg_BeginFrame message for upcoming
// display events. If |enabled| is true, the BeginFrame message will continue
// to be be delivered until the notification is disabled.
IPC_MESSAGE_ROUTED1(ViewHostMsg_SetNeedsBeginFrames,
                    bool /* enabled */)

// Sent by the renderer when it is creating a new window.  The browser creates a
// tab for it.  If |reply.route_id| is MSG_ROUTING_NONE, the view couldn't be
// created.
IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_CreateWindow,
                            ViewHostMsg_CreateWindow_Params,
                            ViewHostMsg_CreateWindow_Reply)

// Similar to ViewHostMsg_CreateWindow, except used for sub-widgets, like
// <select> dropdowns.  This message is sent to the WebContentsImpl that
// contains the widget being created.
IPC_SYNC_MESSAGE_CONTROL2_1(ViewHostMsg_CreateWidget,
                            int /* opener_id */,
                            blink::WebPopupType /* popup type */,
                            int /* route_id */)

// Similar to ViewHostMsg_CreateWidget except the widget is a full screen
// window.
IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_CreateFullscreenWidget,
                            int /* opener_id */,
                            int /* route_id */)

// Asks the browser for a unique routing ID.
IPC_SYNC_MESSAGE_CONTROL0_1(ViewHostMsg_GenerateRoutingID,
                            int /* routing_id */)

// Asks the browser for the default audio hardware configuration.
IPC_SYNC_MESSAGE_CONTROL0_2(ViewHostMsg_GetAudioHardwareConfig,
                            media::AudioParameters /* input parameters */,
                            media::AudioParameters /* output parameters */)

// These three messages are sent to the parent RenderViewHost to display the
// page/widget that was created by
// CreateWindow/CreateWidget/CreateFullscreenWidget. routing_id
// refers to the id that was returned from the Create message above.
// The initial_rect parameter is in screen coordinates.
//
// FUTURE: there will probably be flags here to control if the result is
// in a new window.
IPC_MESSAGE_ROUTED4(ViewHostMsg_ShowView,
                    int /* route_id */,
                    WindowOpenDisposition /* disposition */,
                    gfx::Rect /* initial_rect */,
                    bool /* opened_by_user_gesture */)

IPC_MESSAGE_ROUTED2(ViewHostMsg_ShowWidget,
                    int /* route_id */,
                    gfx::Rect /* initial_rect */)

// Message to show a full screen widget.
IPC_MESSAGE_ROUTED1(ViewHostMsg_ShowFullscreenWidget,
                    int /* route_id */)

// Sent by the renderer process to request that the browser close the view.
// This corresponds to the window.close() API, and the browser may ignore
// this message.  Otherwise, the browser will generates a ViewMsg_Close
// message to close the view.
IPC_MESSAGE_ROUTED0(ViewHostMsg_Close)

// Send in response to a ViewMsg_UpdateScreenRects so that the renderer can
// throttle these messages.
IPC_MESSAGE_ROUTED0(ViewHostMsg_UpdateScreenRects_ACK)

// Sent by the renderer process to request that the browser move the view.
// This corresponds to the window.resizeTo() and window.moveTo() APIs, and
// the browser may ignore this message.
IPC_MESSAGE_ROUTED1(ViewHostMsg_RequestMove,
                    gfx::Rect /* position */)

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

// Indicates that the render view has been closed in respose to a
// Close message.
IPC_MESSAGE_CONTROL1(ViewHostMsg_Close_ACK,
                     int /* old_route_id */)

// Indicates that the current page has been closed, after a ClosePage
// message.
IPC_MESSAGE_ROUTED0(ViewHostMsg_ClosePage_ACK)

// Notifies the browser that we have session history information.
// page_id: unique ID that allows us to distinguish between history entries.
IPC_MESSAGE_ROUTED2(ViewHostMsg_UpdateState,
                    int32_t /* page_id */,
                    content::PageState /* state */)

// Notifies the browser that we want to show a destination url for a potential
// action (e.g. when the user is hovering over a link).
IPC_MESSAGE_ROUTED1(ViewHostMsg_UpdateTargetURL,
                    GURL)

// Sent when the document element is available for the top-level frame.  This
// happens after the page starts loading, but before all resources are
// finished.
IPC_MESSAGE_ROUTED1(ViewHostMsg_DocumentAvailableInMainFrame,
                    bool /* uses_temporary_zoom_level */)

// Sent to update part of the view.  In response to this message, the host
// generates a ViewMsg_UpdateRect_ACK message.
IPC_MESSAGE_ROUTED1(ViewHostMsg_UpdateRect,
                    ViewHostMsg_UpdateRect_Params)

IPC_MESSAGE_ROUTED0(ViewHostMsg_Focus)

// Message sent from renderer to the browser when focus changes inside the
// webpage. The first parameter says whether the newly focused element needs
// keyboard input (true for textfields, text areas and content editable divs).
// The second parameter is the node bounds relative to RenderWidgetHostView.
IPC_MESSAGE_ROUTED2(ViewHostMsg_FocusedNodeChanged,
                    bool /* is_editable_node */,
                    gfx::Rect /* node_bounds */)

IPC_MESSAGE_ROUTED1(ViewHostMsg_SetCursor, content::WebCursor)

#if defined(OS_WIN)
IPC_MESSAGE_ROUTED1(ViewHostMsg_WindowlessPluginDummyWindowCreated,
                    gfx::NativeViewId /* dummy_activation_window */)

IPC_MESSAGE_ROUTED1(ViewHostMsg_WindowlessPluginDummyWindowDestroyed,
                    gfx::NativeViewId /* dummy_activation_window */)
#endif

// Get the list of proxies to use for |url|, as a semicolon delimited list
// of "<TYPE> <HOST>:<PORT>" | "DIRECT".
IPC_SYNC_MESSAGE_CONTROL1_2(ViewHostMsg_ResolveProxy,
                            GURL /* url */,
                            bool /* result */,
                            std::string /* proxy list */)

// A renderer sends this to the browser process when it wants to create a
// worker.  The browser will create the worker process if necessary, and
// will return the route id on in the reply on success.  On error returns
// MSG_ROUTING_NONE and an error type.
IPC_SYNC_MESSAGE_CONTROL1_1(ViewHostMsg_CreateWorker,
                            ViewHostMsg_CreateWorker_Params,
                            ViewHostMsg_CreateWorker_Reply)

// A renderer sends this to the browser process when a document has been
// detached. The browser will use this to constrain the lifecycle of worker
// processes (SharedWorkers are shut down when their last associated document
// is detached).
IPC_MESSAGE_CONTROL1(ViewHostMsg_DocumentDetached, uint64_t /* document_id */)

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
IPC_MESSAGE_CONTROL5(ViewHostMsg_DownloadUrl,
                     int /* render_view_id */,
                     int /* render_frame_id */,
                     GURL /* url */,
                     content::Referrer /* referrer */,
                     base::string16 /* suggested_name */)

// Used to go to the session history entry at the given offset (ie, -1 will
// return the "back" item).
IPC_MESSAGE_ROUTED1(ViewHostMsg_GoToEntryAtOffset,
                    int /* offset (from current) of history item to get */)

// Sent from an inactive renderer for the browser to route to the active
// renderer, instructing it to close.
IPC_MESSAGE_ROUTED0(ViewHostMsg_RouteCloseEvent)

// Notifies that the preferred size of the content changed.
IPC_MESSAGE_ROUTED1(ViewHostMsg_DidContentsPreferredSizeChange,
                    gfx::Size /* pref_size */)

// Notifies whether there are JavaScript touch event handlers or not.
IPC_MESSAGE_ROUTED1(ViewHostMsg_HasTouchEventHandlers,
                    bool /* has_handlers */)

// A message from HTML-based UI.  When (trusted) Javascript calls
// send(message, args), this message is sent to the browser.
IPC_MESSAGE_ROUTED3(ViewHostMsg_WebUISend,
                    GURL /* source_url */,
                    std::string  /* message */,
                    base::ListValue /* args */)

#if defined(ENABLE_PLUGINS)
// A renderer sends this to the browser process when it wants to access a PPAPI
// broker. In contrast to FrameHostMsg_OpenChannelToPpapiBroker, this is called
// for every connection.
// The browser will respond with ViewMsg_PpapiBrokerPermissionResult.
IPC_MESSAGE_ROUTED3(ViewHostMsg_RequestPpapiBrokerPermission,
                    int /* routing_id */,
                    GURL /* document_url */,
                    base::FilePath /* plugin_path */)
#endif  // defined(ENABLE_PLUGINS)

// Send the tooltip text for the current mouse position to the browser.
IPC_MESSAGE_ROUTED2(ViewHostMsg_SetTooltipText,
                    base::string16 /* tooltip text string */,
                    blink::WebTextDirection /* text direction hint */)

// Notification that the text selection has changed.
// Note: The secound parameter is the character based offset of the
// base::string16
// text in the document.
IPC_MESSAGE_ROUTED3(ViewHostMsg_SelectionChanged,
                    base::string16 /* text covers the selection range */,
                    size_t /* the offset of the text in the document */,
                    gfx::Range /* selection range in the document */)

// Notification that the selection bounds have changed.
IPC_MESSAGE_ROUTED1(ViewHostMsg_SelectionBoundsChanged,
                    ViewHostMsg_SelectionBounds_Params)

// Asks the browser to display the file chooser.  The result is returned in a
// ViewMsg_RunFileChooserResponse message.
IPC_MESSAGE_ROUTED1(ViewHostMsg_RunFileChooser,
                    content::FileChooserParams)

// Asks the browser to enumerate a directory.  This is equivalent to running
// the file chooser in directory-enumeration mode and having the user select
// the given directory.  The result is returned in a
// ViewMsg_EnumerateDirectoryResponse message.
IPC_MESSAGE_ROUTED2(ViewHostMsg_EnumerateDirectory,
                    int /* request_id */,
                    base::FilePath /* file_path */)

// Asks the browser to save a image (for <canvas> or <img>) from a data URL.
// Note: |data_url| is the contents of a data:URL, and that it's represented as
// a string only to work around size limitations for GURLs in IPC messages.
IPC_MESSAGE_CONTROL3(ViewHostMsg_SaveImageFromDataURL,
                     int /* render_view_id */,
                     int /* render_frame_id */,
                     std::string /* data_url */)

// Tells the browser to move the focus to the next (previous if reverse is
// true) focusable element.
IPC_MESSAGE_ROUTED1(ViewHostMsg_TakeFocus,
                    bool /* reverse */)

// Required for opening a date/time dialog
IPC_MESSAGE_ROUTED1(ViewHostMsg_OpenDateTimeDialog,
                    ViewHostMsg_DateTimeDialogValue_Params /* value */)

// Required for updating text input state.
IPC_MESSAGE_ROUTED1(ViewHostMsg_TextInputStateChanged,
                    ViewHostMsg_TextInputState_Params /* input state params */)

// Sent when the renderer changes the zoom level for a particular url, so the
// browser can update its records.  If the view is a plugin doc, then url is
// used to update the zoom level for all pages in that site.  Otherwise, the
// render view's id is used so that only the menu is updated.
IPC_MESSAGE_ROUTED2(ViewHostMsg_DidZoomURL,
                    double /* zoom_level */,
                    GURL /* url */)

// Sent when the renderer changes its page scale factor.
IPC_MESSAGE_ROUTED1(ViewHostMsg_PageScaleFactorChanged,
                    float /* page_scale_factor */)

// Updates the minimum/maximum allowed zoom percent for this tab from the
// default values.  If |remember| is true, then the zoom setting is applied to
// other pages in the site and is saved, otherwise it only applies to this
// tab.
IPC_MESSAGE_ROUTED2(ViewHostMsg_UpdateZoomLimits,
                    int /* minimum_percent */,
                    int /* maximum_percent */)

IPC_MESSAGE_ROUTED3(
    ViewHostMsg_SwapCompositorFrame,
    uint32_t /* output_surface_id */,
    cc::CompositorFrame /* frame */,
    std::vector<IPC::Message> /* messages_to_deliver_with_frame */)

// Send back a string to be recorded by UserMetrics.
IPC_MESSAGE_CONTROL1(ViewHostMsg_UserMetricsRecordAction,
                     std::string /* action */)

// Notifies the browser of an event occurring in the media pipeline.
IPC_MESSAGE_CONTROL1(ViewHostMsg_MediaLogEvents,
                     std::vector<media::MediaLogEvent> /* events */)

// Requests to lock the mouse. Will result in a ViewMsg_LockMouse_ACK message
// being sent back.
// |privileged| is used by Pepper Flash. If this flag is set to true, we won't
// pop up a bubble to ask for user permission or take mouse lock content into
// account.
IPC_MESSAGE_ROUTED3(ViewHostMsg_LockMouse,
                    bool /* user_gesture */,
                    bool /* last_unlocked_by_target */,
                    bool /* privileged */)

// Requests to unlock the mouse. A ViewMsg_MouseLockLost message will be sent
// whenever the mouse is unlocked (which may or may not be caused by
// ViewHostMsg_UnlockMouse).
IPC_MESSAGE_ROUTED0(ViewHostMsg_UnlockMouse)

// Notifies that multiple touch targets may have been pressed, and to show
// the disambiguation popup.
IPC_MESSAGE_ROUTED3(ViewHostMsg_ShowDisambiguationPopup,
                    gfx::Rect, /* Border of touched targets */
                    gfx::Size, /* Size of zoomed image */
                    cc::SharedBitmapId /* id */)

// Notifies the browser that document has parsed the body. This is used by the
// ResourceScheduler as an indication that bandwidth contention won't block
// first paint.
IPC_MESSAGE_ROUTED0(ViewHostMsg_WillInsertBody)

// Notification that the urls for the favicon of a site has been determined.
IPC_MESSAGE_ROUTED1(ViewHostMsg_UpdateFaviconURL,
                    std::vector<content::FaviconURL> /* candidates */)

// Message sent from renderer to the browser when the element that is focused
// has been touched. A bool is passed in this message which indicates if the
// node is editable.
IPC_MESSAGE_ROUTED1(ViewHostMsg_FocusedNodeTouched,
                    bool /* editable */)

// Message sent from the renderer to the browser when an HTML form has failed
// validation constraints.
IPC_MESSAGE_ROUTED3(ViewHostMsg_ShowValidationMessage,
                    gfx::Rect /* anchor rectangle in root view coordinate */,
                    base::string16 /* validation message */,
                    base::string16 /* supplemental text */)

// Message sent from the renderer to the browser when a HTML form validation
// message should be hidden from view.
IPC_MESSAGE_ROUTED0(ViewHostMsg_HideValidationMessage)

// Message sent from the renderer to the browser when the suggested co-ordinates
// of the anchor for a HTML form validation message have changed.
IPC_MESSAGE_ROUTED1(ViewHostMsg_MoveValidationMessage,
                    gfx::Rect /* anchor rectangle in root view coordinate */)

// Sent once a paint happens after the first non empty layout. In other words,
// after the frame widget has painted something.
IPC_MESSAGE_ROUTED0(ViewHostMsg_DidFirstVisuallyNonEmptyPaint)

// Send after a paint happens after any page commit, including a blank one.
// TODO(kenrb): This, and all ViewHostMsg_* messages that actually pertain to
// RenderWidget(Host), should be renamed to WidgetHostMsg_*.
// See https://crbug.com/537793.
IPC_MESSAGE_ROUTED0(ViewHostMsg_DidFirstPaintAfterLoad)

// Sent by the renderer to deliver a compositor proto to the browser.
IPC_MESSAGE_ROUTED1(ViewHostMsg_ForwardCompositorProto,
                    std::vector<uint8_t> /* proto */)

#if defined(OS_ANDROID)
// Response to ViewMsg_FindMatchRects.
//
// |version| will contain the current version number of the renderer's find
// match list (incremented whenever they change), which should be passed in the
// next call to ViewMsg_FindMatchRects.
//
// |rects| will either contain a list of the enclosing rects of all matches
// found by the most recent Find operation, or will be empty if |version| is not
// greater than the |current_version| passed to ViewMsg_FindMatchRects (hence
// your locally cached rects should still be valid). The rect coords will be
// custom normalized fractions of the document size. The rects will be sorted by
// frame traversal order starting in the main frame, then by dom order.
//
// |active_rect| will contain the bounding box of the active find-in-page match
// marker, in similarly normalized coords (or an empty rect if there isn't one).
IPC_MESSAGE_ROUTED3(ViewHostMsg_FindMatchRects_Reply,
                    int /* version */,
                    std::vector<gfx::RectF> /* rects */,
                    gfx::RectF /* active_rect */)

// Start an android intent with the given URI.
IPC_MESSAGE_ROUTED2(ViewHostMsg_StartContentIntent,
                    GURL /* content_url */,
                    bool /* is_main_frame */)

// Reply to the ViewMsg_ExtractSmartClipData message.
IPC_MESSAGE_ROUTED3(ViewHostMsg_SmartClipDataExtracted,
                    base::string16 /* text */,
                    base::string16 /* html */,
                    gfx::Rect /* rect */)

// Notifies that an unhandled tap has occurred at the specified x,y position
// and that the UI may need to be triggered.
IPC_MESSAGE_ROUTED2(ViewHostMsg_ShowUnhandledTapUIIfNeeded,
                    int /* x */,
                    int /* y */)

#elif defined(OS_MACOSX)
// Informs the browser that a plugin has gained or lost focus.
IPC_MESSAGE_ROUTED2(ViewHostMsg_PluginFocusChanged,
                    bool, /* focused */
                    int /* plugin_id */)

// Instructs the browser to start plugin IME.
IPC_MESSAGE_ROUTED0(ViewHostMsg_StartPluginIme)

// Receives content of a web page as plain text.
IPC_MESSAGE_ROUTED1(ViewMsg_GetRenderedTextCompleted, std::string)
#endif

// Adding a new message? Stick to the sort order above: first platform
// independent ViewMsg, then ifdefs for platform specific ViewMsg, then platform
// independent ViewHostMsg, then ifdefs for platform specific ViewHostMsg.
