// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper/ppb_nacl_private_impl.h"

#ifndef DISABLE_NACL

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/rand_util_c.h"
#include "chrome/common/render_messages.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_thread.h"
#include "ipc/ipc_sync_message_filter.h"
#include "ppapi/c/private/ppb_nacl_private.h"
#include "ppapi/native_client/src/trusted/plugin/nacl_entry_points.h"

#if defined(OS_WIN)
#include "content/public/common/sandbox_init.h"
#endif

namespace {

base::LazyInstance<scoped_refptr<IPC::SyncMessageFilter> >
    g_background_thread_sender = LAZY_INSTANCE_INITIALIZER;

// Launch NaCl's sel_ldr process.
PP_Bool LaunchSelLdr(PP_Instance instance,
                     const char* alleged_url, int socket_count,
                     void* imc_handles) {
  std::vector<nacl::FileDescriptor> sockets;
  IPC::Message::Sender* sender = content::RenderThread::Get();
  if (sender == NULL)
    sender = g_background_thread_sender.Pointer()->get();

  if (!sender->Send(new ChromeViewHostMsg_LaunchNaCl(
          GURL(alleged_url), socket_count, &sockets)))
    return PP_FALSE;

  CHECK(static_cast<int>(sockets.size()) == socket_count);
  for (int i = 0; i < socket_count; i++) {
    static_cast<nacl::Handle*>(imc_handles)[i] =
        nacl::ToNativeHandle(sockets[i]);
  }

  return PP_TRUE;
}

PP_Bool StartPpapiProxy(PP_Instance instance) {
  return PP_FALSE;
}

int UrandomFD(void) {
#if defined(OS_POSIX)
  return GetUrandomFD();
#else
  return 0;
#endif
}

bool Are3DInterfacesDisabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisable3DAPIs);
}

void EnableBackgroundSelLdrLaunch() {
  g_background_thread_sender.Get() =
      content::RenderThread::Get()->GetSyncMessageFilter();
}

int BrokerDuplicateHandle(void* source_handle,
                          unsigned int process_id,
                          void** target_handle,
                          unsigned int desired_access,
                          unsigned int options) {
#if defined(OS_WIN)
  return content::BrokerDuplicateHandle(source_handle, process_id,
                                        target_handle, desired_access,
                                        options);
#else
  return 0;
#endif
}

const PPB_NaCl_Private nacl_interface = {
  &LaunchSelLdr,
  &StartPpapiProxy,
  &UrandomFD,
  &Are3DInterfacesDisabled,
  &EnableBackgroundSelLdrLaunch,
  &BrokerDuplicateHandle,
};

}  // namespace

const PPB_NaCl_Private* PPB_NaCl_Private_Impl::GetInterface() {
  return &nacl_interface;
}

#endif  // DISABLE_NACL
