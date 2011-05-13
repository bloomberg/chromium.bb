// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_worker_message_filter.h"

#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "content/browser/resource_context.h"
#include "content/browser/worker_host/worker_process_host.h"
#include "content/common/worker_messages.h"

ChromeWorkerMessageFilter::ChromeWorkerMessageFilter(WorkerProcessHost* process)
    : process_(process) {
  ProfileIOData* io_data = reinterpret_cast<ProfileIOData*>(
      process->resource_context()->GetUserData(NULL));
  host_content_settings_map_ = io_data->GetHostContentSettingsMap();
}

ChromeWorkerMessageFilter::~ChromeWorkerMessageFilter() {
}

bool ChromeWorkerMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromeWorkerMessageFilter, message)
    IPC_MESSAGE_HANDLER(WorkerProcessHostMsg_AllowDatabase, OnAllowDatabase)
    IPC_MESSAGE_HANDLER(WorkerProcessHostMsg_AllowFileSystem, OnAllowFileSystem)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

bool ChromeWorkerMessageFilter::Send(IPC::Message* message) {
  return process_->Send(message);
}

void ChromeWorkerMessageFilter::OnAllowDatabase(int worker_route_id,
                                                const GURL& url,
                                                const string16& name,
                                                const string16& display_name,
                                                unsigned long estimated_size,
                                                bool* result) {
  ContentSetting content_setting =
      host_content_settings_map_->GetContentSetting(
          url, CONTENT_SETTINGS_TYPE_COOKIES, "");

  *result = content_setting != CONTENT_SETTING_BLOCK;

  // Find the worker instance and forward the message to all attached documents.
  WorkerProcessHost::Instances::const_iterator i;
  for (i = process_->instances().begin(); i != process_->instances().end();
       ++i) {
    if (i->worker_route_id() != worker_route_id)
      continue;
    const WorkerDocumentSet::DocumentInfoSet& documents =
        i->worker_document_set()->documents();
    for (WorkerDocumentSet::DocumentInfoSet::const_iterator doc =
         documents.begin(); doc != documents.end(); ++doc) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          NewRunnableFunction(
              &TabSpecificContentSettings::WebDatabaseAccessed,
              doc->render_process_id(), doc->render_view_id(),
              url, name, display_name, !*result));
    }
    break;
  }
}

void ChromeWorkerMessageFilter::OnAllowFileSystem(int worker_route_id,
                                                  const GURL& url,
                                                  bool* result) {
  // TODO(kinuko): Need to notify the UI thread to indicate that
  // there's a blocked content.  See the above for inspiration.
  ContentSetting content_setting =
      host_content_settings_map_->GetContentSetting(
          url, CONTENT_SETTINGS_TYPE_COOKIES, "");
  *result = content_setting != CONTENT_SETTING_BLOCK;
}
