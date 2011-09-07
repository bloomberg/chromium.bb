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
    TabContents* parent,
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

// static
void BookmarkEditor::Show(gfx::NativeWindow parent_hwnd,
                          Profile* profile,
                          const BookmarkNode* parent,
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

