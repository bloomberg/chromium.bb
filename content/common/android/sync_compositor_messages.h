// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/memory/shared_memory_handle.h"
#include "cc/output/begin_frame_args.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "content/common/content_export.h"
#include "content/common/content_param_traits.h"
#include "content/common/input/did_overscroll_params.h"
#include "content/common/input/input_event_ack_state.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/gfx/geometry/scroll_offset.h"

#ifndef CONTENT_COMMON_ANDROID_SYNC_COMPOSITOR_MESSAGES_H_
#define CONTENT_COMMON_ANDROID_SYNC_COMPOSITOR_MESSAGES_H_

namespace content {

struct SyncCompositorCommonBrowserParams {
  SyncCompositorCommonBrowserParams();
  ~SyncCompositorCommonBrowserParams();

  size_t bytes_limit;
  cc::CompositorFrameAck ack;
  gfx::ScrollOffset root_scroll_offset;
  bool update_root_scroll_offset;
  bool begin_frame_source_paused;
};

struct SyncCompositorDemandDrawHwParams {
  SyncCompositorDemandDrawHwParams();
  SyncCompositorDemandDrawHwParams(
      const gfx::Size& surface_size,
      const gfx::Transform& transform,
      const gfx::Rect& viewport,
      const gfx::Rect& clip,
      const gfx::Rect& viewport_rect_for_tile_priority,
      const gfx::Transform& transform_for_tile_priority);
  ~SyncCompositorDemandDrawHwParams();

  gfx::Size surface_size;
  gfx::Transform transform;
  gfx::Rect viewport;
  gfx::Rect clip;
  gfx::Rect viewport_rect_for_tile_priority;
  gfx::Transform transform_for_tile_priority;
};

struct SyncCompositorSetSharedMemoryParams {
  SyncCompositorSetSharedMemoryParams();

  size_t buffer_size;
  base::SharedMemoryHandle shm_handle;
};

struct SyncCompositorDemandDrawSwParams {
  SyncCompositorDemandDrawSwParams();
  ~SyncCompositorDemandDrawSwParams();

  gfx::Size size;
  gfx::Rect clip;
  gfx::Transform transform;
};

struct SyncCompositorCommonRendererParams {
  SyncCompositorCommonRendererParams();
  ~SyncCompositorCommonRendererParams();

  unsigned int version;
  gfx::ScrollOffset total_scroll_offset;
  gfx::ScrollOffset max_scroll_offset;
  gfx::SizeF scrollable_size;
  float page_scale_factor;
  float min_page_scale_factor;
  float max_page_scale_factor;
  bool need_animate_scroll;
  bool need_invalidate;
  bool need_begin_frame;
  bool did_activate_pending_tree;
};

}  // namespace content

#endif  // CONTENT_COMMON_ANDROID_SYNC_COMPOSITOR_MESSAGES_H_

// Multiply-included message file, hence no include guard.

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START SyncCompositorMsgStart

