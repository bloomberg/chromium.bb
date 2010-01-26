// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header is meant to be included in multiple passes, hence no traditional
// header guard. It is included by backing_store_messages_internal.h
// See ipc_message_macros.h for explanation of the macros and passes.

// This file needs to be included again, even though we're actually included
// from it via utility_messages.h.
#include "ipc/ipc_message_macros.h"

//------------------------------------------------------------------------------
// Backing Store Messages
// These are messages from the browser to the GPU process.
IPC_BEGIN_MESSAGES(Gpu)

  IPC_MESSAGE_CONTROL2(GpuMsg_NewRenderWidgetHostView,
                       GpuNativeWindowHandle, /* parent window */
                       int32 /* view_id */)

  // Creates a new backing store.
  IPC_MESSAGE_ROUTED2(GpuMsg_NewBackingStore,
                      int32, /* backing_store_id */
                      gfx::Size /* size */)

  // Updates the backing store with the given bitmap. The GPU process will send
  // back a GpuHostMsg_PaintToBackingStore_ACK after the paint is complete to
  // let the caller know the TransportDIB can be freed or reused.
  IPC_MESSAGE_ROUTED4(GpuMsg_PaintToBackingStore,
                      base::ProcessId, /* process */
                      TransportDIB::Id, /* bitmap */
                      gfx::Rect, /* bitmap_rect */
                      std::vector<gfx::Rect>) /* copy_rects */


  IPC_MESSAGE_ROUTED4(GpuMsg_ScrollBackingStore,
                      int, /* dx */
                      int, /* dy */
                      gfx::Rect, /* clip_rect */
                      gfx::Size) /* view_size */

IPC_END_MESSAGES(Gpu)

//------------------------------------------------------------------------------
// Backing Store Host Messagse
// These are messages from the GPU process to the browser.
IPC_BEGIN_MESSAGES(GpuHost)

  // This message is sent in response to BackingStoreMsg_New to tell the host
  // about the child window that was just created.
  IPC_MESSAGE_ROUTED1(GpuHostMsg_CreatedRenderWidgetHostView,
                      gfx::NativeViewId)

  // Send in response to GpuMsg_PaintToBackingStore, see that for more.
  IPC_MESSAGE_ROUTED0(GpuHostMsg_PaintToBackingStore_ACK)

IPC_END_MESSAGES(GpuHost)

