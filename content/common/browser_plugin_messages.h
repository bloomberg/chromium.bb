// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message header, no traditional include guard.

#include <string>

#include "base/basictypes.h"
#include "base/process.h"
#include "base/shared_memory.h"
#include "base/values.h"
#include "content/common/browser_plugin_message_enums.h"
#include "content/common/content_export.h"
#include "content/common/content_param_traits.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragOperation.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragStatus.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webdropdata.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START BrowserPluginMsgStart


IPC_ENUM_TRAITS(BrowserPluginPermissionType)
IPC_ENUM_TRAITS(WebKit::WebDragStatus)

IPC_STRUCT_BEGIN(BrowserPluginHostMsg_AutoSize_Params)
  IPC_STRUCT_MEMBER(bool, enable)
  IPC_STRUCT_MEMBER(gfx::Size, max_size)
  IPC_STRUCT_MEMBER(gfx::Size, min_size)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(BrowserPluginHostMsg_ResizeGuest_Params)
  // The sequence number used to uniquely identify the damage buffer for the
  // current container size.
  IPC_STRUCT_MEMBER(uint32, damage_buffer_sequence_id)
  // The handle to use to map the damage buffer in the browser process.
  IPC_STRUCT_MEMBER(base::SharedMemoryHandle, damage_buffer_handle)
  // The size of the damage buffer.
  IPC_STRUCT_MEMBER(size_t, damage_buffer_size)
  // The new size of the guest view area.
  IPC_STRUCT_MEMBER(gfx::Size, view_size)
  // Indicates the scale factor of the embedder WebView.
  IPC_STRUCT_MEMBER(float, scale_factor)
  // Indicates a request for a full repaint of the page.
  IPC_STRUCT_MEMBER(bool, repaint)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(BrowserPluginHostMsg_CreateGuest_Params)
  IPC_STRUCT_MEMBER(std::string, storage_partition_id)
  IPC_STRUCT_MEMBER(bool, persist_storage)
  IPC_STRUCT_MEMBER(bool, focused)
  IPC_STRUCT_MEMBER(bool, visible)
  IPC_STRUCT_MEMBER(std::string, name)
  IPC_STRUCT_MEMBER(std::string, src)
  IPC_STRUCT_MEMBER(BrowserPluginHostMsg_AutoSize_Params, auto_size_params)
  IPC_STRUCT_MEMBER(BrowserPluginHostMsg_ResizeGuest_Params,
                    resize_guest_params)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(BrowserPluginMsg_LoadCommit_Params)
  // The current URL of the guest.
  IPC_STRUCT_MEMBER(GURL, url)
  // Indicates whether the navigation was on the top-level frame.
  IPC_STRUCT_MEMBER(bool, is_top_level)
  // The browser's process ID for the guest.
  IPC_STRUCT_MEMBER(int, process_id)
  // The browser's routing ID for the guest's RenderView.
  IPC_STRUCT_MEMBER(int, route_id)
  // The index of the current navigation entry after this navigation was
  // committed.
  IPC_STRUCT_MEMBER(int, current_entry_index)
  // The number of navigation entries after this navigation was committed.
  IPC_STRUCT_MEMBER(int, entry_count)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(BrowserPluginMsg_UpdateRect_Params)
  // The sequence number of the damage buffer used by the browser process.
  IPC_STRUCT_MEMBER(uint32, damage_buffer_sequence_id)

  // The position and size of the bitmap.
  IPC_STRUCT_MEMBER(gfx::Rect, bitmap_rect)

  // The scroll delta.  Only one of the delta components can be non-zero, and if
  // they are both zero, then it means there is no scrolling and the scroll_rect
  // is ignored.
  IPC_STRUCT_MEMBER(gfx::Vector2d, scroll_delta)

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
  // progress. If auto-resize is enabled, this should update the corresponding
  // view size.
  IPC_STRUCT_MEMBER(gfx::Size, view_size)

  // All the above coordinates are in DIP. This is the scale factor needed
  // to convert them to pixels.
  IPC_STRUCT_MEMBER(float, scale_factor)

  // Is this UpdateRect an ACK to a resize request?
  IPC_STRUCT_MEMBER(bool, is_resize_ack)

  // Used in HW accelerated case to switch between sending an UpdateRect_ACK
  // with the new size or just resizing.
  IPC_STRUCT_MEMBER(bool, needs_ack)
