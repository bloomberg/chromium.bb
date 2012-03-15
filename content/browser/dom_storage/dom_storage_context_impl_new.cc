// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/dom_storage_context_impl_new.h"

#include <algorithm>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/quota/special_storage_policy.h"
#include "webkit/dom_storage/dom_storage_context.h"
#include "webkit/dom_storage/dom_storage_task_runner.h"
#include "webkit/quota/special_storage_policy.h"

#ifdef ENABLE_NEW_DOM_STORAGE_BACKEND

using content::BrowserThread;
using dom_storage::DomStorageContext;
using dom_storage::DomStorageWorkerPoolTaskRunner;
using WebKit::WebSecurityOrigin;
using WebKit::WebString;

namespace {


const FilePath::CharType kLocalStorageDirectory[] =
    FILE_PATH_LITERAL("Local Storage");

const FilePath::CharType kLocalStorageExtension[] =
    FILE_PATH_LITERAL(".localstorage");

// TODO(michaeln): move to dom_storage::DomStorageContext
void ClearLocalState(const FilePath& domstorage_path,
                     quota::SpecialStoragePolicy* special_storage_policy,
                     bool clear_all_databases) {
  file_util::FileEnumerator file_enumerator(
      domstorage_path, false, file_util::FileEnumerator::FILES);
  for (FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    if (file_path.Extension() == kLocalStorageExtension) {
      GURL origin(WebSecurityOrigin::createFromDatabaseIdentifier(
          webkit_glue::FilePathToWebString(file_path.BaseName())).toString());
      if (special_storage_policy &&
          special_storage_policy->IsStorageProtected(origin))
        continue;
      if (!clear_all_databases &&
          !special_storage_policy->IsStorageSessionOnly(origin)) {
        continue;
      }
      file_util::Delete(file_path, false);
    }
  }
}

}  // namespace

DOMStorageContextImpl::DOMStorageContextImpl(
    const FilePath& data_path,
    quota::SpecialStoragePolicy* special_storage_policy) {
  base::SequencedWorkerPool* worker_pool = BrowserThread::GetBlockingPool();
  context_ = new dom_storage::DomStorageContext(
      data_path, special_storage_policy,
      new DomStorageWorkerPoolTaskRunner(
          worker_pool,
          worker_pool->GetNamedSequenceToken("dom_storage"),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO)));
}

DOMStorageContextImpl::~DOMStorageContextImpl() {
/*
  // TODO(michaeln): move clearing logic dom_storage::DomStorageContext
  if (save_session_state_)
    return;

  quota::SpecialStoragePolicy* policy = context_->special_storage_policy();
  bool has_session_only_origins =
      policy &&
      policy->HasSessionOnlyOrigins();

  // Clearning only session-only origins, and there are none.
  if (!clear_local_state_on_exit_ && !has_session_only_origins)
    return;

  // Not being on the WEBKIT thread here means we are running in a unit test
  // where no clean up is needed.
  // TODO(michaeln): this CurrentlyOn test is hacky, remove it
  if (BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED)) {
    ClearLocalState(data_path_.Append(kLocalStorageDirectory),
                    policy,
                    clear_local_state_on_exit_);
  }
*/
}

std::vector<FilePath> DOMStorageContextImpl::GetAllStorageFiles() {
  return std::vector<FilePath>();
/*
  std::vector<FilePath> files;
  file_util::FileEnumerator file_enumerator(
      data_path_.Append(kLocalStorageDirectory), false,
      file_util::FileEnumerator::FILES);
  for (FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    if (file_path.Extension() == kLocalStorageExtension)
      files.push_back(file_path);
  }
  return files;
*/
}

FilePath DOMStorageContextImpl::GetFilePath(const string16& origin_id) const {
  return FilePath();
/*
  FilePath storage_dir = data_path_.Append(kLocalStorageDirectory);
  FilePath::StringType id = webkit_glue::WebStringToFilePathString(origin_id);
  return storage_dir.Append(id.append(kLocalStorageExtension));
*/
}

void DOMStorageContextImpl::DeleteForOrigin(const string16& origin_id) {
  // TODO(michaeln): move deletion logic to dom_storage::DomStorageContext
  //DeleteLocalStorageFile(GetFilePath(origin_id));
}

void DOMStorageContextImpl::DeleteLocalStorageFile(const FilePath& file_path) {
  // TODO(michaeln): move deletion logic to dom_storage::DomStorageContext

  // Make sure that we don't delete a database that's currently being accessed
  // by unloading all of the databases temporarily.
  // TODO(bulach): both this method and DeleteDataModifiedSince could purge
  // only the memory used by the specific file instead of all memory at once.
  // See http://crbug.com/32000
  //PurgeMemory();
  //file_util::Delete(file_path, false);
}

void DOMStorageContextImpl::DeleteDataModifiedSince(const base::Time& cutoff) {
  // TODO(michaeln): move deletion logic to dom_storage::DomStorageContext

  // Make sure that we don't delete a database that's currently being accessed
  // by unloading all of the databases temporarily.
  //PurgeMemory();

  //file_util::FileEnumerator file_enumerator(
  //    data_path_.Append(kLocalStorageDirectory), false,
  //    file_util::FileEnumerator::FILES);
  //for (FilePath path = file_enumerator.Next(); !path.value().empty();
  //     path = file_enumerator.Next()) {
  //  GURL origin(WebSecurityOrigin::createFromDatabaseIdentifier(
  //      webkit_glue::FilePathToWebString(path.BaseName())).toString());
  //  if (special_storage_policy_->IsStorageProtected(origin))
  //    continue;

  //  file_util::FileEnumerator::FindInfo find_info;
  //  file_enumerator.GetFindInfo(&find_info);
  //  if (file_util::HasFileBeenModifiedSince(find_info, cutoff))
  //    file_util::Delete(path, false);
  //}
}

void DOMStorageContextImpl::DeleteAllLocalStorageFiles() {
  // TODO(michaeln): move deletion logic to dom_storage::DomStorageContext

  // Make sure that we don't delete a database that's currently being accessed
  // by unloading all of the databases temporarily.
  //PurgeMemory();

  //file_util::FileEnumerator file_enumerator(
  //    data_path_.Append(kLocalStorageDirectory), false,
  //    file_util::FileEnumerator::FILES);
  //for (FilePath file_path = file_enumerator.Next(); !file_path.empty();
  //     file_path = file_enumerator.Next()) {
  //  if (file_path.Extension() == kLocalStorageExtension)
  //    file_util::Delete(file_path, false);
  //}
}

void DOMStorageContextImpl::PurgeMemory() {
  // TODO(michaeln): move purging logic to dom_storage::DomStorageContext

  // It is only safe to purge the memory from the LocalStorage namespace,
  // because it is backed by disk and can be reloaded later.  If we purge a
  // SessionStorage namespace, its data will be gone forever, because it isn't
  // currently backed by disk.
}

#endif
