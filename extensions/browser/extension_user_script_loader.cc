// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_user_script_loader.h"

#include <set>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/version.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/component_extension_resource_manager.h"
#include "extensions/browser/content_verifier.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_handlers/default_locale_handler.h"
#include "extensions/common/message_bundle.h"
#include "extensions/common/one_shot_event.h"
#include "ui/base/resource/resource_bundle.h"

using content::BrowserContext;

namespace extensions {

namespace {

// Verifies file contents as they are read.
void VerifyContent(const scoped_refptr<ContentVerifier>& verifier,
                   const std::string& extension_id,
                   const base::FilePath& extension_root,
                   const base::FilePath& relative_path,
                   const std::string& content) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  scoped_refptr<ContentVerifyJob> job(
      verifier->CreateJobFor(extension_id, extension_root, relative_path));
  if (job.get()) {
    job->Start();
    job->BytesRead(content.size(), content.data());
    job->DoneReading();
  }
}

// Loads user scripts from the extension who owns these scripts.
bool ExtensionLoadScriptContent(
    const HostID& host_id,
    UserScript::File* script_file,
    const UserScriptLoader::SubstitutionMap* localization_messages,
    const scoped_refptr<ContentVerifier>& verifier) {
  DCHECK(script_file);
  std::string content;
  const base::FilePath& path = ExtensionResource::GetFilePath(
      script_file->extension_root(), script_file->relative_path(),
      ExtensionResource::SYMLINKS_MUST_RESOLVE_WITHIN_ROOT);
  if (path.empty()) {
    int resource_id = 0;
    if (ExtensionsBrowserClient::Get()
            ->GetComponentExtensionResourceManager()
            ->IsComponentExtensionResource(script_file->extension_root(),
                                           script_file->relative_path(),
                                           &resource_id)) {
      const ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      content = rb.GetRawDataResource(resource_id).as_string();
    } else {
      LOG(WARNING) << "Failed to get file path to "
                   << script_file->relative_path().value() << " from "
                   << script_file->extension_root().value();
      return false;
    }
  } else {
    if (!base::ReadFileToString(path, &content)) {
      LOG(WARNING) << "Failed to load user script file: " << path.value();
      return false;
    }
    if (verifier.get()) {
      content::BrowserThread::PostTask(
          content::BrowserThread::IO, FROM_HERE,
          base::Bind(&VerifyContent, verifier, host_id.id(),
                     script_file->extension_root(),
                     script_file->relative_path(), content));
    }
  }

  // Localize the content.
  if (localization_messages) {
    std::string error;
    MessageBundle::ReplaceMessagesWithExternalDictionary(*localization_messages,
                                                         &content, &error);
    if (!error.empty())
      LOG(WARNING) << "Failed to replace messages in script: " << error;
  }

  // Remove BOM from the content.
  std::string::size_type index = content.find(base::kUtf8ByteOrderMark);
  if (index == 0)
    script_file->set_content(content.substr(strlen(base::kUtf8ByteOrderMark)));
  else
    script_file->set_content(content);

  return true;
}

}  // namespace

ExtensionUserScriptLoader::ExtensionUserScriptLoader(
    BrowserContext* browser_context,
    const HostID& host_id,
    bool listen_for_extension_system_loaded)
    : UserScriptLoader(
          browser_context,
          host_id,
          ExtensionSystem::Get(browser_context)->content_verifier()),
      extension_registry_observer_(this),
      weak_factory_(this) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context));
  if (listen_for_extension_system_loaded) {
    ExtensionSystem::Get(browser_context)
        ->ready()
        .Post(FROM_HERE,
              base::Bind(&ExtensionUserScriptLoader::OnExtensionSystemReady,
                         weak_factory_.GetWeakPtr()));
  } else {
    SetReady(true);
  }
}

ExtensionUserScriptLoader::~ExtensionUserScriptLoader() {
}

void ExtensionUserScriptLoader::UpdateHostsInfo(
    const std::set<HostID>& changed_hosts) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context());
  for (const HostID& host_id : changed_hosts) {
    const Extension* extension =
        registry->GetExtensionById(host_id.id(), ExtensionRegistry::ENABLED);
    // |changed_hosts_| may include hosts that have been removed,
    // which leads to the above lookup failing. In this case, just continue.
    if (!extension)
      continue;
    AddHostInfo(host_id, ExtensionSet::ExtensionPathAndDefaultLocale(
                             extension->path(),
                             LocaleInfo::GetDefaultLocale(extension)));
  }
}

UserScriptLoader::LoadUserScriptsContentFunction
ExtensionUserScriptLoader::GetLoadUserScriptsFunction() {
  return base::Bind(&ExtensionLoadScriptContent);
}

void ExtensionUserScriptLoader::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  RemoveHostInfo(HostID(HostID::EXTENSIONS, extension->id()));
}

void ExtensionUserScriptLoader::OnExtensionSystemReady() {
  SetReady(true);
}

}  // namespace extensions
