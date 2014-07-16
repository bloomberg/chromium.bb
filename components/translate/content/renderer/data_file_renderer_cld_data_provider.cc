// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "data_file_renderer_cld_data_provider.h"

#include "base/basictypes.h"
#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "components/translate/content/common/data_file_cld_data_provider_messages.h"
#include "content/public/renderer/render_view_observer.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"
#include "third_party/cld_2/src/public/compact_lang_det.h"

namespace {

// A struct that contains the pointer to the CLD mmap. Used so that we can
// leverage LazyInstance:Leaky to properly scope the lifetime of the mmap.
struct CLDMmapWrapper {
  CLDMmapWrapper() { value = NULL; }
  base::MemoryMappedFile* value;
};
base::LazyInstance<CLDMmapWrapper>::Leaky g_cld_mmap =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace translate {

// Implementation of the static factory method from RendererCldDataProvider,
// hooking up this specific implementation for all of Chromium.
RendererCldDataProvider* CreateRendererCldDataProviderFor(
    content::RenderViewObserver* render_view_observer) {
  // This log line is to help with determining which kind of provider has been
  // configured. See also: chrome://translate-internals
  VLOG(1) << "Creating DataFileRendererCldDataProvider";
  return new DataFileRendererCldDataProvider(render_view_observer);
}

DataFileRendererCldDataProvider::DataFileRendererCldDataProvider(
    content::RenderViewObserver* render_view_observer)
    : render_view_observer_(render_view_observer) {
}

DataFileRendererCldDataProvider::~DataFileRendererCldDataProvider() {
}

bool DataFileRendererCldDataProvider::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DataFileRendererCldDataProvider, message)
  IPC_MESSAGE_HANDLER(ChromeViewMsg_CldDataFileAvailable, OnCldDataAvailable)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DataFileRendererCldDataProvider::SendCldDataRequest() {
  // Else, send the IPC message to the browser process requesting the data...
  render_view_observer_->Send(new ChromeViewHostMsg_NeedCldDataFile(
      render_view_observer_->routing_id()));
}

bool DataFileRendererCldDataProvider::IsCldDataAvailable() {
  // This neatly removes the need for code that depends on the generalized
  // RendererCldDataProvider to #ifdef on CLD2_DYNAMIC_MODE
  return CLD2::isDataLoaded();  // ground truth, independent of our state.
}

void DataFileRendererCldDataProvider::SetCldAvailableCallback(
    base::Callback<void(void)> callback) {
  cld_available_callback_ = callback;
}

void DataFileRendererCldDataProvider::OnCldDataAvailable(
    const IPC::PlatformFileForTransit ipc_file_handle,
    const uint64 data_offset,
    const uint64 data_length) {
  LoadCldData(IPC::PlatformFileForTransitToFile(ipc_file_handle),
              data_offset,
              data_length);
}

void DataFileRendererCldDataProvider::LoadCldData(base::File file,
                                                  const uint64 data_offset,
                                                  const uint64 data_length) {
  // Terminate immediately if data is already loaded.
  if (IsCldDataAvailable())
    return;

  if (!file.IsValid()) {
    LOG(ERROR) << "Can't find the CLD data file.";
    return;
  }

  // mmap the file
  g_cld_mmap.Get().value = new base::MemoryMappedFile();
  bool initialized = g_cld_mmap.Get().value->Initialize(file.Pass());
  if (!initialized) {
    LOG(ERROR) << "mmap initialization failed";
    delete g_cld_mmap.Get().value;
    g_cld_mmap.Get().value = NULL;
    return;
  }

  // Sanity checks
  uint64 max_int32 = std::numeric_limits<int32>::max();
  if (data_length + data_offset > g_cld_mmap.Get().value->length() ||
      data_length > max_int32) {  // max signed 32 bit integer
    LOG(ERROR) << "Illegal mmap config: data_offset=" << data_offset
               << ", data_length=" << data_length
               << ", mmap->length()=" << g_cld_mmap.Get().value->length();
    delete g_cld_mmap.Get().value;
    g_cld_mmap.Get().value = NULL;
    return;
  }

  // Initialize the CLD subsystem... and it's all done!
  const uint8* data_ptr = g_cld_mmap.Get().value->data() + data_offset;
  CLD2::loadDataFromRawAddress(data_ptr, data_length);
  DCHECK(CLD2::isDataLoaded()) << "Failed to load CLD data from mmap";
  if (!cld_available_callback_.is_null()) {
    cld_available_callback_.Run();
  }
}

}  // namespace translate
