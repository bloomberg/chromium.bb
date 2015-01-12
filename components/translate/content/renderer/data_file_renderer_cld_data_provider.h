// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_RENDERER_DATA_FILE_RENDERER_CLD_DATA_PROVIDER_H_
#define COMPONENTS_TRANSLATE_CONTENT_RENDERER_DATA_FILE_RENDERER_CLD_DATA_PROVIDER_H_

#include "base/files/file.h"
#include "base/macros.h"
#include "components/translate/content/renderer/renderer_cld_data_provider.h"
#include "ipc/ipc_platform_file.h"

namespace content {
class RenderViewObserver;
}

namespace translate {

class DataFileRendererCldDataProvider : public RendererCldDataProvider {
 public:
  explicit DataFileRendererCldDataProvider(content::RenderViewObserver*);
  ~DataFileRendererCldDataProvider() override;

  // Load the CLD data from the specified file, starting at the specified byte
  // offset and having the specified length (also in bytes). Nominally, the
  // implementation will mmap the file in read-only mode and hand the data off
  // to CLD2::loadDataFromRawAddress(...). See the module
  // third_party/cld_2/src/internal/compact_lang_det.h for more information on
  // the dynamic data loading process.
  void LoadCldData(base::File file,
                   const uint64 data_offset,
                   const uint64 data_length);

  // RendererCldDataProvider implementations:
  bool OnMessageReceived(const IPC::Message&) override;
  void SendCldDataRequest() override;
  void SetCldAvailableCallback(base::Callback<void(void)>) override;
  bool IsCldDataAvailable() override;

 private:
  void OnCldDataAvailable(const IPC::PlatformFileForTransit ipc_file_handle,
                          const uint64 data_offset,
                          const uint64 data_length);
  content::RenderViewObserver* render_view_observer_;
  base::Callback<void(void)> cld_available_callback_;

  DISALLOW_COPY_AND_ASSIGN(DataFileRendererCldDataProvider);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CONTENT_RENDERER_DATA_FILE_RENDERER_CLD_DATA_PROVIDER_H_
