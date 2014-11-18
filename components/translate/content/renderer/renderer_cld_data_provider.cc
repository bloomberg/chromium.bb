// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/renderer/renderer_cld_data_provider.h"

namespace translate {

RendererCldDataProvider::RendererCldDataProvider() {
  // Nothing
}

bool RendererCldDataProvider::IsCldDataAvailable() {
  return true;
}

bool RendererCldDataProvider::OnMessageReceived(const IPC::Message& message) {
  return false;  // Message not handled
}

void RendererCldDataProvider::SetCldAvailableCallback
(base::Callback<void(void)> callback) {
  callback.Run();
}

}  // namespace translate
