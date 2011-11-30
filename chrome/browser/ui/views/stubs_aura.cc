// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"

#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/importer/importer_progress_dialog.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_WIN)
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/first_run/first_run_import_observer.h"
#include "chrome/browser/ui/views/first_run_bubble.h"
#else
#include "chrome/browser/ui/certificate_dialogs.h"
#endif

#if defined(USE_NSS)
#include "chrome/browser/ui/crypto_module_password_dialog.h"
#endif

class SSLClientAuthHandler;
class TabContents;
class TabContentsWrapper;

namespace net {
class SSLCertRequestInfo;
class X509Certificate;
}
namespace views {
class Widget;
}

namespace browser {

#if defined(OS_WIN)
void ShowSSLClientCertificateSelector(
    TabContentsWrapper* parent,
    net::SSLCertRequestInfo* cert_request_info,
    SSLClientAuthHandler* delegate) {
  // TODO(beng):
  NOTIMPLEMENTED();
}
#endif

void ShowAboutIPCDialog() {
  // TODO(beng):
  NOTIMPLEMENTED();
}

}  // namespace browser

#if defined(OS_WIN)
void ShowCertificateViewer(gfx::NativeWindow parent,
                           net::X509Certificate* cert) {
  // No certificate viewer on Windows.
}
#endif // OS_WIN

namespace importer {

void ShowImportProgressDialog(gfx::NativeWindow parent_window,
                              uint16 items,
                              ImporterHost* importer_host,
                              ImporterObserver* importer_observer,
                              const SourceProfile& source_profile,
                              Profile* target_profile,
                              bool first_run) {
  // TODO(beng);
  NOTIMPLEMENTED();
}

}  // namespace importer

// static
void ExternalProtocolHandler::RunExternalProtocolDialog(
    const GURL& url, int render_process_host_id, int routing_id) {
}
