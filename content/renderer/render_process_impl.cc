// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include <objidl.h>
#include <mlang.h>
#endif

#include "content/renderer/render_process_impl.h"

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/sys_info.h"
#include "base/utf_string_conversions.h"
#include "content/common/child_thread.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/content_renderer_client.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_message_utils.h"
#include "skia/ext/platform_canvas.h"
#include "ui/gfx/surface/transport_dib.h"
#include "webkit/plugins/npapi/plugin_instance.h"
#include "webkit/plugins/npapi/plugin_lib.h"
#include "webkit/glue/webkit_glue.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

RenderProcessImpl::RenderProcessImpl()
    : ALLOW_THIS_IN_INITIALIZER_LIST(shared_mem_cache_cleaner_(
          FROM_HERE, base::TimeDelta::FromSeconds(5),
          this, &RenderProcessImpl::ClearTransportDIBCache)),
      transport_dib_next_sequence_number_(0) {
  in_process_plugins_ = InProcessPlugins();
  for (size_t i = 0; i < arraysize(shared_mem_cache_); ++i)
    shared_mem_cache_[i] = NULL;

#if defined(OS_WIN)
  // HACK:  See http://b/issue?id=1024307 for rationale.
  if (GetModuleHandle(L"LPK.DLL") == NULL) {
    // Makes sure lpk.dll is loaded by gdi32 to make sure ExtTextOut() works
    // when buffering into a EMF buffer for printing.
    typedef BOOL (__stdcall *GdiInitializeLanguagePack)(int LoadedShapingDLLs);
    GdiInitializeLanguagePack gdi_init_lpk =
        reinterpret_cast<GdiInitializeLanguagePack>(GetProcAddress(
            GetModuleHandle(L"GDI32.DLL"),
            "GdiInitializeLanguagePack"));
    DCHECK(gdi_init_lpk);
    if (gdi_init_lpk) {
      gdi_init_lpk(0);
    }
  }
#endif

  // Out of process dev tools rely upon auto break behavior.
  webkit_glue::SetJavaScriptFlags(
      "--debugger-auto-break"
      // Enable on-demand profiling.
      " --prof --prof-lazy");

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kJavaScriptFlags)) {
    webkit_glue::SetJavaScriptFlags(
        command_line.GetSwitchValueASCII(switches::kJavaScriptFlags));
  }

  if (command_line.HasSwitch(switches::kDartFlags)) {
    webkit_glue::SetDartFlags(
        command_line.GetSwitchValueASCII(switches::kDartFlags));
  }
}

RenderProcessImpl::~RenderProcessImpl() {
  // TODO(port): Try and limit what we pull in for our non-Win unit test bundle.
#ifndef NDEBUG
  // log important leaked objects
  webkit_glue::CheckForLeaks();
#endif

  GetShutDownEvent()->Signal();
  ClearTransportDIBCache();
}

bool RenderProcessImpl::InProcessPlugins() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
#if defined(OS_LINUX) || defined(OS_OPENBSD)
  // Plugin processes require a UI message loop, and the Linux message loop
  // implementation only allows one UI loop per process.
  if (command_line.HasSwitch(switches::kInProcessPlugins))
    NOTIMPLEMENTED() << ": in process plugins not supported on Linux";
  return command_line.HasSwitch(switches::kInProcessPlugins);
#else
  return command_line.HasSwitch(switches::kInProcessPlugins) ||
         command_line.HasSwitch(switches::kSingleProcess);
#endif
}

// -----------------------------------------------------------------------------
// Platform specific code for dealing with bitmap transport...

TransportDIB* RenderProcessImpl::CreateTransportDIB(size_t size) {
#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_OPENBSD)
  // Windows and Linux create transport DIBs inside the renderer
  return TransportDIB::Create(size, transport_dib_next_sequence_number_++);
#elif defined(OS_MACOSX)
  // Mac creates transport DIBs in the browser, so we need to do a sync IPC to
  // get one.  The TransportDIB is cached in the browser.
  TransportDIB::Handle handle;
  IPC::Message* msg = new ViewHostMsg_AllocTransportDIB(size, true, &handle);
  if (!main_thread()->Send(msg))
    return NULL;
  if (handle.fd < 0)
    return NULL;
  return TransportDIB::Map(handle);
#endif  // defined(OS_MACOSX)
}

