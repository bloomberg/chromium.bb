// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/user_script_loader.h"

#include <set>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/shared_memory.h"
#include "base/version.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/i18n/default_locale_handler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/component_extension_resource_manager.h"
#include "extensions/browser/content_verifier.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/file_util.h"
#include "extensions/common/message_bundle.h"
#include "extensions/common/one_shot_event.h"
#include "ui/base/resource/resource_bundle.h"

using content::BrowserThread;
using extensions::ExtensionsBrowserClient;

namespace extensions {

namespace {

typedef base::Callback<
    void(scoped_ptr<UserScriptList>, scoped_ptr<base::SharedMemory>)>
    LoadScriptsCallback;

void VerifyContent(scoped_refptr<ContentVerifier> verifier,
                   const ExtensionId& extension_id,
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

bool LoadScriptContent(const ExtensionId& extension_id,
                       UserScript::File* script_file,
                       const SubstitutionMap* localization_messages,
                       scoped_refptr<ContentVerifier> verifier) {
  std::string content;
  const base::FilePath& path = ExtensionResource::GetFilePath(
      script_file->extension_root(),
      script_file->relative_path(),
      ExtensionResource::SYMLINKS_MUST_RESOLVE_WITHIN_ROOT);
  if (path.empty()) {
    int resource_id;
    if (ExtensionsBrowserClient::Get()->GetComponentExtensionResourceManager()->
        IsComponentExtensionResource(script_file->extension_root(),
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
    if (verifier) {
      content::BrowserThread::PostTask(content::BrowserThread::IO,
                                       FROM_HERE,
                                       base::Bind(&VerifyContent,
                                                  verifier,
                                                  extension_id,
                                                  script_file->extension_root(),
                                                  script_file->relative_path(),
                                                  content));
    }
  }

  // Localize the content.
  if (localization_messages) {
    std::string error;
    MessageBundle::ReplaceMessagesWithExternalDictionary(
        *localization_messages, &content, &error);
    if (!error.empty()) {
      LOG(WARNING) << "Failed to replace messages in script: " << error;
    }
  }

  // Remove BOM from the content.
  std::string::size_type index = content.find(base::kUtf8ByteOrderMark);
  if (index == 0) {
    script_file->set_content(content.substr(strlen(base::kUtf8ByteOrderMark)));
  } else {
    script_file->set_content(content);
  }

  return true;
}

SubstitutionMap* GetLocalizationMessages(const ExtensionsInfo& extensions_info,
                                         const ExtensionId& extension_id) {
  ExtensionsInfo::const_iterator iter = extensions_info.find(extension_id);
  if (iter == extensions_info.end())
    return NULL;
  return file_util::LoadMessageBundleSubstitutionMap(
      iter->second.first, extension_id, iter->second.second);
}

void LoadUserScripts(UserScriptList* user_scripts,
                     const ExtensionsInfo& extensions_info,
                     const std::set<int64>& added_script_ids,
                     ContentVerifier* verifier) {
  for (UserScriptList::iterator script = user_scripts->begin();
       script != user_scripts->end();
       ++script) {
    if (added_script_ids.count(script->id()) == 0)
      continue;
    scoped_ptr<SubstitutionMap> localization_messages(
        GetLocalizationMessages(extensions_info, script->extension_id()));
    for (size_t k = 0; k < script->js_scripts().size(); ++k) {
      UserScript::File& script_file = script->js_scripts()[k];
      if (script_file.GetContent().empty())
        LoadScriptContent(script->extension_id(), &script_file, NULL, verifier);
    }
    for (size_t k = 0; k < script->css_scripts().size(); ++k) {
      UserScript::File& script_file = script->css_scripts()[k];
      if (script_file.GetContent().empty())
        LoadScriptContent(script->extension_id(),
                          &script_file,
                          localization_messages.get(),
                          verifier);
    }
  }
}

// Pickle user scripts and return pointer to the shared memory.
scoped_ptr<base::SharedMemory> Serialize(const UserScriptList& scripts) {
  Pickle pickle;
  pickle.WriteUInt64(scripts.size());
  for (UserScriptList::const_iterator script = scripts.begin();
       script != scripts.end();
       ++script) {
    // TODO(aa): This can be replaced by sending content script metadata to
    // renderers along with other extension data in ExtensionMsg_Loaded.
    // See crbug.com/70516.
    script->Pickle(&pickle);
    // Write scripts as 'data' so that we can read it out in the slave without
    // allocating a new string.
    for (size_t j = 0; j < script->js_scripts().size(); j++) {
      base::StringPiece contents = script->js_scripts()[j].GetContent();
      pickle.WriteData(contents.data(), contents.length());
    }
    for (size_t j = 0; j < script->css_scripts().size(); j++) {
      base::StringPiece contents = script->css_scripts()[j].GetContent();
      pickle.WriteData(contents.data(), contents.length());
    }
  }

  // Create the shared memory object.
  base::SharedMemory shared_memory;

  base::SharedMemoryCreateOptions options;
  options.size = pickle.size();
  options.share_read_only = true;
  if (!shared_memory.Create(options))
    return scoped_ptr<base::SharedMemory>();

  if (!shared_memory.Map(pickle.size()))
    return scoped_ptr<base::SharedMemory>();

  // Copy the pickle to shared memory.
  memcpy(shared_memory.memory(), pickle.data(), pickle.size());

  base::SharedMemoryHandle readonly_handle;
  if (!shared_memory.ShareReadOnlyToProcess(base::GetCurrentProcessHandle(),
                                            &readonly_handle))
    return scoped_ptr<base::SharedMemory>();

  return make_scoped_ptr(new base::SharedMemory(readonly_handle,
                                                /*read_only=*/true));
}

void LoadScriptsOnFileThread(scoped_ptr<UserScriptList> user_scripts,
                             const ExtensionsInfo& extensions_info,
                             const std::set<int64>& added_script_ids,
                             scoped_refptr<ContentVerifier> verifier,
                             LoadScriptsCallback callback) {
  DCHECK(user_scripts.get());
  LoadUserScripts(
      user_scripts.get(), extensions_info, added_script_ids, verifier);
  scoped_ptr<base::SharedMemory> memory = Serialize(*user_scripts);
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(callback, base::Passed(&user_scripts), base::Passed(&memory)));
}

// Helper function to parse greasesmonkey headers
bool GetDeclarationValue(const base::StringPiece& line,
                         const base::StringPiece& prefix,
                         std::string* value) {
  base::StringPiece::size_type index = line.find(prefix);
  if (index == base::StringPiece::npos)
    return false;

  std::string temp(line.data() + index + prefix.length(),
                   line.length() - index - prefix.length());

  if (temp.empty() || !IsWhitespace(temp[0]))
    return false;

  base::TrimWhitespaceASCII(temp, base::TRIM_ALL, value);
  return true;
}

}  // namespace

// static
bool UserScriptLoader::ParseMetadataHeader(const base::StringPiece& script_text,
                                           UserScript* script) {
  // http://wiki.greasespot.net/Metadata_block
  base::StringPiece line;
  size_t line_start = 0;
  size_t line_end = line_start;
  bool in_metadata = false;

  static const base::StringPiece kUserScriptBegin("// ==UserScript==");
  static const base::StringPiece kUserScriptEng("// ==/UserScript==");
  static const base::StringPiece kNamespaceDeclaration("// @namespace");
  static const base::StringPiece kNameDeclaration("// @name");
  static const base::StringPiece kVersionDeclaration("// @version");
  static const base::StringPiece kDescriptionDeclaration("// @description");
  static const base::StringPiece kIncludeDeclaration("// @include");
  static const base::StringPiece kExcludeDeclaration("// @exclude");
  static const base::StringPiece kMatchDeclaration("// @match");
  static const base::StringPiece kExcludeMatchDeclaration("// @exclude_match");
  static const base::StringPiece kRunAtDeclaration("// @run-at");
  static const base::StringPiece kRunAtDocumentStartValue("document-start");
  static const base::StringPiece kRunAtDocumentEndValue("document-end");
  static const base::StringPiece kRunAtDocumentIdleValue("document-idle");

  while (line_start < script_text.length()) {
    line_end = script_text.find('\n', line_start);

    // Handle the case where there is no trailing newline in the file.
    if (line_end == std::string::npos)
      line_end = script_text.length() - 1;

    line.set(script_text.data() + line_start, line_end - line_start);

    if (!in_metadata) {
      if (line.starts_with(kUserScriptBegin))
        in_metadata = true;
    } else {
      if (line.starts_with(kUserScriptEng))
        break;

      std::string value;
      if (GetDeclarationValue(line, kIncludeDeclaration, &value)) {
        // We escape some characters that MatchPattern() considers special.
        ReplaceSubstringsAfterOffset(&value, 0, "\\", "\\\\");
        ReplaceSubstringsAfterOffset(&value, 0, "?", "\\?");
        script->add_glob(value);
      } else if (GetDeclarationValue(line, kExcludeDeclaration, &value)) {
        ReplaceSubstringsAfterOffset(&value, 0, "\\", "\\\\");
        ReplaceSubstringsAfterOffset(&value, 0, "?", "\\?");
        script->add_exclude_glob(value);
      } else if (GetDeclarationValue(line, kNamespaceDeclaration, &value)) {
        script->set_name_space(value);
      } else if (GetDeclarationValue(line, kNameDeclaration, &value)) {
        script->set_name(value);
      } else if (GetDeclarationValue(line, kVersionDeclaration, &value)) {
        Version version(value);
        if (version.IsValid())
          script->set_version(version.GetString());
      } else if (GetDeclarationValue(line, kDescriptionDeclaration, &value)) {
        script->set_description(value);
      } else if (GetDeclarationValue(line, kMatchDeclaration, &value)) {
        URLPattern pattern(UserScript::ValidUserScriptSchemes());
        if (URLPattern::PARSE_SUCCESS != pattern.Parse(value))
          return false;
        script->add_url_pattern(pattern);
      } else if (GetDeclarationValue(line, kExcludeMatchDeclaration, &value)) {
        URLPattern exclude(UserScript::ValidUserScriptSchemes());
        if (URLPattern::PARSE_SUCCESS != exclude.Parse(value))
          return false;
        script->add_exclude_url_pattern(exclude);
      } else if (GetDeclarationValue(line, kRunAtDeclaration, &value)) {
        if (value == kRunAtDocumentStartValue)
          script->set_run_location(UserScript::DOCUMENT_START);
        else if (value == kRunAtDocumentEndValue)
          script->set_run_location(UserScript::DOCUMENT_END);
        else if (value == kRunAtDocumentIdleValue)
          script->set_run_location(UserScript::DOCUMENT_IDLE);
        else
          return false;
      }

      // TODO(aa): Handle more types of metadata.
    }

    line_start = line_end + 1;
  }

  // If no patterns were specified, default to @include *. This is what
  // Greasemonkey does.
  if (script->globs().empty() && script->url_patterns().is_empty())
    script->add_glob("*");

  return true;
}

// static
void UserScriptLoader::LoadScriptsForTest(UserScriptList* user_scripts) {
  ExtensionsInfo info;
  std::set<int64> added_script_ids;
  for (UserScriptList::iterator it = user_scripts->begin();
       it != user_scripts->end();
       ++it) {
    added_script_ids.insert(it->id());
  }
  LoadUserScripts(
      user_scripts, info, added_script_ids, NULL /* no verifier for testing */);
}

UserScriptLoader::UserScriptLoader(Profile* profile,
                                   const ExtensionId& owner_extension_id,
                                   bool listen_for_extension_system_loaded)
    : user_scripts_(new UserScriptList()),
      extension_system_ready_(false),
      pending_load_(false),
      profile_(profile),
      owner_extension_id_(owner_extension_id),
      weak_factory_(this),
      extension_registry_observer_(this) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(profile));
  if (listen_for_extension_system_loaded) {
    ExtensionSystem::Get(profile_)->ready().Post(
        FROM_HERE,
        base::Bind(&UserScriptLoader::OnExtensionSystemReady,
                   weak_factory_.GetWeakPtr()));
  } else {
    extension_system_ready_ = true;
  }
  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

UserScriptLoader::~UserScriptLoader() {
}

void UserScriptLoader::AddScripts(const std::set<UserScript>& scripts) {
  for (std::set<UserScript>::const_iterator it = scripts.begin();
       it != scripts.end();
       ++it) {
    removed_scripts_.erase(*it);
    added_scripts_.insert(*it);
  }
  AttemptLoad();
}

void UserScriptLoader::RemoveScripts(const std::set<UserScript>& scripts) {
  for (std::set<UserScript>::const_iterator it = scripts.begin();
       it != scripts.end();
       ++it) {
    added_scripts_.erase(*it);
    removed_scripts_.insert(*it);
  }
  AttemptLoad();
}

void UserScriptLoader::ClearScripts() {
  clear_scripts_ = true;
  added_scripts_.clear();
  removed_scripts_.clear();
  AttemptLoad();
}

void UserScriptLoader::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  DCHECK_EQ(type, content::NOTIFICATION_RENDERER_PROCESS_CREATED);
  content::RenderProcessHost* process =
      content::Source<content::RenderProcessHost>(source).ptr();
  Profile* profile = Profile::FromBrowserContext(process->GetBrowserContext());
  if (!profile_->IsSameProfile(profile))
    return;
  if (scripts_ready()) {
    SendUpdate(process,
               shared_memory_.get(),
               std::set<ExtensionId>());  // Include all extensions.
  }
}

void UserScriptLoader::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  extensions_info_.erase(extension->id());
}