IPC_STRUCT_END()

// Browser plugin messages

// -----------------------------------------------------------------------------
// These messages are from the embedder to the browser process.

// This message is sent to the browser process to request an instance ID.
// |request_id| is used by BrowserPluginEmbedder to route the response back
// to its origin.
IPC_MESSAGE_ROUTED1(BrowserPluginHostMsg_AllocateInstanceID,
                    int /* request_id */)

// This message is sent to the browser process to enable or disable autosize
// mode.
IPC_MESSAGE_ROUTED3(
    BrowserPluginHostMsg_SetAutoSize,
    int /* instance_id */,
    BrowserPluginHostMsg_AutoSize_Params /* auto_size_params */,
    BrowserPluginHostMsg_ResizeGuest_Params /* resize_guest_params */)


// This message is sent to the browser process to create the browser plugin
// embedder and helper. It is sent once prior to sending the first
// BrowserPluginHostMsg_NavigateGuest message.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_CreateGuest,
                    int /* instance_id */,
                    BrowserPluginHostMsg_CreateGuest_Params /* params */)

// Tells the browser process to terminate the guest associated with the
// browser plugin associated with the provided |instance_id|.
IPC_MESSAGE_ROUTED1(BrowserPluginHostMsg_TerminateGuest,
                    int /* instance_id */)

// Tells the guest to navigate to an entry |relative_index| away from the
// current navigation entry.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_Go,
                    int /* instance_id */,
                    int /* relative_index */)

// Tells the guest to focus or defocus itself.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_SetFocus,
                    int /* instance_id */,
                    bool /* enable */)

// Tell the guest to stop loading.
IPC_MESSAGE_ROUTED1(BrowserPluginHostMsg_Stop,
                    int /* instance_id */)

// Tell the guest to reload.
IPC_MESSAGE_ROUTED1(BrowserPluginHostMsg_Reload,
                    int /* instance_id */)

// Sends an input event to the guest.
IPC_MESSAGE_ROUTED3(BrowserPluginHostMsg_HandleInputEvent,
                    int /* instance_id */,
                    gfx::Rect /* guest_window_rect */,
                    IPC::WebInputEventPointer /* event */)

// An ACK to the guest process letting it know that the embedder has handled
// the previous frame and is ready for the next frame. If the guest sent the
// embedder a bitmap that does not match the size of the BrowserPlugin's
// container, the BrowserPlugin requests a new size as well.
IPC_MESSAGE_ROUTED3(BrowserPluginHostMsg_UpdateRect_ACK,
    int /* instance_id */,
    BrowserPluginHostMsg_AutoSize_Params /* auto_size_params */,
    BrowserPluginHostMsg_ResizeGuest_Params /* resize_guest_params */)

// A BrowserPlugin sends this to BrowserPluginEmbedder (browser process) when it
// wants to navigate to a given src URL. If a guest WebContents already exists,
// it will navigate that WebContents. If not, it will create the WebContents,
// associate it with the BrowserPluginGuest, and navigate it to the requested
// URL.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_NavigateGuest,
                    int /* instance_id*/,
                    std::string /* src */)

// Acknowledge that we presented a HW buffer and provide a sync point
// to specify the location in the command stream when the compositor
// is no longer using it.
IPC_MESSAGE_ROUTED5(BrowserPluginHostMsg_BuffersSwappedACK,
                    int /* instance_id */,
                    int /* route_id */,
                    int /* gpu_host_id */,
                    std::string /* mailbox_name */,
                    uint32 /* sync_point */)

// When a BrowserPlugin has been removed from the embedder's DOM, it informs
// the browser process to cleanup the guest.
IPC_MESSAGE_ROUTED1(BrowserPluginHostMsg_PluginDestroyed,
                    int /* instance_id */)