void RenderProcessImpl::FreeTransportDIB(TransportDIB* dib) {
  if (!dib)
    return;

#if defined(OS_MACOSX)
  // On Mac we need to tell the browser that it can drop a reference to the
  // shared memory.
  IPC::Message* msg = new ViewHostMsg_FreeTransportDIB(dib->id());
  main_thread()->Send(msg);
#endif

  delete dib;
}

// -----------------------------------------------------------------------------


skia::PlatformCanvas* RenderProcessImpl::GetDrawingCanvas(
    TransportDIB** memory, const gfx::Rect& rect) {
  int width = rect.width();
  int height = rect.height();
  const size_t stride = skia::PlatformCanvas::StrideForWidth(rect.width());
#if defined(OS_LINUX) || defined(OS_OPENBSD)
  const size_t max_size = base::SysInfo::MaxSharedMemorySize();
#else
  const size_t max_size = 0;
#endif

  // If the requested size is too big, reduce the height. Ideally we might like
  // to reduce the width as well to make the size reduction more "balanced", but
  // it rarely comes up in practice.
  if ((max_size != 0) && (height * stride > max_size))
    height = max_size / stride;

  const size_t size = height * stride;

  if (!GetTransportDIBFromCache(memory, size)) {
    *memory = CreateTransportDIB(size);
    if (!*memory)
      return NULL;
  }

  return (*memory)->GetPlatformCanvas(width, height);
}

void RenderProcessImpl::ReleaseTransportDIB(TransportDIB* mem) {
  if (PutSharedMemInCache(mem)) {
    shared_mem_cache_cleaner_.Reset();
    return;
  }

  FreeTransportDIB(mem);
}

bool RenderProcessImpl::UseInProcessPlugins() const {
  return in_process_plugins_;
}

bool RenderProcessImpl::GetTransportDIBFromCache(TransportDIB** mem,
                                             size_t size) {
  // look for a cached object that is suitable for the requested size.
  for (size_t i = 0; i < arraysize(shared_mem_cache_); ++i) {
    if (shared_mem_cache_[i] &&
        size <= shared_mem_cache_[i]->size()) {
      *mem = shared_mem_cache_[i];
      shared_mem_cache_[i] = NULL;
      return true;
    }
  }

  return false;
}

int RenderProcessImpl::FindFreeCacheSlot(size_t size) {
  // simple algorithm:
  //  - look for an empty slot to store mem, or
  //  - if full, then replace smallest entry which is smaller than |size|
  for (size_t i = 0; i < arraysize(shared_mem_cache_); ++i) {
    if (shared_mem_cache_[i] == NULL)
      return i;
  }

  size_t smallest_size = size;
  int smallest_index = -1;

  for (size_t i = 1; i < arraysize(shared_mem_cache_); ++i) {
    const size_t entry_size = shared_mem_cache_[i]->size();
    if (entry_size < smallest_size) {
      smallest_size = entry_size;
      smallest_index = i;
    }
  }

  if (smallest_index != -1) {
    FreeTransportDIB(shared_mem_cache_[smallest_index]);
    shared_mem_cache_[smallest_index] = NULL;
  }

  return smallest_index;
}

bool RenderProcessImpl::PutSharedMemInCache(TransportDIB* mem) {
  const int slot = FindFreeCacheSlot(mem->size());
  if (slot == -1)
    return false;

  shared_mem_cache_[slot] = mem;
  return true;
}

void RenderProcessImpl::ClearTransportDIBCache() {
  for (size_t i = 0; i < arraysize(shared_mem_cache_); ++i) {
    if (shared_mem_cache_[i]) {
      FreeTransportDIB(shared_mem_cache_[i]);
      shared_mem_cache_[i] = NULL;
    }
  }
}
