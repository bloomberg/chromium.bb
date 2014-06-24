// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_RENDERER_STATIC_RENDERER_CLD_DATA_PROVIDER_H_
#define COMPONENTS_TRANSLATE_CONTENT_RENDERER_STATIC_RENDERER_CLD_DATA_PROVIDER_H_

#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/translate/content/renderer/renderer_cld_data_provider.h"
#include "ipc/ipc_platform_file.h"

namespace translate {

class StaticRendererCldDataProvider : public RendererCldDataProvider {
 public:
  explicit StaticRendererCldDataProvider();
  virtual ~StaticRendererCldDataProvider();
  // RendererCldDataProvider implementations:
  virtual bool OnMessageReceived(const IPC::Message&) OVERRIDE;
  virtual void SendCldDataRequest() OVERRIDE;
  virtual void SetCldAvailableCallback(base::Callback<void(void)>) OVERRIDE;
  virtual bool IsCldDataAvailable() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(StaticRendererCldDataProvider);
};

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_CONTENT_RENDERER_STATIC_RENDERER_CLD_DATA_PROVIDER_H_