// Tells the guest it has been shown or hidden.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_SetVisibility,
                    int /* instance_id */,
                    bool /* visible */)

// Tells the guest that a drag event happened on the plugin.
IPC_MESSAGE_ROUTED5(BrowserPluginHostMsg_DragStatusUpdate,
                    int /* instance_id */,
                    WebKit::WebDragStatus /* drag_status */,
                    WebDropData /* drop_data */,
                    WebKit::WebDragOperationsMask /* operation_mask */,
                    gfx::Point /* plugin_location */)

// Response to BrowserPluginMsg_PluginAtPositionRequest, returns the browser
// plugin instace id and the coordinates (local to the plugin).
IPC_MESSAGE_ROUTED3(BrowserPluginHostMsg_PluginAtPositionResponse,
                    int /* instance_id */,
                    int /* request_id */,
                    gfx::Point /* position */)

// Sets the name of the guest window to the provided |name|.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_SetName,
                    int /* instance_id */,
                    std::string /* name */)

// Tells the guest that its request for an API permission has been allowed or
// denied.
// Note that |allow| = true does not readily mean that the guest will be granted
// permission, since a security check in the embedder might follow. For example
// for media access permission, the guest will be granted permission only if its
// embedder also has access.
IPC_MESSAGE_ROUTED4(BrowserPluginHostMsg_RespondPermission,
                    int /* instance_id */,
                    BrowserPluginPermissionType /* permission_type */,
                    int /* request_id */,
                    bool /* allow */)

// Sends a PointerLock Lock ACK to the BrowserPluginGuest.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_LockMouse_ACK,
                    int /* instance_id */,
                    bool /* succeeded */)

// Sends a PointerLock Unlock ACK to the BrowserPluginGuest.
IPC_MESSAGE_ROUTED1(BrowserPluginHostMsg_UnlockMouse_ACK, int /* instance_id */)

// -----------------------------------------------------------------------------
// These messages are from the guest renderer to the browser process

// A embedder sends this message to the browser when it wants
// to resize a guest plugin container so that the guest is relaid out
// according to the new size.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_ResizeGuest,
                    int /* instance_id*/,
                    BrowserPluginHostMsg_ResizeGuest_Params)

// -----------------------------------------------------------------------------
// These messages are from the browser process to the embedder.

// This message is sent from the browser process to the embedder render process
// in response to a request to allocate an instance ID. The |request_id| is used
// to route the response to the requestor.
IPC_MESSAGE_ROUTED2(BrowserPluginMsg_AllocateInstanceID_ACK,
                    int /* request_id */,
                    int /* instance_id */)

// Once the swapped out guest RenderView has been created in the embedder render
// process, the browser process informs the embedder of its routing ID.
IPC_MESSAGE_CONTROL2(BrowserPluginMsg_GuestContentWindowReady,
                     int /* instance_id */,
                     int /* source_routing_id */)

// When the guest begins to load a page, the browser process informs the
// embedder through the BrowserPluginMsg_LoadStart message.
IPC_MESSAGE_CONTROL3(BrowserPluginMsg_LoadStart,
                     int /* instance_id */,
                     GURL /* url */,
                     bool /* is_top_level */)

// If the guest fails to commit a page load then it will inform the
// embedder through the BrowserPluginMsg_LoadAbort. A description
// of the error will be stored in |type|.  The list of known error
// types can be found in net/base/net_error_list.h.
IPC_MESSAGE_CONTROL4(BrowserPluginMsg_LoadAbort,
                     int /* instance_id */,
                     GURL /* url */,
                     bool /* is_top_level */,
                     std::string /* type */)

// When the guest redirects a navigation, the browser process informs the
// embedder through the BrowserPluginMsg_LoadRedirect message.
IPC_MESSAGE_CONTROL4(BrowserPluginMsg_LoadRedirect,
                     int /* instance_id */,
                     GURL /* old_url */,
                     GURL /* new_url */,
                     bool /* is_top_level */)