void UserScriptLoader::OnExtensionSystemReady() {
  extension_system_ready_ = true;
  AttemptLoad();
}

bool UserScriptLoader::ScriptsMayHaveChanged() const {
  // Scripts may have changed if there are scripts added, scripts removed, or
  // if scripts were cleared and either:
  // (1) A load is in progress (which may result in a non-zero number of
  //     scripts that need to be cleared), or
  // (2) The current set of scripts is non-empty (so they need to be cleared).
  return (added_scripts_.size() ||
          removed_scripts_.size() ||
          (clear_scripts_ &&
           (is_loading() || user_scripts_->size())));
}

void UserScriptLoader::AttemptLoad() {
  if (extension_system_ready_ && ScriptsMayHaveChanged()) {
    if (is_loading())
      pending_load_ = true;
    else
      StartLoad();
  }
}

void UserScriptLoader::StartLoad() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!is_loading());

  // If scripts were marked for clearing before adding and removing, then clear
  // them.
  if (clear_scripts_) {
    user_scripts_->clear();
  } else {
    for (UserScriptList::iterator it = user_scripts_->begin();
         it != user_scripts_->end();) {
      if (removed_scripts_.count(*it))
        it = user_scripts_->erase(it);
      else
        ++it;
    }
  }

  user_scripts_->insert(
      user_scripts_->end(), added_scripts_.begin(), added_scripts_.end());

  std::set<int64> added_script_ids;
  for (std::set<UserScript>::const_iterator it = added_scripts_.begin();
       it != added_scripts_.end();
       ++it) {
    added_script_ids.insert(it->id());
  }

  // Expand |changed_extensions_| for OnScriptsLoaded, which will use it in
  // its IPC message. This must be done before we clear |added_scripts_| and
  // |removed_scripts_| below.
  std::set<UserScript> changed_scripts(added_scripts_);
  changed_scripts.insert(removed_scripts_.begin(), removed_scripts_.end());
  ExpandChangedExtensions(changed_scripts);

  // Update |extensions_info_| to contain info from every extension in
  // |changed_extensions_| before passing it to LoadScriptsOnFileThread.
  UpdateExtensionsInfo();

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&LoadScriptsOnFileThread,
                 base::Passed(&user_scripts_),
                 extensions_info_,
                 added_script_ids,
                 make_scoped_refptr(
                     ExtensionSystem::Get(profile_)->content_verifier()),
                 base::Bind(&UserScriptLoader::OnScriptsLoaded,
                            weak_factory_.GetWeakPtr())));

  clear_scripts_ = false;
  added_scripts_.clear();
  removed_scripts_.clear();
  user_scripts_.reset(NULL);
}

