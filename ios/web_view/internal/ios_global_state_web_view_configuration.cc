// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/global_state/ios_global_state_configuration.h"

#include "ios/web/public/web_thread.h"
#include "ios/web_view/internal/web_view_global_state_util.h"

namespace ios_global_state {

scoped_refptr<base::SingleThreadTaskRunner>
GetSharedNetworkIOThreadTaskRunner() {
  static dispatch_once_t once_token;
  dispatch_once(&once_token, ^{
    ios_web_view::InitializeGlobalState();
  });
  return web::WebThread::GetTaskRunnerForThread(web::WebThread::IO);
}

}  // namespace ios_global_state
