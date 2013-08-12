// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/pepper/pepper_crx_file_system_message_filter.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pepper_permission_util.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_view_host.h"
#include "extensions/common/constants.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/dispatch_host_message.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "webkit/browser/fileapi/isolated_context.h"

namespace chrome {

namespace {

const char* kPredefinedAllowedCrxFsOrigins[] = {
  "6EAED1924DB611B6EEF2A664BD077BE7EAD33B8F",  // see crbug.com/234789
  "4EB74897CB187C7633357C2FE832E0AD6A44883A"   // see crbug.com/234789
};

}  // namespace

// static
PepperCrxFileSystemMessageFilter* PepperCrxFileSystemMessageFilter::Create(
    PP_Instance instance, content::BrowserPpapiHost* host) {
  int render_process_id;
  int unused_render_view_id;
  if (!host->GetRenderViewIDsForInstance(instance,
                                         &render_process_id,
                                         &unused_render_view_id)) {
    return NULL;
  }
  return new PepperCrxFileSystemMessageFilter(
      render_process_id,
      host->GetProfileDataDirectory(),
      host->GetDocumentURLForInstance(instance));
}

PepperCrxFileSystemMessageFilter::PepperCrxFileSystemMessageFilter(
    int render_process_id,
    const base::FilePath& profile_directory,
    const GURL& document_url)
    : render_process_id_(render_process_id),
      profile_directory_(profile_directory),
      document_url_(document_url) {
  for (size_t i = 0; i < arraysize(kPredefinedAllowedCrxFsOrigins); ++i)
    allowed_crxfs_origins_.insert(kPredefinedAllowedCrxFsOrigins[i]);
}

PepperCrxFileSystemMessageFilter::~PepperCrxFileSystemMessageFilter() {
}

scoped_refptr<base::TaskRunner>
PepperCrxFileSystemMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& msg) {
  // In order to reach ExtensionSystem, we need to get ProfileManager first.
  // ProfileManager lives in UI thread, so we need to do this in UI thread.
  return content::BrowserThread::GetMessageLoopProxyForThread(
      content::BrowserThread::UI);
}

int32_t PepperCrxFileSystemMessageFilter::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  IPC_BEGIN_MESSAGE_MAP(PepperCrxFileSystemMessageFilter, msg)
    PPAPI_DISPATCH_HOST_RESOURCE_CALL_0(
        PpapiHostMsg_Ext_CrxFileSystem_BrowserOpen, OnOpenFileSystem);
  IPC_END_MESSAGE_MAP()
  return PP_ERROR_FAILED;
}

Profile* PepperCrxFileSystemMessageFilter::GetProfile() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  return profile_manager->GetProfile(profile_directory_);
}

std::string PepperCrxFileSystemMessageFilter::CreateIsolatedFileSystem(
    Profile* profile) {
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(profile);
  if (!extension_system)
    return std::string();

  const ExtensionService* extension_service =
      extension_system->extension_service();
  if (!extension_service)
    return std::string();

  const extensions::Extension* extension =
      extension_service->GetExtensionById(document_url_.host(), false);
  if (!extension)
    return std::string();

  // First level directory for isolated filesystem to lookup.
  std::string kFirstLevelDirectory("crxfs");
  return fileapi::IsolatedContext::GetInstance()->
      RegisterFileSystemForPath(fileapi::kFileSystemTypeNativeLocal,
                                extension->path(),
                                &kFirstLevelDirectory);
}

int32_t PepperCrxFileSystemMessageFilter::OnOpenFileSystem(
    ppapi::host::HostMessageContext* context) {
  Profile* profile = GetProfile();
  const ExtensionSet* extension_set = NULL;
  if (profile) {
    extension_set = extensions::ExtensionSystem::Get(profile)->
        extension_service()->extensions();
  }
  if (!IsExtensionOrSharedModuleWhitelisted(
          document_url_, extension_set, allowed_crxfs_origins_) &&
      !IsHostAllowedByCommandLine(
          document_url_, extension_set, switches::kAllowNaClCrxFsAPI)) {
    LOG(ERROR) << "Host " << document_url_.host() << " cannot use CrxFs API.";
    return PP_ERROR_NOACCESS;
  }

  // TODO(raymes): When we remove FileSystem from the renderer, we should create
  // a pending PepperFileSystemBrowserHost here with the fsid and send the
  // pending host ID back to the plugin.
  const std::string fsid = CreateIsolatedFileSystem(profile);
  if (fsid.empty()) {
    context->reply_msg =
        PpapiPluginMsg_Ext_CrxFileSystem_BrowserOpenReply(std::string());
    return PP_ERROR_NOTSUPPORTED;
  }

  // Grant readonly access of isolated filesystem to renderer process.
  content::ChildProcessSecurityPolicy* policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  policy->GrantReadFileSystem(render_process_id_, fsid);

  context->reply_msg = PpapiPluginMsg_Ext_CrxFileSystem_BrowserOpenReply(fsid);
  return PP_OK;
}

}  // namespace chrome
