// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message header, no traditional include guard.

#include <string>

#include "base/basictypes.h"
#include "base/process.h"
#include "content/common/content_export.h"
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

// Tells the guest to focus or defocus itself.
IPC_MESSAGE_ROUTED2(BrowserPluginHostMsg_SetFocus,
                    int /* instance_id */,
                    bool /* enable */)

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

// A BrowserPlugin sends this to the browser process when it wants to navigate
// to a given src URL. If a guest WebContents already exists, it will navigate
// that WebContents. If not, it will create the WebContents, associate it with
// the BrowserPlugin's browser-side BrowserPluginHost as a guest, and navigate
// it to the requested URL.
IPC_MESSAGE_ROUTED4(BrowserPluginHostMsg_NavigateGuest,
                    int /* instance_id*/,
                    int64 /* frame_id */,
                    std::string /* src */,
                    gfx::Size /* size */)

// When a BrowserPlugin has been removed from the embedder's DOM, it informs
// the browser process to cleanup the guest.
IPC_MESSAGE_ROUTED1(BrowserPluginHostMsg_PluginDestroyed,
                    int /* instance_id */)

// -----------------------------------------------------------------------------
// These messages are from the guest renderer to the browser process

IPC_STRUCT_BEGIN(BrowserPluginHostMsg_ResizeGuest_Params)
  // A handle to the new buffer to use to transport damage to the
  // embedder renderer process.
  IPC_STRUCT_MEMBER(TransportDIB::Id, damage_buffer_id)
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

// A embedder sends this message to the browser when it wants
// to resize a guest plugin container so that the guest is relaid out
// according to the new size.
IPC_SYNC_MESSAGE_ROUTED2_0(BrowserPluginHostMsg_ResizeGuest,
                           int /* instance_id*/,
                           BrowserPluginHostMsg_ResizeGuest_Params)

// -----------------------------------------------------------------------------
// These messages are from the browser process to the embedder.

// When the guest navigates, the browser process informs the embedder through
// the BrowserPluginMsg_DidNavigate message.
IPC_MESSAGE_CONTROL2(BrowserPluginMsg_DidNavigate,
                     int /* instance_id */,
                     GURL /* url */)

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

// The guest has damage it wants to convey to the embedder so that it can
// update its backing store.
IPC_MESSAGE_CONTROL3(BrowserPluginMsg_UpdateRect,
                     int /* instance_id */,
                     int /* message_id */,
                     BrowserPluginMsg_UpdateRect_Params)
