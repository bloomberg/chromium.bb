// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/browser_ppapi_host_test.h"

#include "base/process_util.h"
#include "content/browser/renderer_host/pepper/browser_ppapi_host_impl.h"

namespace content {

BrowserPpapiHostTest::BrowserPpapiHostTest()
    : sink_(),
      ppapi_host_(new BrowserPpapiHostImpl(
            &sink_,
            ppapi::PpapiPermissions::AllPermissions())) {
  ppapi_host_->set_plugin_process_handle(base::GetCurrentProcessHandle());
}

BrowserPpapiHostTest::~BrowserPpapiHostTest() {
}

BrowserPpapiHost* BrowserPpapiHostTest::GetPpapiHost() {
  return ppapi_host_;
}

}  // namespace content