void UserScriptLoader::OnScriptsLoaded(
    scoped_ptr<UserScriptList> user_scripts,
    scoped_ptr<base::SharedMemory> shared_memory) {
  user_scripts_.reset(user_scripts.release());
  if (pending_load_) {
    // While we were loading, there were further changes. Don't bother
    // notifying about these scripts and instead just immediately reload.
    pending_load_ = false;
    StartLoad();
    return;
  }

  if (shared_memory.get() == NULL) {
    // This can happen if we run out of file descriptors.  In that case, we
    // have a choice between silently omitting all user scripts for new tabs,
    // by nulling out shared_memory_, or only silently omitting new ones by
    // leaving the existing object in place. The second seems less bad, even
    // though it removes the possibility that freeing the shared memory block
    // would open up enough FDs for long enough for a retry to succeed.

    // Pretend the extension change didn't happen.
    return;
  }

  // We've got scripts ready to go.
  shared_memory_.reset(shared_memory.release());

  for (content::RenderProcessHost::iterator i(
           content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd();
       i.Advance()) {
    SendUpdate(i.GetCurrentValue(), shared_memory_.get(), changed_extensions_);
  }
  changed_extensions_.clear();

  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_USER_SCRIPTS_UPDATED,
      content::Source<Profile>(profile_),
      content::Details<base::SharedMemory>(shared_memory_.get()));
}

