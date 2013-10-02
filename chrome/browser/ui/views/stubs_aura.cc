// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/logging.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "ui/gfx/native_widget_types.h"

#if !defined(OS_WIN)
#include "chrome/browser/ui/certificate_dialogs.h"
#endif

#if defined(USE_NSS)
#include "chrome/browser/ui/crypto_module_password_dialog.h"
#endif

class SSLClientAuthHandler;

namespace content {
class WebContents;
}

namespace net {
class HttpNetworkSession;
class SSLCertRequestInfo;
class X509Certificate;
}

namespace views {
class Widget;
}

namespace chrome {

void ShowAboutIPCDialog() {
  // TODO(beng):
  NOTIMPLEMENTED();
}

}  // namespace chrome

#if !defined(OS_CHROMEOS) && !defined(OS_WIN)
// static
void ExternalProtocolHandler::RunExternalProtocolDialog(
    const GURL& url, int render_process_host_id, int routing_id) {
}
#endif
