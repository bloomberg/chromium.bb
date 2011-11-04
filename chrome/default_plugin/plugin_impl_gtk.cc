// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/default_plugin/plugin_impl_gtk.h"

#include <X11/Xdefs.h>
#include <gtk/gtk.h>

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/common/chrome_plugin_messages.h"
#include "chrome/default_plugin/plugin_main.h"
#include "content/common/child_thread.h"
#include "content/public/common/content_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/webkit_strings.h"
#include "unicode/locid.h"
#include "webkit/plugins/npapi/default_plugin_shared.h"

// TODO(thakis): Most methods in this class are stubbed out an need to be
// implemented.

PluginInstallerImpl::PluginInstallerImpl(int16 mode)
  : instance_(NULL),
    plugin_install_stream_(NULL),
    plugin_installer_state_(PluginInstallerStateUndefined),
    container_(NULL) {
}

PluginInstallerImpl::~PluginInstallerImpl() {
  if (container_)
    gtk_widget_destroy(container_);
}

bool PluginInstallerImpl::Initialize(void* module_handle, NPP instance,
                                     NPMIMEType mime_type, int16 argc,
                                     char* argn[], char* argv[]) {
  DVLOG(1) << __FUNCTION__ << " MIME Type : " << mime_type;
  DCHECK(instance != NULL);

  if (mime_type == NULL || strlen(mime_type) == 0) {
    DLOG(WARNING) << __FUNCTION__ << " Invalid parameters passed in";
    NOTREACHED();
    return false;
  }

  instance_ = instance;
  mime_type_ = mime_type;

  PluginInstallerBase::SetRoutingIds(argc, argn, argv);
  return true;
}

bool PluginInstallerImpl::NPP_SetWindow(NPWindow* window_info) {
  if (container_)
    gtk_widget_destroy(container_);
  container_ = gtk_plug_new(reinterpret_cast<XID>(window_info->window));

  // Add label.
  GtkWidget* box = gtk_vbox_new(FALSE, 0);
  GtkWidget* label = gtk_label_new("Missing Plug-in");
  gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);
  gtk_container_add(GTK_CONTAINER(container_), box);

  gtk_widget_show_all(container_);

  return true;
}

void PluginInstallerImpl::Shutdown() {
}

void PluginInstallerImpl::NewStream(NPStream* stream) {
  plugin_install_stream_ = stream;
}

void PluginInstallerImpl::DestroyStream(NPStream* stream, NPError reason) {
  if (stream == plugin_install_stream_)
    plugin_install_stream_ = NULL;
}

bool PluginInstallerImpl::WriteReady(NPStream* stream) {
  bool ready_to_accept_data = false;
  return ready_to_accept_data;
}

int32 PluginInstallerImpl::Write(NPStream* stream, int32 offset,
                                 int32 buffer_length, void* buffer) {
  return true;
}

void PluginInstallerImpl::ClearDisplay() {
}

void PluginInstallerImpl::RefreshDisplay() {
}

bool PluginInstallerImpl::CreateToolTip() {
  return true;
}

void PluginInstallerImpl::UpdateToolTip() {
}

void PluginInstallerImpl::DisplayAvailablePluginStatus() {
}

void PluginInstallerImpl::DisplayStatus(int message_resource_id) {
}

void PluginInstallerImpl::DisplayPluginDownloadFailedStatus() {
}

void PluginInstallerImpl::URLNotify(const char* url, NPReason reason) {
}

int16 PluginInstallerImpl::NPP_HandleEvent(void* event) {
  return 0;
}

std::wstring PluginInstallerImpl::ReplaceStringForPossibleEmptyReplacement(
    int message_id_with_placeholders,
    int messsage_id_without_placeholders,
    const std::wstring& replacement_string) {
  return L"";
}

void PluginInstallerImpl::DownloadPlugin() {
}

void PluginInstallerImpl::DownloadCancelled() {
  DisplayAvailablePluginStatus();
}

void PluginInstallerImpl::ShowInstallDialog() {
}

void PluginInstallerImpl::NotifyPluginStatus(int status) {
  ChildThread::current()->Send(
      new ChromePluginProcessHostMsg_MissingPluginStatus(
          status,
          renderer_process_id(),
          render_view_id(),
          0));
}
