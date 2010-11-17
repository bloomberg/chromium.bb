// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/in_process_webkit/dom_storage_context.h"

#include <algorithm>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/in_process_webkit/dom_storage_area.h"
#include "chrome/browser/in_process_webkit/dom_storage_namespace.h"
#include "chrome/browser/in_process_webkit/webkit_context.h"
#include "chrome/common/dom_storage_common.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebSecurityOrigin;

const FilePath::CharType DOMStorageContext::kLocalStorageDirectory[] =
    FILE_PATH_LITERAL("Local Storage");

const FilePath::CharType DOMStorageContext::kLocalStorageExtension[] =
    FILE_PATH_LITERAL(".localstorage");

static const FilePath::CharType kLocalStorageOldPath[] =
    FILE_PATH_LITERAL("localStorage");

// TODO(jorlow): Remove after Chrome 4 ships.
static void MigrateLocalStorageDirectory(const FilePath& data_path) {
  FilePath new_path = data_path.Append(
      DOMStorageContext::kLocalStorageDirectory);
  FilePath old_path = data_path.Append(kLocalStorageOldPath);
  if (!file_util::DirectoryExists(new_path) &&
      file_util::DirectoryExists(old_path)) {
    file_util::Move(old_path, new_path);
  }
}

DOMStorageContext::DOMStorageContext(WebKitContext* webkit_context)
    : last_storage_area_id_(0),
      last_session_storage_namespace_id_on_ui_thread_(kLocalStorageNamespaceId),
      last_session_storage_namespace_id_on_io_thread_(kLocalStorageNamespaceId),
      webkit_context_(webkit_context) {
}

DOMStorageContext::~DOMStorageContext() {
  // This should not go away until all DOM Storage Dispatcher hosts have gone
  // away.  And they remove themselves from this list.
  DCHECK(dispatcher_host_set_.empty());

  for (StorageNamespaceMap::iterator iter(storage_namespace_map_.begin());
       iter != storage_namespace_map_.end(); ++iter) {
    delete iter->second;
  }
}

int64 DOMStorageContext::AllocateStorageAreaId() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  return ++last_storage_area_id_;
}

int64 DOMStorageContext::AllocateSessionStorageNamespaceId() {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI))
    return ++last_session_storage_namespace_id_on_ui_thread_;
  return --last_session_storage_namespace_id_on_io_thread_;
}

int64 DOMStorageContext::CloneSessionStorage(int64 original_id) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  int64 clone_id = AllocateSessionStorageNamespaceId();
  BrowserThread::PostTask(
      BrowserThread::WEBKIT, FROM_HERE, NewRunnableFunction(
          &DOMStorageContext::CompleteCloningSessionStorage,
          this, original_id, clone_id));
  return clone_id;
}

void DOMStorageContext::RegisterStorageArea(DOMStorageArea* storage_area) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  int64 id = storage_area->id();
  DCHECK(!GetStorageArea(id));
  storage_area_map_[id] = storage_area;
}

void DOMStorageContext::UnregisterStorageArea(DOMStorageArea* storage_area) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  int64 id = storage_area->id();
  DCHECK(GetStorageArea(id));
  storage_area_map_.erase(id);
}

DOMStorageArea* DOMStorageContext::GetStorageArea(int64 id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  StorageAreaMap::iterator iter = storage_area_map_.find(id);
  if (iter == storage_area_map_.end())
    return NULL;
  return iter->second;
}

void DOMStorageContext::DeleteSessionStorageNamespace(int64 namespace_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  StorageNamespaceMap::iterator iter =
      storage_namespace_map_.find(namespace_id);
  if (iter == storage_namespace_map_.end())
    return;
  DCHECK(iter->second->dom_storage_type() == DOM_STORAGE_SESSION);
  delete iter->second;
  storage_namespace_map_.erase(iter);
}

DOMStorageNamespace* DOMStorageContext::GetStorageNamespace(
    int64 id, bool allocation_allowed) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  StorageNamespaceMap::iterator iter = storage_namespace_map_.find(id);
  if (iter != storage_namespace_map_.end())
    return iter->second;
  if (!allocation_allowed)
    return NULL;
  if (id == kLocalStorageNamespaceId)
    return CreateLocalStorage();
  return CreateSessionStorage(id);
}

void DOMStorageContext::RegisterDispatcherHost(
    DOMStorageDispatcherHost* dispatcher_host) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(dispatcher_host_set_.find(dispatcher_host) ==
         dispatcher_host_set_.end());
  dispatcher_host_set_.insert(dispatcher_host);
}

void DOMStorageContext::UnregisterDispatcherHost(
    DOMStorageDispatcherHost* dispatcher_host) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(dispatcher_host_set_.find(dispatcher_host) !=
         dispatcher_host_set_.end());
  dispatcher_host_set_.erase(dispatcher_host);
}

const DOMStorageContext::DispatcherHostSet*
DOMStorageContext::GetDispatcherHostSet() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return &dispatcher_host_set_;
}

void DOMStorageContext::PurgeMemory() {
  // It is only safe to purge the memory from the LocalStorage namespace,
  // because it is backed by disk and can be reloaded later.  If we purge a
  // SessionStorage namespace, its data will be gone forever, because it isn't
  // currently backed by disk.
  DOMStorageNamespace* local_storage =
      GetStorageNamespace(kLocalStorageNamespaceId, false);
  if (local_storage)
    local_storage->PurgeMemory();
}

