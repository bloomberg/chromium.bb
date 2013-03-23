// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/user_script_master.h"

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/pickle.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/version.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/api/i18n/default_locale_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/extensions/message_bundle.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/common/extension_resource.h"

using content::BrowserThread;

namespace extensions {

// Helper function to parse greasesmonkey headers
static bool GetDeclarationValue(const base::StringPiece& line,
                                const base::StringPiece& prefix,
                                std::string* value) {
  base::StringPiece::size_type index = line.find(prefix);
  if (index == base::StringPiece::npos)
    return false;

  std::string temp(line.data() + index + prefix.length(),
                   line.length() - index - prefix.length());

  if (temp.empty() || !IsWhitespace(temp[0]))
    return false;

  TrimWhitespaceASCII(temp, TRIM_ALL, value);
  return true;
}

UserScriptMaster::ScriptReloader::ScriptReloader(UserScriptMaster* master)
    : master_(master) {
  CHECK(BrowserThread::GetCurrentThreadIdentifier(&master_thread_id_));
}

// static
bool UserScriptMaster::ScriptReloader::ParseMetadataHeader(
      const base::StringPiece& script_text, UserScript* script) {
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

void UserScriptMaster::ScriptReloader::StartLoad(
    const UserScriptList& user_scripts,
    const ExtensionsInfo& extensions_info_) {
  // Add a reference to ourselves to keep ourselves alive while we're running.
  // Balanced by NotifyMaster().
  AddRef();

  this->extensions_info_ = extensions_info_;
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(
          &UserScriptMaster::ScriptReloader::RunLoad, this, user_scripts));
}

UserScriptMaster::ScriptReloader::~ScriptReloader() {}

void UserScriptMaster::ScriptReloader::NotifyMaster(
    base::SharedMemory* memory) {
  // The master went away, so these new scripts aren't useful anymore.
  if (!master_)
    delete memory;
  else
    master_->NewScriptsAvailable(memory);

  // Drop our self-reference.
  // Balances StartLoad().
  Release();
}

static bool LoadScriptContent(UserScript::File* script_file,
                              const SubstitutionMap* localization_messages) {
  std::string content;
  const base::FilePath& path = ExtensionResource::GetFilePath(
      script_file->extension_root(), script_file->relative_path(),
      ExtensionResource::SYMLINKS_MUST_RESOLVE_WITHIN_ROOT);
  if (path.empty()) {
    LOG(WARNING) << "Failed to get file path to "
                 << script_file->relative_path().value() << " from "
                 << script_file->extension_root().value();
    return false;
  }
  if (!file_util::ReadFileToString(path, &content)) {
    LOG(WARNING) << "Failed to load user script file: " << path.value();
    return false;
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
  std::string::size_type index = content.find(kUtf8ByteOrderMark);
  if (index == 0) {
    script_file->set_content(content.substr(strlen(kUtf8ByteOrderMark)));
  } else {
    script_file->set_content(content);
  }

  return true;
}

void UserScriptMaster::ScriptReloader::LoadUserScripts(
    UserScriptList* user_scripts) {
  for (size_t i = 0; i < user_scripts->size(); ++i) {
    UserScript& script = user_scripts->at(i);
    scoped_ptr<SubstitutionMap> localization_messages(
        GetLocalizationMessages(script.extension_id()));
    for (size_t k = 0; k < script.js_scripts().size(); ++k) {
      UserScript::File& script_file = script.js_scripts()[k];
      if (script_file.GetContent().empty())
        LoadScriptContent(&script_file, NULL);
    }
    for (size_t k = 0; k < script.css_scripts().size(); ++k) {
      UserScript::File& script_file = script.css_scripts()[k];
      if (script_file.GetContent().empty())
        LoadScriptContent(&script_file, localization_messages.get());
    }
  }
}

SubstitutionMap* UserScriptMaster::ScriptReloader::GetLocalizationMessages(
    std::string extension_id) {
  if (extensions_info_.find(extension_id) == extensions_info_.end()) {
    return NULL;
  }

  return extension_file_util::LoadMessageBundleSubstitutionMap(
      extensions_info_[extension_id].first,
      extension_id,
      extensions_info_[extension_id].second);
}

// Pickle user scripts and return pointer to the shared memory.
static base::SharedMemory* Serialize(const UserScriptList& scripts) {
  Pickle pickle;
  pickle.WriteUInt64(scripts.size());
  for (size_t i = 0; i < scripts.size(); i++) {
    const UserScript& script = scripts[i];
    // TODO(aa): This can be replaced by sending content script metadata to
    // renderers along with other extension data in ExtensionMsg_Loaded.
    // See crbug.com/70516.
    script.Pickle(&pickle);
    // Write scripts as 'data' so that we can read it out in the slave without
    // allocating a new string.
    for (size_t j = 0; j < script.js_scripts().size(); j++) {
      base::StringPiece contents = script.js_scripts()[j].GetContent();
      pickle.WriteData(contents.data(), contents.length());
    }
    for (size_t j = 0; j < script.css_scripts().size(); j++) {
      base::StringPiece contents = script.css_scripts()[j].GetContent();
      pickle.WriteData(contents.data(), contents.length());
    }
  }

  // Create the shared memory object.
  scoped_ptr<base::SharedMemory> shared_memory(new base::SharedMemory());

  if (!shared_memory->CreateAndMapAnonymous(pickle.size()))
    return NULL;

  // Copy the pickle to shared memory.
  memcpy(shared_memory->memory(), pickle.data(), pickle.size());

  return shared_memory.release();
}

// This method will be called on the file thread.
void UserScriptMaster::ScriptReloader::RunLoad(
    const UserScriptList& user_scripts) {
  LoadUserScripts(const_cast<UserScriptList*>(&user_scripts));

  // Scripts now contains list of up-to-date scripts. Load the content in the
  // shared memory and let the master know it's ready. We need to post the task
  // back even if no scripts ware found to balance the AddRef/Release calls.
  BrowserThread::PostTask(
      master_thread_id_, FROM_HERE,
      base::Bind(
          &ScriptReloader::NotifyMaster, this, Serialize(user_scripts)));
}


UserScriptMaster::UserScriptMaster(Profile* profile)
    : extensions_service_ready_(false),
      pending_load_(false),
      profile_(profile) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSIONS_READY,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

UserScriptMaster::~UserScriptMaster() {
  if (script_reloader_)
    script_reloader_->DisownMaster();
}

void UserScriptMaster::NewScriptsAvailable(base::SharedMemory* handle) {
  // Ensure handle is deleted or released.
  scoped_ptr<base::SharedMemory> handle_deleter(handle);

  if (pending_load_) {
    // While we were loading, there were further changes.  Don't bother
    // notifying about these scripts and instead just immediately reload.
    pending_load_ = false;
    StartLoad();
  } else {
    // We're no longer loading.
    script_reloader_ = NULL;
    // We've got scripts ready to go.
    shared_memory_.swap(handle_deleter);

    for (content::RenderProcessHost::iterator i(
            content::RenderProcessHost::AllHostsIterator());
         !i.IsAtEnd(); i.Advance()) {
      SendUpdate(i.GetCurrentValue(), handle);
    }

    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_USER_SCRIPTS_UPDATED,
        content::Source<Profile>(profile_),
        content::Details<base::SharedMemory>(handle));
  }
}

void UserScriptMaster::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  bool should_start_load = false;
  switch (type) {
    case chrome::NOTIFICATION_EXTENSIONS_READY:
      extensions_service_ready_ = true;
      should_start_load = true;
      break;
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      // Add any content scripts inside the extension.
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      extensions_info_[extension->id()] =
          ExtensionSet::ExtensionPathAndDefaultLocale(
              extension->path(), LocaleInfo::GetDefaultLocale(extension));
      bool incognito_enabled = extensions::ExtensionSystem::Get(profile_)->
          extension_service()->IsIncognitoEnabled(extension->id());
      const UserScriptList& scripts = extension->content_scripts();
      for (UserScriptList::const_iterator iter = scripts.begin();
           iter != scripts.end(); ++iter) {
        user_scripts_.push_back(*iter);
        user_scripts_.back().set_incognito_enabled(incognito_enabled);
      }
      if (extensions_service_ready_)
        should_start_load = true;
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      // Remove any content scripts.
      const Extension* extension =
          content::Details<UnloadedExtensionInfo>(details)->extension;
      extensions_info_.erase(extension->id());
      UserScriptList new_user_scripts;
      for (UserScriptList::iterator iter = user_scripts_.begin();
           iter != user_scripts_.end(); ++iter) {
        if (iter->extension_id() != extension->id())
          new_user_scripts.push_back(*iter);
      }
      user_scripts_ = new_user_scripts;
      should_start_load = true;
      break;
    }
    case content::NOTIFICATION_RENDERER_PROCESS_CREATED: {
      content::RenderProcessHost* process =
          content::Source<content::RenderProcessHost>(source).ptr();
      Profile* profile = Profile::FromBrowserContext(
          process->GetBrowserContext());
      if (!profile_->IsSameProfile(profile))
        return;
      if (ScriptsReady())
        SendUpdate(process, GetSharedMemory());
      break;
    }
    default:
      DCHECK(false);
  }

  if (should_start_load) {
    if (script_reloader_) {
      pending_load_ = true;
    } else {
      StartLoad();
    }
  }
}

void UserScriptMaster::StartLoad() {
  if (!script_reloader_)
    script_reloader_ = new ScriptReloader(this);

  script_reloader_->StartLoad(user_scripts_, extensions_info_);
}

void UserScriptMaster::SendUpdate(content::RenderProcessHost* process,
                                  base::SharedMemory* shared_memory) {
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

  if (base::SharedMemory::IsHandleValid(handle_for_process))
    process->Send(new ExtensionMsg_UpdateUserScripts(handle_for_process));
}

}  // namespace extensions