// When the guest commits a navigation, the browser process informs
// the embedder through the BrowserPluginMsg_LoadCommit message.
IPC_MESSAGE_CONTROL2(BrowserPluginMsg_LoadCommit,
                     int /* instance_id */,
                     BrowserPluginMsg_LoadCommit_Params)

// When the guest page has completed loading (including subframes), the browser
// process informs the embedder through the BrowserPluginMsg_LoadStop message.
IPC_MESSAGE_CONTROL1(BrowserPluginMsg_LoadStop,
                     int /* instance_id */)

// When the guest crashes, the browser process informs the embedder through this
// message.
IPC_MESSAGE_CONTROL3(BrowserPluginMsg_GuestGone,
                     int /* instance_id */,
                     int /* process_id */,
                     int /* This is really base::TerminationStatus */)

// When the guest is unresponsive, the browser process informs the embedder
// through this message.
IPC_MESSAGE_CONTROL2(BrowserPluginMsg_GuestUnresponsive,
                     int /* instance_id */,
                     int /* process_id */)

// When the guest begins responding again, the browser process informs the
// embedder through this message.
IPC_MESSAGE_CONTROL2(BrowserPluginMsg_GuestResponsive,
                     int /* instance_id */,
                     int /* process_id */)

// When the user tabs to the end of the tab stops of a guest, the browser
// process informs the embedder to tab out of the browser plugin.
IPC_MESSAGE_CONTROL2(BrowserPluginMsg_AdvanceFocus,
                     int /* instance_id */,
                     bool /* reverse */)

// When the guest starts/stops listening to touch events, it needs to notify the
// plugin in the embedder about it.
IPC_MESSAGE_CONTROL2(BrowserPluginMsg_ShouldAcceptTouchEvents,
                     int /* instance_id */,
                     bool /* accept */)

// Inform the embedder of the cursor the guest wishes to display.
IPC_MESSAGE_CONTROL2(BrowserPluginMsg_SetCursor,
                     int /* instance_id */,
                     WebCursor /* cursor */)

// The guest has damage it wants to convey to the embedder so that it can
// update its backing store.
IPC_MESSAGE_CONTROL2(BrowserPluginMsg_UpdateRect,
                     int /* instance_id */,
                     BrowserPluginMsg_UpdateRect_Params)

// Requests the renderer to find out if a browser plugin is at position
// (|x|, |y|) within the embedder.
// The response message is BrowserPluginHostMsg_PluginAtPositionResponse.
// The |request_id| uniquely identifies a request from an embedder.
IPC_MESSAGE_ROUTED2(BrowserPluginMsg_PluginAtPositionRequest,
                    int /* request_id */,
                    gfx::Point /* position */)

// Informs BrowserPlugin of a new name set for the top-level guest frame.
IPC_MESSAGE_CONTROL2(BrowserPluginMsg_UpdatedName,
                     int /* instance_id */,
                     std::string /* name */)

// Guest renders into an FBO with textures provided by the embedder.
// When HW accelerated buffers are swapped in the guest, the message
// is forwarded to the embedder to notify it of a new texture
// available for compositing.
IPC_MESSAGE_CONTROL5(BrowserPluginMsg_BuffersSwapped,
                     int /* instance_id */,
                     gfx::Size /* size */,
                     std::string /* mailbox_name */,
                     int /* route_id */,
                     int /* gpu_host_id */)

// When the guest requests permission, the browser process forwards this
// request to the embeddder through this message.
IPC_MESSAGE_CONTROL4(BrowserPluginMsg_RequestPermission,
                     int /* instance_id */,
                     BrowserPluginPermissionType /* permission_type */,
                     int /* request_id */,
                     DictionaryValue /* request_info */)

// Forwards a PointerLock Lock request to the BrowserPlugin.
IPC_MESSAGE_ROUTED4(BrowserPluginMsg_LockMouse,
                    int /* instance_id */,
                    bool /* user_gesture */,
                    bool /* last_unlocked_by_target */,
                    bool /* privileged */)

// Forwards a PointerLock Unlock request to the BrowserPlugin.
IPC_MESSAGE_ROUTED1(BrowserPluginMsg_UnlockMouse, int /* instance_id */)

