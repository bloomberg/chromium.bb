// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message header, no traditional include guard.

#include <string>

#include "base/basictypes.h"
#include "base/process.h"
#include "content/common/content_export.h"
#include "content/common/content_param_traits.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_utils.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "webkit/glue/webcursor.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START BrowserPluginMsgStart

// Browser plugin messages

// -----------------------------------------------------------------------------
// These messages are from the embedder to the browser process.

// This message is sent to the browser process to create the browser plugin
// embedder and helper. It is sent once prior to sending the first
// BrowserPluginHostMsg_NavigateGuest message.
IPC_MESSAGE_ROUTED3(BrowserPluginHostMsg_CreateGuest,
                    int /* instance_id */,
                    std::string /* storage_partition_id */,
                    bool /* persist_storage */)

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

/// Tell the guest to reload.
IPC_MESSAGE_ROUTED1(BrowserPluginHostMsg_Reload,
                    int /* instance_id */)

// Message payload includes:
// 1. A blob that should be cast to WebInputEvent
// 2. An optional boolean value indicating if a RawKeyDown event is associated
//    to a keyboard shortcut of the browser.
IPC_SYNC_MESSAGE_ROUTED0_2(BrowserPluginHostMsg_HandleInputEvent,
                           bool /* handled */,
                           WebCursor /* cursor */)

// An ACK to the guest process letting it know that the embedder has handled
// the previous frame and is ready for the next frame. If the guest sent the
// embedder a bitmap that does not match the size of the BrowserPlugin's
// container, the BrowserPlugin requests a new size as well.
IPC_MESSAGE_ROUTED3(BrowserPluginHostMsg_UpdateRect_ACK,
                    int /* instance_id */,
                    int /* message_id */,
                    gfx::Size /* repaint_view_size */)

IPC_STRUCT_BEGIN(BrowserPluginHostMsg_ResizeGuest_Params)
  // An identifier to the new buffer to use to transport damage to the embedder
  // renderer process.
  IPC_STRUCT_MEMBER(TransportDIB::Id, damage_buffer_id)
#if defined(OS_MACOSX)
  // On OSX, a handle to the new buffer is used to map the transport dib since
  // we don't let browser manage the dib.
  IPC_STRUCT_MEMBER(TransportDIB::Handle, damage_buffer_handle)
#endif
#if defined(OS_WIN)
  // The size of the damage buffer because this information is not available
  // on Windows.
  IPC_STRUCT_MEMBER(int, damage_buffer_size)
#endif
  // The new width of the plugin container.
  IPC_STRUCT_MEMBER(int, width)
  // The new height of the plugin container.
  IPC_STRUCT_MEMBER(int, height)
  // Indicates whether the embedder is currently waiting on a ACK from the
  // guest for a previous resize request.
  IPC_STRUCT_MEMBER(bool, resize_pending)
  // Indicates the scale factor of the embedder WebView.
  IPC_STRUCT_MEMBER(float, scale_factor)
IPC_STRUCT_END()

// A BrowserPlugin sends this to BrowserPluginEmbedder (browser process) when it
// wants to navigate to a given src URL. If a guest WebContents already exists,
// it will navigate that WebContents. If not, it will create the WebContents,
// associate it with the BrowserPluginGuest, and navigate it to the requested
// URL.
IPC_MESSAGE_ROUTED3(BrowserPluginHostMsg_NavigateGuest,
                    int /* instance_id*/,
                    std::string /* src */,
                    BrowserPluginHostMsg_ResizeGuest_Params /* resize_params */)

// When a BrowserPlugin has been removed from the embedder's DOM, it informs
// the browser process to cleanup the guest.
IPC_MESSAGE_ROUTED1(BrowserPluginHostMsg_PluginDestroyed,
                    int /* instance_id */)

// Tells the guest it has been shown or hidden.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_SetVisibility,
                    int /* instance_id */,
                    bool /* visible */)

// -----------------------------------------------------------------------------
// These messages are from the guest renderer to the browser process

// A embedder sends this message to the browser when it wants
// to resize a guest plugin container so that the guest is relaid out
// according to the new size.
IPC_SYNC_MESSAGE_ROUTED2_0(BrowserPluginHostMsg_ResizeGuest,
                           int /* instance_id*/,
                           BrowserPluginHostMsg_ResizeGuest_Params)

// -----------------------------------------------------------------------------
// These messages are from the browser process to the embedder.

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

IPC_STRUCT_BEGIN(BrowserPluginMsg_DidNavigate_Params)
  // The current URL of the guest.
  IPC_STRUCT_MEMBER(GURL, url)
  // Indicates whether the navigation was on the top-level frame.
  IPC_STRUCT_MEMBER(bool, is_top_level)
  // Chrome's process ID for the guest.
  IPC_STRUCT_MEMBER(int, process_id)
  // The index of the current navigation entry after this navigation was
  // committed.
  IPC_STRUCT_MEMBER(int, current_entry_index)
  // The number of navigation entries after this navigation was committed.
  IPC_STRUCT_MEMBER(int, entry_count)
IPC_STRUCT_END()

// When the guest navigates, the browser process informs the embedder through
// the BrowserPluginMsg_DidNavigate message along with information about the
// guest's current navigation state.
IPC_MESSAGE_CONTROL2(BrowserPluginMsg_DidNavigate,
                     int /* instance_id */,
                     BrowserPluginMsg_DidNavigate_Params)

// When the guest crashes, the browser process informs the embedder through this
// message.
IPC_MESSAGE_CONTROL1(BrowserPluginMsg_GuestCrashed,
                     int /* instance_id */)

IPC_STRUCT_BEGIN(BrowserPluginMsg_UpdateRect_Params)
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
  // progress. If auto-resize is enabled, this should update the corresponding
  // view size.
  IPC_STRUCT_MEMBER(gfx::Size, view_size)

  // All the above coordinates are in DIP. This is the scale factor needed
  // to convert them to pixels.
  IPC_STRUCT_MEMBER(float, scale_factor)

  // Is this UpdateRect an ACK to a resize request?
  IPC_STRUCT_MEMBER(bool, is_resize_ack)
IPC_STRUCT_END()

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

// The guest has damage it wants to convey to the embedder so that it can
// update its backing store.
IPC_MESSAGE_CONTROL3(BrowserPluginMsg_UpdateRect,
                     int /* instance_id */,
                     int /* message_id */,
                     BrowserPluginMsg_UpdateRect_Params)