void DOMStorageContext::DeleteDataModifiedSince(
    const base::Time& cutoff,
    const char* url_scheme_to_be_skipped,
    const std::vector<string16>& protected_origins) {
  // Make sure that we don't delete a database that's currently being accessed
  // by unloading all of the databases temporarily.
  PurgeMemory();

  file_util::FileEnumerator file_enumerator(
      webkit_context_->data_path().Append(kLocalStorageDirectory), false,
      file_util::FileEnumerator::FILES);
  for (FilePath path = file_enumerator.Next(); !path.value().empty();
       path = file_enumerator.Next()) {
    WebSecurityOrigin web_security_origin =
        WebSecurityOrigin::createFromDatabaseIdentifier(
            webkit_glue::FilePathToWebString(path.BaseName()));
    if (EqualsASCII(web_security_origin.protocol(), url_scheme_to_be_skipped))
      continue;

    std::vector<string16>::const_iterator find_iter =
        std::find(protected_origins.begin(), protected_origins.end(),
            web_security_origin.databaseIdentifier());
    if (find_iter != protected_origins.end())
      continue;

    file_util::FileEnumerator::FindInfo find_info;
    file_enumerator.GetFindInfo(&find_info);
    if (file_util::HasFileBeenModifiedSince(find_info, cutoff))
      file_util::Delete(path, false);
  }
}

void DOMStorageContext::DeleteLocalStorageFile(const FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));

  // Make sure that we don't delete a database that's currently being accessed
  // by unloading all of the databases temporarily.
  // TODO(bulach): both this method and DeleteDataModifiedSince could purge
  // only the memory used by the specific file instead of all memory at once.
  // See http://crbug.com/32000
  PurgeMemory();
  file_util::Delete(file_path, false);
}

void DOMStorageContext::DeleteLocalStorageForOrigin(const string16& origin_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  DeleteLocalStorageFile(GetLocalStorageFilePath(origin_id));
}

void DOMStorageContext::DeleteAllLocalStorageFiles() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));

  // Make sure that we don't delete a database that's currently being accessed
  // by unloading all of the databases temporarily.
  PurgeMemory();

  file_util::FileEnumerator file_enumerator(
      webkit_context_->data_path().Append(kLocalStorageDirectory), false,
      file_util::FileEnumerator::FILES);
  for (FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    if (file_path.Extension() == kLocalStorageExtension)
      file_util::Delete(file_path, false);
  }
}

DOMStorageNamespace* DOMStorageContext::CreateLocalStorage() {
  FilePath data_path = webkit_context_->data_path();
  FilePath dir_path;
  if (!data_path.empty()) {
    MigrateLocalStorageDirectory(data_path);
    dir_path = data_path.Append(kLocalStorageDirectory);
  }
  DOMStorageNamespace* new_namespace =
      DOMStorageNamespace::CreateLocalStorageNamespace(this, dir_path);
  RegisterStorageNamespace(new_namespace);
  return new_namespace;
}

DOMStorageNamespace* DOMStorageContext::CreateSessionStorage(
    int64 namespace_id) {
  DOMStorageNamespace* new_namespace =
      DOMStorageNamespace::CreateSessionStorageNamespace(this, namespace_id);
  RegisterStorageNamespace(new_namespace);
  return new_namespace;
}

void DOMStorageContext::RegisterStorageNamespace(
    DOMStorageNamespace* storage_namespace) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  int64 id = storage_namespace->id();
  DCHECK(!GetStorageNamespace(id, false));
  storage_namespace_map_[id] = storage_namespace;
}

/* static */
void DOMStorageContext::CompleteCloningSessionStorage(
    DOMStorageContext* context, int64 existing_id, int64 clone_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  DOMStorageNamespace* existing_namespace =
      context->GetStorageNamespace(existing_id, false);
  // If nothing exists, then there's nothing to clone.
  if (existing_namespace)
    context->RegisterStorageNamespace(existing_namespace->Copy(clone_id));
}

// static
void DOMStorageContext::ClearLocalState(const FilePath& profile_path,
                                        const char* url_scheme_to_be_skipped) {
  file_util::FileEnumerator file_enumerator(profile_path.Append(
      kLocalStorageDirectory), false, file_util::FileEnumerator::FILES);
  for (FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    if (file_path.Extension() == kLocalStorageExtension) {
      WebSecurityOrigin web_security_origin =
          WebSecurityOrigin::createFromDatabaseIdentifier(
              webkit_glue::FilePathToWebString(file_path.BaseName()));
      if (!EqualsASCII(web_security_origin.protocol(),
                       url_scheme_to_be_skipped)) {
        file_util::Delete(file_path, false);
      }
    }
  }
}

FilePath DOMStorageContext::GetLocalStorageFilePath(
    const string16& origin_id) const {
  FilePath storageDir = webkit_context_->data_path().Append(
      DOMStorageContext::kLocalStorageDirectory);
  FilePath::StringType id =
      webkit_glue::WebStringToFilePathString(origin_id);
  return storageDir.Append(id.append(kLocalStorageExtension));
}
