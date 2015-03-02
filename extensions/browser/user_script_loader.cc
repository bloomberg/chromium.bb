// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/user_script_loader.h"

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
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/content_verifier.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/file_util.h"

using content::BrowserThread;
using content::BrowserContext;

namespace extensions {

namespace {

using LoadScriptsCallback =
    base::Callback<void(scoped_ptr<UserScriptList>,
                        scoped_ptr<base::SharedMemory>)>;

UserScriptLoader::SubstitutionMap* GetLocalizationMessages(
    const UserScriptLoader::HostsInfo& hosts_info,
    const HostID& host_id) {
  UserScriptLoader::HostsInfo::const_iterator iter = hosts_info.find(host_id);
  if (iter == hosts_info.end())
    return nullptr;
  return file_util::LoadMessageBundleSubstitutionMap(
      iter->second.first, host_id.id(), iter->second.second);
}

void LoadUserScripts(
    UserScriptList* user_scripts,
    const UserScriptLoader::HostsInfo& hosts_info,
    const std::set<int>& added_script_ids,
    const scoped_refptr<ContentVerifier>& verifier,
    UserScriptLoader::LoadUserScriptsContentFunction callback) {
  for (UserScriptList::iterator script = user_scripts->begin();
       script != user_scripts->end();
       ++script) {
    if (added_script_ids.count(script->id()) == 0)
      continue;
    scoped_ptr<UserScriptLoader::SubstitutionMap> localization_messages(
        GetLocalizationMessages(hosts_info, script->host_id()));
    for (size_t k = 0; k < script->js_scripts().size(); ++k) {
      UserScript::File& script_file = script->js_scripts()[k];
      if (script_file.GetContent().empty())
        callback.Run(script->host_id(), &script_file, NULL, verifier);
    }
    for (size_t k = 0; k < script->css_scripts().size(); ++k) {
      UserScript::File& script_file = script->css_scripts()[k];
      if (script_file.GetContent().empty())
        callback.Run(script->host_id(), &script_file,
                     localization_messages.get(), verifier);
    }
  }
}

// Pickle user scripts and return pointer to the shared memory.
scoped_ptr<base::SharedMemory> Serialize(const UserScriptList& scripts) {
  Pickle pickle;
  pickle.WriteSizeT(scripts.size());
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

void LoadScriptsOnFileThread(
    scoped_ptr<UserScriptList> user_scripts,
    const UserScriptLoader::HostsInfo& hosts_info,
    const std::set<int>& added_script_ids,
    const scoped_refptr<ContentVerifier>& verifier,
    UserScriptLoader::LoadUserScriptsContentFunction function,
    LoadScriptsCallback callback) {
  DCHECK(user_scripts.get());
  LoadUserScripts(user_scripts.get(), hosts_info, added_script_ids,
                  verifier, function);
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

void UserScriptLoader::LoadScriptsForTest(UserScriptList* user_scripts) {
  HostsInfo info;
  std::set<int> added_script_ids;
  for (UserScriptList::iterator it = user_scripts->begin();
       it != user_scripts->end();
       ++it) {
    added_script_ids.insert(it->id());
  }
  LoadUserScripts(user_scripts, info, added_script_ids,
                  NULL /* no verifier for testing */,
                  GetLoadUserScriptsFunction());
}

UserScriptLoader::UserScriptLoader(
    BrowserContext* browser_context,
    const HostID& host_id,
    const scoped_refptr<ContentVerifier>& content_verifier)
    : user_scripts_(new UserScriptList()),
      clear_scripts_(false),
      ready_(false),
      pending_load_(false),
      browser_context_(browser_context),
      host_id_(host_id),
      content_verifier_(content_verifier),
      weak_factory_(this) {
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
  if (!ExtensionsBrowserClient::Get()->IsSameContext(
          browser_context_, process->GetBrowserContext()))
    return;
  if (scripts_ready()) {
    SendUpdate(process, shared_memory_.get(),
               std::set<HostID>());  // Include all hosts.
  }
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
  if (ready_ && ScriptsMayHaveChanged()) {
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

  std::set<int> added_script_ids;
  for (std::set<UserScript>::const_iterator it = added_scripts_.begin();
       it != added_scripts_.end();
       ++it) {
    added_script_ids.insert(it->id());
  }

  // Expand |changed_hosts_| for OnScriptsLoaded, which will use it in
  // its IPC message. This must be done before we clear |added_scripts_| and
  // |removed_scripts_| below.
  std::set<UserScript> changed_scripts(added_scripts_);
  changed_scripts.insert(removed_scripts_.begin(), removed_scripts_.end());
  for (const UserScript& script : changed_scripts)
    changed_hosts_.insert(script.host_id());

  // |changed_hosts_| before passing it to LoadScriptsOnFileThread.
  UpdateHostsInfo(changed_hosts_);

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&LoadScriptsOnFileThread, base::Passed(&user_scripts_),
                 hosts_info_, added_script_ids, content_verifier_,
                 GetLoadUserScriptsFunction(),
                 base::Bind(&UserScriptLoader::OnScriptsLoaded,
                            weak_factory_.GetWeakPtr())));

  clear_scripts_ = false;
  added_scripts_.clear();
  removed_scripts_.clear();
  user_scripts_.reset(NULL);
}

void UserScriptLoader::AddHostInfo(const HostID& host_id,
                                   const PathAndDefaultLocale& location) {
  if (hosts_info_.find(host_id) != hosts_info_.end())
    return;
  hosts_info_[host_id] = location;
}

void UserScriptLoader::RemoveHostInfo(const HostID& host_id) {
  hosts_info_.erase(host_id);
}

void UserScriptLoader::SetReady(bool ready) {
  bool was_ready = ready_;
  ready_ = ready;
  if (ready_ && !was_ready)
    AttemptLoad();
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

  // If user scripts are comming from a <webview>, will only notify the
  // RenderProcessHost of that <webview>; otherwise will notify all of the
  // RenderProcessHosts.
  if (user_scripts && !user_scripts->empty() &&
      (*user_scripts)[0].consumer_instance_type() ==
          UserScript::ConsumerInstanceType::WEBVIEW) {
    DCHECK_EQ(1u, user_scripts->size());
    int render_process_id = (*user_scripts)[0].routing_info().render_process_id;
    content::RenderProcessHost* host =
        content::RenderProcessHost::FromID(render_process_id);
    if (host)
      SendUpdate(host, shared_memory_.get(), changed_hosts_);
  } else {
    for (content::RenderProcessHost::iterator i(
            content::RenderProcessHost::AllHostsIterator());
        !i.IsAtEnd(); i.Advance()) {
      SendUpdate(i.GetCurrentValue(), shared_memory_.get(), changed_hosts_);
    }
  }
  changed_hosts_.clear();

  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_USER_SCRIPTS_UPDATED,
      content::Source<BrowserContext>(browser_context_),
      content::Details<base::SharedMemory>(shared_memory_.get()));
}

void UserScriptLoader::SendUpdate(content::RenderProcessHost* process,
                                  base::SharedMemory* shared_memory,
                                  const std::set<HostID>& changed_hosts) {
  // Don't allow injection of content scripts into <webview>.
  if (process->IsIsolatedGuest())
    return;

  // Make sure we only send user scripts to processes in our browser_context.
  if (!ExtensionsBrowserClient::Get()->IsSameContext(
          browser_context_, process->GetBrowserContext()))
    return;

  // If the process is being started asynchronously, early return.  We'll end up
  // calling InitUserScripts when it's created which will call this again.
  base::ProcessHandle handle = process->GetHandle();
  if (!handle)
    return;

  base::SharedMemoryHandle handle_for_process;
  if (!shared_memory->ShareToProcess(handle, &handle_for_process))
    return;  // This can legitimately fail if the renderer asserts at startup.

  // TODO(hanxi): update the IPC message to send a set of HostIDs to render.
  // Also, remove this function when the refactor is done on render side.
  std::set<std::string> changed_ids_set;
  for (const HostID& id : changed_hosts)
    changed_ids_set.insert(id.id());

  if (base::SharedMemory::IsHandleValid(handle_for_process)) {
    process->Send(new ExtensionMsg_UpdateUserScripts(
        handle_for_process, host_id().id(), changed_ids_set));
  }
}

}  // namespace extensions
