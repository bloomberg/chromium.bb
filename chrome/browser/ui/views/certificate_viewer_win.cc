// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/certificate_viewer.h"

#include <windows.h>
#include <cryptuiapi.h>
#pragma comment(lib, "cryptui.lib")

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/task_runner.h"
#include "base/threading/thread.h"
#include "chrome/browser/ui/host_desktop.h"
#include "net/cert/x509_certificate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/shell_dialogs/base_shell_dialog_win.h"

namespace {

// Shows a Windows certificate viewer dialog on a background thread to avoid
// nested message loops.
class CertificateViewerDialog : public ui::BaseShellDialogImpl {
 public:
  CertificateViewerDialog() {}

  // Shows the dialog and calls |callback| when the dialog closes. The caller
  // must ensure the CertificateViewerDialog remains valid until then.
  void Show(HWND parent,
            net::X509Certificate* cert,
            const base::Closure& callback) {
    if (IsRunningDialogForOwner(parent)) {
      base::MessageLoop::current()->PostTask(FROM_HERE, callback);
      return;
    }

    RunState run_state = BeginRun(parent);
    run_state.dialog_thread->task_runner()->PostTaskAndReply(
        FROM_HERE,
        base::Bind(&CertificateViewerDialog::ShowOnDialogThread,
                   base::Unretained(this), run_state, make_scoped_refptr(cert)),
        base::Bind(&CertificateViewerDialog::OnDialogClosed,
                   base::Unretained(this), run_state, callback));
  }

 private:
  void ShowOnDialogThread(const RunState& run_state,
                          const scoped_refptr<net::X509Certificate>& cert) {
    // Create a new cert context and store containing just the certificate
    // and its intermediate certificates.
    PCCERT_CONTEXT cert_list = cert->CreateOSCertChainForCert();
    CHECK(cert_list);

    CRYPTUI_VIEWCERTIFICATE_STRUCT view_info = {0};
    view_info.dwSize = sizeof(view_info);
    view_info.hwndParent = run_state.owner;
    view_info.dwFlags =
        CRYPTUI_DISABLE_EDITPROPERTIES | CRYPTUI_DISABLE_ADDTOSTORE;
    view_info.pCertContext = cert_list;
    HCERTSTORE cert_store = cert_list->hCertStore;
    view_info.cStores = 1;
    view_info.rghStores = &cert_store;

    BOOL properties_changed;
    ::CryptUIDlgViewCertificate(&view_info, &properties_changed);

    CertFreeCertificateContext(cert_list);
  }

  void OnDialogClosed(const RunState& run_state,
                      const base::Closure& callback) {
    EndRun(run_state);
    // May delete |this|.
    callback.Run();
  }

  DISALLOW_COPY_AND_ASSIGN(CertificateViewerDialog);
};

}  // namespace

void ShowCertificateViewer(content::WebContents* web_contents,
                           gfx::NativeWindow parent,
                           net::X509Certificate* cert) {
  if (chrome::GetHostDesktopTypeForNativeWindow(parent) !=
      chrome::HOST_DESKTOP_TYPE_ASH) {
    CertificateViewerDialog* dialog = new CertificateViewerDialog;
    dialog->Show(
        parent->GetHost()->GetAcceleratedWidget(), cert,
        base::Bind(&base::DeletePointer<CertificateViewerDialog>, dialog));
  } else {
    NOTIMPLEMENTED();
  }
}
