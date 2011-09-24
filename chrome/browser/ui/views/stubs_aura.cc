// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"

#include "chrome/browser/bookmarks/bookmark_editor.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/first_run/first_run_import_observer.h"
#include "chrome/browser/importer/importer_progress_dialog.h"
#include "chrome/browser/ui/views/first_run_bubble.h"
#include "ui/gfx/native_widget_types.h"

#if !defined(OS_WIN)
#include "chrome/browser/ui/gtk/certificate_dialogs.h"
#endif

#if defined(USE_NSS)
#include "chrome/browser/ui/crypto_module_password_dialog.h"
#endif

class SSLClientAuthHandler;
class TabContents;
class TabContentsWrapper;
namespace crypto {
class CryptoModuleBlockingPasswordDelegate;
}
namespace net {
class SSLCertRequestInfo;
class X509Certificate;
}
namespace views {
class Widget;
}

#if !defined(OS_WIN)
class EditSearchEngineControllerDelegate;
class TemplateURL;
#endif

namespace browser {

// Declared in browser_dialogs.h so others don't need to depend on our header.
void ShowTaskManager() {
  // TODO(beng):
  NOTIMPLEMENTED();
}

void ShowBackgroundPages() {
  // TODO(beng):
  NOTIMPLEMENTED();
}

void ShowCollectedCookiesDialog(gfx::NativeWindow parent_window,
                                TabContentsWrapper* tab_contents) {
  // TODO(beng):
  NOTIMPLEMENTED();
}

void ShowSSLClientCertificateSelector(
    TabContentsWrapper* parent,
    net::SSLCertRequestInfo* cert_request_info,
    SSLClientAuthHandler* delegate) {
  // TODO(beng):
  NOTIMPLEMENTED();
}

void ShowAboutIPCDialog() {
  // TODO(beng):
  NOTIMPLEMENTED();
}

gfx::Rect GrabWindowSnapshot(gfx::NativeWindow window,
                             std::vector<unsigned char>* png_representation) {
  // TODO(beng):
  NOTIMPLEMENTED();
  return gfx::Rect();
}

void ShowHungRendererDialog(TabContents* contents) {
  // TODO(beng):
  NOTIMPLEMENTED();
}

void HideHungRendererDialog(TabContents* contents) {
  // TODO(beng):
  NOTIMPLEMENTED();
}

#if defined(USE_NSS)
crypto::CryptoModuleBlockingPasswordDelegate*
    NewCryptoModuleBlockingDialogDelegate(
        CryptoModulePasswordReason reason,
        const std::string& server) {
  // TODO(saintlou):
  NOTIMPLEMENTED();
  return NULL;
}
#endif

#if !defined(OS_WIN)
void EditSearchEngine(
    gfx::NativeWindow,
    const TemplateURL*,
    EditSearchEngineControllerDelegate*,
    Profile*) {
  // TODO(saintlou):
  NOTIMPLEMENTED();
}

void ShowRepostFormWarningDialog(gfx::NativeWindow parent_window,
                                 TabContents* tab_contents) {
  // TODO(saintlou):
  NOTIMPLEMENTED();
}

void ShowCryptoModulePasswordDialog(const std::string& module_name,
                            bool retry,
                            CryptoModulePasswordReason reason,
                            const std::string& server,
                            CryptoModulePasswordCallback* callback) {
  // TODO(saintlou):
  NOTIMPLEMENTED();
}
#endif

}  // namespace browser


void ShowCertificateViewer(gfx::NativeWindow parent,
                           net::X509Certificate* cert) {
  // TODO(beng);
  NOTIMPLEMENTED();
}

// static
FirstRunBubble* FirstRunBubble::Show(
    Profile* profile,
    views::Widget* parent,
    const gfx::Rect& position_relative_to,
    views::BubbleBorder::ArrowLocation arrow_location,
    FirstRun::BubbleType bubble_type) {
  // TODO(beng);
  NOTIMPLEMENTED();
  return NULL;
}

#if !defined(OS_WIN)
void ShowCertSelectFileDialog(SelectFileDialog* select_file_dialog,
                              SelectFileDialog::Type type,
                              const FilePath& suggested_path,
                              TabContents* tab_contents,
                              gfx::NativeWindow parent,
                              void* params) {
  // TODO(saintlou);
  NOTIMPLEMENTED();
}

void ShowCertExportDialog(TabContents* tab_contents,
                          gfx::NativeWindow parent,
                          net::X509Certificate::OSCertHandle cert) {
  // TODO(saintlou);
  NOTIMPLEMENTED();
}
#endif

// static
void BookmarkEditor::Show(gfx::NativeWindow parent_window,
                          Profile* profile,
                          const EditDetails& details,
                          Configuration configuration) {
  // TODO(beng);
  NOTIMPLEMENTED();
}

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
