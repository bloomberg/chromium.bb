// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/user_script_master.h"

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/pickle.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/version.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/url_constants.h"
#include "net/base/net_util.h"


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
  static const base::StringPiece kRunAtDeclaration("// @run-at");
  static const base::StringPiece kRunAtDocumentStartValue("document-start");
  static const base::StringPiece kRunAtDocumentEndValue("document-end");

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
        scoped_ptr<Version> version(Version::GetVersionFromString(value));
        if (version.get())
          script->set_version(version->GetString());
      } else if (GetDeclarationValue(line, kDescriptionDeclaration, &value)) {
        script->set_description(value);
      } else if (GetDeclarationValue(line, kMatchDeclaration, &value)) {
        URLPattern pattern(UserScript::kValidUserScriptSchemes);
        if (URLPattern::PARSE_SUCCESS !=
            pattern.Parse(value, URLPattern::PARSE_LENIENT))
          return false;
        script->add_url_pattern(pattern);
      } else if (GetDeclarationValue(line, kRunAtDeclaration, &value)) {
        if (value == kRunAtDocumentStartValue)
          script->set_run_location(UserScript::DOCUMENT_START);
        else if (value != kRunAtDocumentEndValue)
          return false;
      }

      // TODO(aa): Handle more types of metadata.
    }

    line_start = line_end + 1;
  }

  // If no patterns were specified, default to @include *. This is what
  // Greasemonkey does.
  if (script->globs().empty() && script->url_patterns().empty())
    script->add_glob("*");

  return true;
}

void UserScriptMaster::ScriptReloader::StartScan(
    const FilePath& script_dir, const UserScriptList& lone_scripts) {
  // Add a reference to ourselves to keep ourselves alive while we're running.
  // Balanced by NotifyMaster().
  AddRef();
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &UserScriptMaster::ScriptReloader::RunScan, script_dir,
          lone_scripts));
}

void UserScriptMaster::ScriptReloader::NotifyMaster(
    base::SharedMemory* memory) {
  // The master went away, so these new scripts aren't useful anymore.
  if (!master_)
    delete memory;
  else
    master_->NewScriptsAvailable(memory);

  // Drop our self-reference.
  // Balances StartScan().
  Release();
}

