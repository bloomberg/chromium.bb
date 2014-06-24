// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_RENDERER_DATA_FILE_RENDERER_CLD_DATA_PROVIDER_H_
#define COMPONENTS_TRANSLATE_CONTENT_RENDERER_DATA_FILE_RENDERER_CLD_DATA_PROVIDER_H_

#include "base/files/file.h"
#include "base/macros.h"
#include "components/translate/content/renderer/renderer_cld_data_provider.h"
#include "ipc/ipc_platform_file.h"

namespace translate {

class DataFileRendererCldDataProvider : public RendererCldDataProvider {
 public:
  explicit DataFileRendererCldDataProvider(content::RenderViewObserver*);
  virtual ~DataFileRendererCldDataProvider();
  // RendererCldDataProvider implementations:
  virtual bool OnMessageReceived(const IPC::Message&) OVERRIDE;
  virtual void SendCldDataRequest() OVERRIDE;
  virtual void SetCldAvailableCallback(base::Callback<void(void)>) OVERRIDE;
  virtual bool IsCldDataAvailable() OVERRIDE;

 private:
  void OnCldDataAvailable(const IPC::PlatformFileForTransit ipc_file_handle,
                          const uint64 data_offset,
                          const uint64 data_length);
  void LoadCldData(base::File file,
                   const uint64 data_offset,
                   const uint64 data_length);
  content::RenderViewObserver* render_view_observer_;
  base::Callback<void(void)> cld_available_callback_;

  DISALLOW_COPY_AND_ASSIGN(DataFileRendererCldDataProvider);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CONTENT_RENDERER_DATA_FILE_RENDERER_CLD_DATA_PROVIDER_H_
