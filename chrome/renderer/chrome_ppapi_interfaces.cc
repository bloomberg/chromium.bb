// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_ppapi_interfaces.h"

#include "base/logging.h"
#include "base/rand_util_c.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/chrome_ppb_pdf_impl.h"
#include "content/renderer/render_thread.h"
#include "ppapi/c/private/ppb_nacl_private.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "webkit/plugins/ppapi/ppapi_interface_factory.h"

#if !defined(DISABLE_NACL)
#include "native_client/src/shared/imc/nacl_imc.h"
#include "ppapi/native_client/src/trusted/plugin/nacl_entry_points.h"
#endif

namespace chrome {

// Launch NaCl's sel_ldr process.
bool LaunchSelLdr(const char* alleged_url, int socket_count,
                  void* imc_handles, void* nacl_process_handle,
                  int* nacl_process_id) {
#if !defined(DISABLE_NACL)
  std::vector<nacl::FileDescriptor> sockets;
  base::ProcessHandle nacl_process;
  if (!RenderThread::current()->Send(
    new ChromeViewHostMsg_LaunchNaCl(
        ASCIIToWide(alleged_url),
        socket_count,
        &sockets,
        &nacl_process,
        reinterpret_cast<base::ProcessId*>(nacl_process_id)))) {
    return false;
  }
  CHECK(static_cast<int>(sockets.size()) == socket_count);
  for (int i = 0; i < socket_count; i++) {
    static_cast<nacl::Handle*>(imc_handles)[i] =
        nacl::ToNativeHandle(sockets[i]);
  }
  *static_cast<nacl::Handle*>(nacl_process_handle) = nacl_process;
  return true;
#else
  return false;
#endif
}

int UrandomFD(void) {
#if defined(OS_POSIX)
  return GetUrandomFD();
#else
  return 0;
#endif
}

const PPB_NaCl_Private ppb_nacl = {
  &LaunchSelLdr,
  &UrandomFD,
};

class PPB_NaCl_Impl {
 public:
  // Returns a pointer to the interface implementing PPB_NaCl_Private that is
  // exposed to the plugin.
  static const PPB_NaCl_Private* GetInterface() {
    return &ppb_nacl;
  }
};

const void* ChromePPAPIInterfaceFactory(const std::string& interface_name) {
  if (interface_name == PPB_NACL_PRIVATE_INTERFACE)
    return chrome::PPB_NaCl_Impl::GetInterface();
  if (interface_name == PPB_PDF_INTERFACE)
    return chrome::PPB_PDF_Impl::GetInterface();
  return NULL;
}

void InitializePPAPI() {
  webkit::ppapi::PpapiInterfaceFactoryManager* factory_manager =
      webkit::ppapi::PpapiInterfaceFactoryManager::GetInstance();
  factory_manager->RegisterFactory(ChromePPAPIInterfaceFactory);
}

void UninitializePPAPI() {
  webkit::ppapi::PpapiInterfaceFactoryManager* factory_manager =
      webkit::ppapi::PpapiInterfaceFactoryManager::GetInstance();
  factory_manager->UnregisterFactory(ChromePPAPIInterfaceFactory);
}

}  // namespace chrome