void UserScriptLoader::SendUpdate(
    content::RenderProcessHost* process,
    base::SharedMemory* shared_memory,
    const std::set<ExtensionId>& changed_extensions) {
  // Don't allow injection of content scripts into <webview>.
  if (process->IsIsolatedGuest())
    return;

  Profile* profile = Profile::FromBrowserContext(process->GetBrowserContext());
  // Make sure we only send user scripts to processes in our profile.
  if (!profile_->IsSameProfile(profile))
    return;

  // If the process is being started asynchronously, early return.  We'll end up
  // calling InitUserScripts when it's created which will call this again.
  base::ProcessHandle handle = process->GetHandle();
  if (!handle)
    return;

  base::SharedMemoryHandle handle_for_process;
  if (!shared_memory->ShareToProcess(handle, &handle_for_process))
    return;  // This can legitimately fail if the renderer asserts at startup.

  if (base::SharedMemory::IsHandleValid(handle_for_process)) {
    process->Send(new ExtensionMsg_UpdateUserScripts(
        handle_for_process, "" /* owner */, changed_extensions));
  }
}

void UserScriptLoader::ExpandChangedExtensions(
    const std::set<UserScript>& scripts) {
  for (std::set<UserScript>::const_iterator it = scripts.begin();
       it != scripts.end();
       ++it) {
    changed_extensions_.insert(it->extension_id());
  }
}

void UserScriptLoader::UpdateExtensionsInfo() {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  for (std::set<ExtensionId>::const_iterator it = changed_extensions_.begin();
       it != changed_extensions_.end();
       ++it) {
    if (extensions_info_.find(*it) == extensions_info_.end()) {
      const Extension* extension =
          registry->GetExtensionById(*it, ExtensionRegistry::EVERYTHING);
      // |changed_extensions_| may include extensions that have been removed,
      // which leads to the above lookup failing. In this case, just continue.
      if (!extension)
        continue;
      extensions_info_[*it] = ExtensionSet::ExtensionPathAndDefaultLocale(
          extension->path(), LocaleInfo::GetDefaultLocale(extension));
    }
  }
}

}  // namespace extensions