IPC_STRUCT_TRAITS_BEGIN(content::SyncCompositorCommonBrowserParams)
  IPC_STRUCT_TRAITS_MEMBER(bytes_limit)
  IPC_STRUCT_TRAITS_MEMBER(ack)
  IPC_STRUCT_TRAITS_MEMBER(root_scroll_offset)
  IPC_STRUCT_TRAITS_MEMBER(update_root_scroll_offset)
  IPC_STRUCT_TRAITS_MEMBER(begin_frame_source_paused)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::SyncCompositorDemandDrawHwParams)
  IPC_STRUCT_TRAITS_MEMBER(surface_size)
  IPC_STRUCT_TRAITS_MEMBER(transform)
  IPC_STRUCT_TRAITS_MEMBER(viewport)
  IPC_STRUCT_TRAITS_MEMBER(clip)
  IPC_STRUCT_TRAITS_MEMBER(viewport_rect_for_tile_priority)
  IPC_STRUCT_TRAITS_MEMBER(transform_for_tile_priority)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::SyncCompositorSetSharedMemoryParams)
  IPC_STRUCT_TRAITS_MEMBER(buffer_size)
  IPC_STRUCT_TRAITS_MEMBER(shm_handle)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::SyncCompositorDemandDrawSwParams)
  IPC_STRUCT_TRAITS_MEMBER(size)
  IPC_STRUCT_TRAITS_MEMBER(clip)
  IPC_STRUCT_TRAITS_MEMBER(transform)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::SyncCompositorCommonRendererParams)
  IPC_STRUCT_TRAITS_MEMBER(version)
  IPC_STRUCT_TRAITS_MEMBER(total_scroll_offset)
  IPC_STRUCT_TRAITS_MEMBER(max_scroll_offset)
  IPC_STRUCT_TRAITS_MEMBER(scrollable_size)
  IPC_STRUCT_TRAITS_MEMBER(page_scale_factor)
  IPC_STRUCT_TRAITS_MEMBER(min_page_scale_factor)
  IPC_STRUCT_TRAITS_MEMBER(max_page_scale_factor)
  IPC_STRUCT_TRAITS_MEMBER(need_animate_scroll)
  IPC_STRUCT_TRAITS_MEMBER(need_invalidate)
  IPC_STRUCT_TRAITS_MEMBER(need_begin_frame)
  IPC_STRUCT_TRAITS_MEMBER(did_activate_pending_tree)
IPC_STRUCT_TRAITS_END()

// Messages sent from the browser to the renderer.

IPC_SYNC_MESSAGE_ROUTED2_2(SyncCompositorMsg_HandleInputEvent,
                           content::SyncCompositorCommonBrowserParams,
                           IPC::WebInputEventPointer,
                           content::SyncCompositorCommonRendererParams,
                           content::InputEventAckState)

IPC_SYNC_MESSAGE_ROUTED2_1(SyncCompositorMsg_BeginFrame,
                           content::SyncCompositorCommonBrowserParams,
                           cc::BeginFrameArgs,
                           content::SyncCompositorCommonRendererParams)

IPC_SYNC_MESSAGE_ROUTED2_1(SyncCompositorMsg_ComputeScroll,
                           content::SyncCompositorCommonBrowserParams,
                           base::TimeTicks,
                           content::SyncCompositorCommonRendererParams)

IPC_SYNC_MESSAGE_ROUTED2_2(SyncCompositorMsg_DemandDrawHw,
                           content::SyncCompositorCommonBrowserParams,
                           content::SyncCompositorDemandDrawHwParams,
                           content::SyncCompositorCommonRendererParams,
                           cc::CompositorFrame)

IPC_SYNC_MESSAGE_ROUTED2_2(SyncCompositorMsg_SetSharedMemory,
                           content::SyncCompositorCommonBrowserParams,
                           content::SyncCompositorSetSharedMemoryParams,
                           bool /* success */,
                           content::SyncCompositorCommonRendererParams);

IPC_MESSAGE_ROUTED0(SyncCompositorMsg_ZeroSharedMemory);

IPC_SYNC_MESSAGE_ROUTED2_3(SyncCompositorMsg_DemandDrawSw,
                           content::SyncCompositorCommonBrowserParams,
                           content::SyncCompositorDemandDrawSwParams,
                           bool /* result */,
                           content::SyncCompositorCommonRendererParams,
                           cc::CompositorFrame)

IPC_MESSAGE_ROUTED1(SyncCompositorMsg_UpdateState,
                    content::SyncCompositorCommonBrowserParams)

// -----------------------------------------------------------------------------
// Messages sent from the renderer to the browser.

IPC_MESSAGE_ROUTED1(SyncCompositorHostMsg_UpdateState,
                    content::SyncCompositorCommonRendererParams)

IPC_MESSAGE_ROUTED2(SyncCompositorHostMsg_OverScroll,
                    content::SyncCompositorCommonRendererParams,
                    content::DidOverscrollParams)