static bool LoadScriptContent(UserScript::File* script_file) {
  std::string content;
  const FilePath& path = ExtensionResource::GetFilePath(
      script_file->extension_root(), script_file->relative_path());
  if (path.empty() || !file_util::ReadFileToString(path, &content)) {
    LOG(WARNING) << "Failed to load user script file: " << path.value();
    return false;
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

// static
void UserScriptMaster::ScriptReloader::LoadScriptsFromDirectory(
    const FilePath& script_dir, UserScriptList* result) {
  // Clear the list. We will populate it with the scripts found in script_dir.
  result->clear();

  // Find all the scripts in |script_dir|.
  if (!script_dir.value().empty()) {
    // Create the "<Profile>/User Scripts" directory if it doesn't exist
    if (!file_util::DirectoryExists(script_dir))
      file_util::CreateDirectory(script_dir);

    file_util::FileEnumerator enumerator(script_dir, false,
                                         file_util::FileEnumerator::FILES,
                                         FILE_PATH_LITERAL("*.user.js"));
    for (FilePath file = enumerator.Next(); !file.value().empty();
         file = enumerator.Next()) {
      result->push_back(UserScript());
      UserScript& user_script = result->back();

      // We default standalone user scripts to document-end for better
      // Greasemonkey compatibility.
      user_script.set_run_location(UserScript::DOCUMENT_END);

      // Push single js file in this UserScript.
      GURL url(std::string(chrome::kUserScriptScheme) + ":/" +
          net::FilePathToFileURL(file).ExtractFileName());
      user_script.js_scripts().push_back(UserScript::File(
          script_dir, file.BaseName(), url));
      UserScript::File& script_file = user_script.js_scripts().back();
      if (!LoadScriptContent(&script_file))
        result->pop_back();
      else
        ParseMetadataHeader(script_file.GetContent(), &user_script);
    }
  }
}

static void LoadLoneScripts(UserScriptList* lone_scripts) {
  for (size_t i = 0; i < lone_scripts->size(); ++i) {
    UserScript& script = lone_scripts->at(i);
    for (size_t k = 0; k < script.js_scripts().size(); ++k) {
      UserScript::File& script_file = script.js_scripts()[k];
      if (script_file.GetContent().empty())
        LoadScriptContent(&script_file);
    }
    for (size_t k = 0; k < script.css_scripts().size(); ++k) {
      UserScript::File& script_file = script.css_scripts()[k];
      if (script_file.GetContent().empty())
        LoadScriptContent(&script_file);
    }
  }
}

// Pickle user scripts and return pointer to the shared memory.
static base::SharedMemory* Serialize(const UserScriptList& scripts) {
  Pickle pickle;
  pickle.WriteSize(scripts.size());
  for (size_t i = 0; i < scripts.size(); i++) {
    const UserScript& script = scripts[i];
    // TODO(aa): This can be replaced by sending content script metadata to
    // renderers along with other extension data in ViewMsg_ExtensionLoaded.
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

// This method will be called from the file thread
void UserScriptMaster::ScriptReloader::RunScan(
    const FilePath script_dir, UserScriptList lone_script) {
  UserScriptList scripts;
  // Get list of user scripts.
  if (!script_dir.empty())
    LoadScriptsFromDirectory(script_dir, &scripts);

  LoadLoneScripts(&lone_script);

  // Merge with the explicit scripts
  scripts.reserve(scripts.size() + lone_script.size());
  scripts.insert(scripts.end(),
      lone_script.begin(), lone_script.end());

  // Scripts now contains list of up-to-date scripts. Load the content in the
  // shared memory and let the master know it's ready. We need to post the task
  // back even if no scripts ware found to balance the AddRef/Release calls
  BrowserThread::PostTask(
      master_thread_id_, FROM_HERE,
      NewRunnableMethod(
          this, &ScriptReloader::NotifyMaster, Serialize(scripts)));
}


UserScriptMaster::UserScriptMaster(const FilePath& script_dir, Profile* profile)
    : user_script_dir_(script_dir),
      extensions_service_ready_(false),
      pending_scan_(false),
      profile_(profile) {
  registrar_.Add(this, NotificationType::EXTENSIONS_READY,
                 Source<Profile>(profile_));
  registrar_.Add(this, NotificationType::EXTENSION_LOADED,
                 Source<Profile>(profile_));
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
                 Source<Profile>(profile_));
  registrar_.Add(this, NotificationType::EXTENSION_USER_SCRIPTS_UPDATED,
                 Source<Profile>(profile_));
}

UserScriptMaster::~UserScriptMaster() {
  if (script_reloader_)
    script_reloader_->DisownMaster();
}

void UserScriptMaster::NewScriptsAvailable(base::SharedMemory* handle) {
  // Ensure handle is deleted or released.
  scoped_ptr<base::SharedMemory> handle_deleter(handle);

  if (pending_scan_) {
    // While we were scanning, there were further changes.  Don't bother
    // notifying about these scripts and instead just immediately rescan.
    pending_scan_ = false;
    StartScan();
  } else {
    // We're no longer scanning.
    script_reloader_ = NULL;
    // We've got scripts ready to go.
    shared_memory_.swap(handle_deleter);

    NotificationService::current()->Notify(
        NotificationType::USER_SCRIPTS_UPDATED,
        Source<Profile>(profile_),
        Details<base::SharedMemory>(handle));
  }
}

void UserScriptMaster::Observe(NotificationType type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSIONS_READY:
      extensions_service_ready_ = true;
      StartScan();
      break;
    case NotificationType::EXTENSION_LOADED: {
      // Add any content scripts inside the extension.
      const Extension* extension = Details<const Extension>(details).ptr();
      bool incognito_enabled = profile_->GetExtensionService()->
          IsIncognitoEnabled(extension);
      bool allow_file_access = profile_->GetExtensionService()->
          AllowFileAccess(extension);
      const UserScriptList& scripts = extension->content_scripts();
      for (UserScriptList::const_iterator iter = scripts.begin();
           iter != scripts.end(); ++iter) {
        lone_scripts_.push_back(*iter);
        lone_scripts_.back().set_incognito_enabled(incognito_enabled);
        lone_scripts_.back().set_allow_file_access(allow_file_access);
      }
      if (extensions_service_ready_)
        StartScan();
      break;
    }
    case NotificationType::EXTENSION_UNLOADED: {
      // Remove any content scripts.
      const Extension* extension =
          Details<UnloadedExtensionInfo>(details)->extension;
      UserScriptList new_lone_scripts;
      for (UserScriptList::iterator iter = lone_scripts_.begin();
           iter != lone_scripts_.end(); ++iter) {
        if (iter->extension_id() != extension->id())
          new_lone_scripts.push_back(*iter);
      }
      lone_scripts_ = new_lone_scripts;
      StartScan();

      // TODO(aa): Do we want to do something smarter for the scripts that have
      // already been injected?

      break;
    }
    case NotificationType::EXTENSION_USER_SCRIPTS_UPDATED: {
      const Extension* extension = Details<const Extension>(details).ptr();
      UserScriptList new_lone_scripts;
      bool incognito_enabled = profile_->GetExtensionService()->
          IsIncognitoEnabled(extension);
      bool allow_file_access = profile_->GetExtensionService()->
          AllowFileAccess(extension);
      for (UserScriptList::iterator iter = lone_scripts_.begin();
           iter != lone_scripts_.end(); ++iter) {
        if (iter->extension_id() == extension->id()) {
          iter->set_incognito_enabled(incognito_enabled);
          iter->set_allow_file_access(allow_file_access);
        }
      }
      StartScan();
      break;
    }

    default:
      DCHECK(false);
  }
}

void UserScriptMaster::StartScan() {
  if (!script_reloader_)
    script_reloader_ = new ScriptReloader(this);

  script_reloader_->StartScan(user_script_dir_, lone_scripts_);
}
