// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_verifier.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/content_hash_fetcher.h"
#include "extensions/browser/content_hash_reader.h"
#include "extensions/browser/content_verifier_delegate.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/content_scripts_handler.h"

namespace extensions {

namespace {

ContentVerifier::TestObserver* g_content_verifier_test_observer = NULL;

// This function converts paths like "//foo/bar", "./foo/bar", and
// "/foo/bar" to "foo/bar". It also converts path separators to "/".
base::FilePath NormalizeRelativePath(const base::FilePath& path) {
  if (path.ReferencesParent())
    return base::FilePath();

  std::vector<base::FilePath::StringType> parts;
  path.GetComponents(&parts);
  if (parts.empty())
    return base::FilePath();

  // Remove the first component if it is '.' or '/' or '//'.
  const base::FilePath::StringType separators(
      base::FilePath::kSeparators, base::FilePath::kSeparatorsLength);
  if (!parts[0].empty() &&
      (parts[0] == base::FilePath::kCurrentDirectory ||
       parts[0].find_first_not_of(separators) == std::string::npos))
    parts.erase(parts.begin());

  // Note that elsewhere we always normalize path separators to '/' so this
  // should work for all platforms.
  return base::FilePath(
      base::JoinString(parts, base::FilePath::StringType(1, '/')));
}

bool HasScriptFileExt(const base::FilePath& requested_path) {
  return requested_path.Extension() == FILE_PATH_LITERAL(".js");
}

bool HasPageFileExt(const base::FilePath& requested_path) {
  base::FilePath::StringType file_extension = requested_path.Extension();
  return file_extension == FILE_PATH_LITERAL(".html") ||
         file_extension == FILE_PATH_LITERAL(".htm");
}

std::unique_ptr<ContentVerifierIOData::ExtensionData> CreateIOData(
    const Extension* extension,
    ContentVerifierDelegate* delegate) {
  // The browser image paths from the extension may not be relative (eg
  // they might have leading '/' or './'), so we strip those to make
  // comparing to actual relative paths work later on.
  std::set<base::FilePath> original_image_paths =
      delegate->GetBrowserImagePaths(extension);

  auto image_paths = std::make_unique<std::set<base::FilePath>>();
  for (const auto& path : original_image_paths) {
    image_paths->insert(NormalizeRelativePath(path));
  }

  auto background_or_content_paths =
      std::make_unique<std::set<base::FilePath>>();
  for (const std::string& script :
       BackgroundInfo::GetBackgroundScripts(extension)) {
    background_or_content_paths->insert(
        extension->GetResource(script).relative_path());
  }
  if (BackgroundInfo::HasBackgroundPage(extension)) {
    background_or_content_paths->insert(
        extensions::file_util::ExtensionURLToRelativeFilePath(
            BackgroundInfo::GetBackgroundURL(extension)));
  }
  for (const std::unique_ptr<UserScript>& script :
       ContentScriptsInfo::GetContentScripts(extension)) {
    for (const std::unique_ptr<UserScript::File>& js_file :
         script->js_scripts()) {
      background_or_content_paths->insert(js_file->relative_path());
    }
  }

  return std::make_unique<ContentVerifierIOData::ExtensionData>(
      std::move(image_paths), std::move(background_or_content_paths),
      extension->version());
}

}  // namespace

// A class to retrieve ContentHash for ContentVerifier.
//
// All public calls originate and terminate on IO, making it suitable for
// ContentVerifier to cache ContentHash instances easily.
// TODO(lazyboy): Implement caching.
//
// This class makes sure we do not have more than one ContentHash request in
// flight for a particular version of an extension. If a call to retrieve an
// extensions's ContentHash is made while another retieval for the same
// version of the extension is in flight, this class will queue up the
// callback(s) and respond to all of them when ContentHash is available.
class ContentVerifier::HashHelper {
 public:
  explicit HashHelper(ContentVerifier* content_verifier)
      : content_verifier_(content_verifier), weak_factory_(this) {}
  ~HashHelper() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    // TODO(lazyboy): Do we need to Cancel() the callacks?
  }

  // Cancels any ongoing computed_hashes.json disk write for an extension.
  void Cancel(const ExtensionId& extension_id,
              const base::Version& extension_version) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    auto callback_key = std::make_pair(extension_id, extension_version);
    auto iter = callback_infos_.find(callback_key);
    if (iter == callback_infos_.end())
      return;
    iter->second.Cancel();
    callback_infos_.erase(iter);
  }

  // Retrieves the ContentHash of an extension and responds via |callback|.
  //
  // Must be called on IO thread. The method responds through |callback| on IO
  // thread.
  void GetContentHash(const ContentHash::ExtensionKey& extension_key,
                      const ContentHash::FetchParams& fetch_params,
                      bool force_missing_computed_hashes_creation,
                      ContentHashCallback callback) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    auto callback_key = std::make_pair(extension_key.extension_id,
                                       extension_key.extension_version);
    auto iter = callback_infos_.find(callback_key);
    if (iter != callback_infos_.end()) {
      iter->second.callbacks.push_back(std::move(callback));
      iter->second.force_missing_computed_hashes_creation |=
          force_missing_computed_hashes_creation;
      return;
    }
    scoped_refptr<IsCancelledChecker> checker =
        base::MakeRefCounted<IsCancelledChecker>();
    auto iter_pair = callback_infos_.emplace(
        callback_key, CallbackInfo(checker, std::move(callback)));
    DCHECK(iter_pair.second);
    iter_pair.first->second.force_missing_computed_hashes_creation |=
        force_missing_computed_hashes_creation;

    GetExtensionFileTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &HashHelper::ReadHashOnFileTaskRunner, extension_key, fetch_params,
            base::BindRepeating(&IsCancelledChecker::IsCancelled, checker),
            base::BindOnce(&HashHelper::DidReadHash, weak_factory_.GetWeakPtr(),
                           callback_key, checker)));
  }

 private:
  using CallbackKey = std::pair<ExtensionId, base::Version>;

  class IsCancelledChecker
      : public base::RefCountedThreadSafe<IsCancelledChecker> {
   public:
    IsCancelledChecker() {}

    // Safe to call from any thread.
    void Cancel() {
      base::AutoLock autolock(cancelled_lock_);
      cancelled_ = true;
    }
    // Safe to call from any thread.
    bool IsCancelled() {
      base::AutoLock autolock(cancelled_lock_);
      return cancelled_;
    }

   private:
    friend class base::RefCountedThreadSafe<IsCancelledChecker>;

    ~IsCancelledChecker() {}

    // Note: this may be accessed from multiple threads, so all access should
    // be protected by |cancelled_lock_|.
    bool cancelled_ = false;

    // A lock for synchronizing access to |cancelled_|.
    base::Lock cancelled_lock_;

    DISALLOW_COPY_AND_ASSIGN(IsCancelledChecker);
  };

  // Holds information about each call to HashHelper::GetContentHash(), for a
  // particular extension (id and version).
  //
  // |callbacks| are the callbacks that callers to GetContentHash() passed us.
  // |cancelled_checker| is used to cancel an extension's task from any thread.
  // |force_missing_computed_hashes_creation| is true if any callback (from
  //     ContentVerifyJob) requested to recompute computed_hashes.json file in
  //     case the file is missing or cannot be read.
  struct CallbackInfo {
    CallbackInfo(const scoped_refptr<IsCancelledChecker>& cancelled_checker,
                 ContentHashCallback callback)
        : cancelled_checker(cancelled_checker) {
      callbacks.push_back(std::move(callback));
    }

    void Cancel() { cancelled_checker->Cancel(); }

    scoped_refptr<IsCancelledChecker> cancelled_checker;
    // TODO(lazyboy): Use std::list?
    std::vector<ContentHashCallback> callbacks;
    bool force_missing_computed_hashes_creation = false;
  };

  using IsCancelledCallback = base::RepeatingCallback<bool(void)>;

  static void ForwardToIO(ContentHash::CreatedCallback callback,
                          const scoped_refptr<ContentHash>& content_hash,
                          bool was_cancelled) {
    // If the request was cancelled, then we don't have a corresponding entry
    // for the request in |callback_infos_| anymore.
    if (was_cancelled)
      return;

    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(std::move(callback), content_hash, was_cancelled));
  }

  static void ReadHashOnFileTaskRunner(
      const ContentHash::ExtensionKey& extension_key,
      const ContentHash::FetchParams& fetch_params,
      const IsCancelledCallback& is_cancelled,
      ContentHash::CreatedCallback created_callback) {
    ContentHash::Create(
        extension_key, fetch_params, is_cancelled,
        base::BindOnce(&HashHelper::ForwardToIO, std::move(created_callback)));
  }

  static void ForceBuildComputedHashesOnFileTaskRuner(
      const scoped_refptr<ContentHash> content_hash,
      const IsCancelledCallback& is_cancelled,
      ContentHash::CreatedCallback created_callback) {
    content_hash->ForceBuildComputedHashes(
        is_cancelled,
        base::BindOnce(&HashHelper::ForwardToIO, std::move(created_callback)));
  }

  void DidReadHash(const CallbackKey& key,
                   const scoped_refptr<IsCancelledChecker>& checker,
                   const scoped_refptr<ContentHash>& content_hash,
                   bool was_cancelled) {
    DCHECK(checker);
    if (was_cancelled ||
        // The request might have been cancelled on IO after |content_hash| was
        // built.
        // TODO(lazyboy): Add a specific test case for this. See
        // https://crbug.com/825470 for a likely example of this.
        checker->IsCancelled()) {
      return;
    }

    auto iter = callback_infos_.find(key);
    DCHECK(iter != callback_infos_.end());
    auto& callback_info = iter->second;

    // Force creation of computed_hashes.json if all of the following are true:
    //   - any caller(s) has explicitly requested it.
    //   - hash retrieval failed due to invalid computed_hashes.json and
    //     re-creating the file might make the hash retrieval successful.
    if (callback_info.force_missing_computed_hashes_creation &&
        content_hash->might_require_computed_hashes_force_creation()) {
      GetExtensionFileTaskRunner()->PostTask(
          FROM_HERE,
          base::BindOnce(&HashHelper::ForceBuildComputedHashesOnFileTaskRuner,
                         content_hash,
                         base::BindRepeating(&IsCancelledChecker::IsCancelled,
                                             callback_info.cancelled_checker),
                         base::BindOnce(&HashHelper::CompleteDidReadHash,
                                        weak_factory_.GetWeakPtr(), key,
                                        callback_info.cancelled_checker)));
      return;
    }

    CompleteDidReadHash(key, callback_info.cancelled_checker,
                        std::move(content_hash), was_cancelled);
  }

  void CompleteDidReadHash(const CallbackKey& key,
                           const scoped_refptr<IsCancelledChecker>& checker,
                           const scoped_refptr<ContentHash>& content_hash,
                           bool was_cancelled) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    DCHECK(checker);
    if (was_cancelled ||
        // The request might have been cancelled on IO after |content_hash| was
        // built.
        checker->IsCancelled()) {
      return;
    }

    auto iter = callback_infos_.find(key);
    DCHECK(iter != callback_infos_.end());
    auto& callback_info = iter->second;

    for (auto& callback : callback_info.callbacks)
      std::move(callback).Run(content_hash);
    callback_infos_.erase(iter);

    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

    // OnFetchComplete will check content_hash->hash_mismatch_unix_paths():
    content_verifier_->OnFetchComplete(content_hash);
  }

  // List of pending callbacks of GetContentHash().
  std::map<CallbackKey, CallbackInfo> callback_infos_;

  ContentVerifier* const content_verifier_ = nullptr;

  base::WeakPtrFactory<HashHelper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(HashHelper);
};

// static
bool ContentVerifier::ShouldRepairIfCorrupted(
    const ManagementPolicy* management_policy,
    const Extension* extension) {
  return management_policy->MustRemainEnabled(extension, nullptr) ||
         management_policy->MustRemainInstalled(extension, nullptr);
}

// static
void ContentVerifier::SetObserverForTests(TestObserver* observer) {
  g_content_verifier_test_observer = observer;
}

ContentVerifier::ContentVerifier(
    content::BrowserContext* context,
    std::unique_ptr<ContentVerifierDelegate> delegate)
    : context_(context),
      delegate_(std::move(delegate)),
      request_context_getter_(
          content::BrowserContext::GetDefaultStoragePartition(context)
              ->GetURLRequestContext()),
      observer_(this),
      io_data_(new ContentVerifierIOData) {}

ContentVerifier::~ContentVerifier() {
}

void ContentVerifier::Start() {
  ExtensionRegistry* registry = ExtensionRegistry::Get(context_);
  observer_.Add(registry);
}

void ContentVerifier::Shutdown() {
  shutdown_on_ui_ = true;
  delegate_->Shutdown();
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&ContentVerifier::ShutdownOnIO, this));
  observer_.RemoveAll();
}

void ContentVerifier::ShutdownOnIO() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  shutdown_on_io_ = true;
  io_data_->Clear();
  hash_helper_.reset();
}

ContentVerifyJob* ContentVerifier::CreateJobFor(
    const std::string& extension_id,
    const base::FilePath& extension_root,
    const base::FilePath& relative_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  const ContentVerifierIOData::ExtensionData* data =
      io_data_->GetData(extension_id);
  // The absence of |data| generally means that we don't have to verify the
  // extension resource. However, it could also mean that
  // OnExtensionLoadedOnIO didn't get a chance to fire yet.
  // See https://crbug.com/826584 for an example of how this can happen from
  // ExtensionUserScriptLoader. Currently, ExtensionUserScriptLoader performs a
  // thread hopping to work around this problem.
  // TODO(lazyboy): Prefer queueing up jobs in these case instead of the thread
  // hopping solution, but that requires a substantial change in
  // ContnetVerifier/ContentVerifyJob.
  if (!data)
    return NULL;

  base::FilePath normalized_unix_path = NormalizeRelativePath(relative_path);

  std::set<base::FilePath> unix_paths;
  unix_paths.insert(normalized_unix_path);
  if (!ShouldVerifyAnyPaths(extension_id, extension_root, unix_paths))
    return NULL;

  // TODO(asargent) - we can probably get some good performance wins by having
  // a cache of ContentHashReader's that we hold onto past the end of each job.
  return new ContentVerifyJob(
      extension_id, data->version, extension_root, normalized_unix_path,
      base::BindOnce(&ContentVerifier::VerifyFailed, this, extension_id));
}

void ContentVerifier::GetContentHash(
    const ExtensionId& extension_id,
    const base::FilePath& extension_root,
    const base::Version& extension_version,
    bool force_missing_computed_hashes_creation,
    ContentHashCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (shutdown_on_io_) {
    // NOTE: Release |callback| asynchronously, so that we don't release ref of
    // ContentVerifyJob and possibly destroy it synchronously here while
    // ContentVerifyJob is holding a lock. The lock destroyer would fail DCHECK
    // in that case.
    // TODO(lazyboy): Make CreateJobFor return a scoped_refptr instead of raw
    // pointer to fix this. Also add unit test to exercise this code path
    // explicitly.
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(base::DoNothing::Once<ContentHashCallback>(),
                       std::move(callback)));
    return;
  }

  ContentHash::ExtensionKey extension_key(extension_id, extension_root,
                                          extension_version,
                                          delegate_->GetPublicKey());
  ContentHash::FetchParams fetch_params =
      GetFetchParams(extension_id, extension_version);
  // Since |shutdown_on_io_| = false, GetOrCreateHashHelper() must return
  // non-nullptr instance of HashHelper.
  GetOrCreateHashHelper()->GetContentHash(
      extension_key, fetch_params, force_missing_computed_hashes_creation,
      std::move(callback));
}

void ContentVerifier::VerifyFailed(const ExtensionId& extension_id,
                                   ContentVerifyJob::FailureReason reason) {
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::BindOnce(&ContentVerifier::VerifyFailed, this, extension_id,
                       reason));
    return;
  }
  if (shutdown_on_ui_)
    return;

  VLOG(1) << "VerifyFailed " << extension_id << " reason:" << reason;
  DCHECK_NE(ContentVerifyJob::NONE, reason);

  ExtensionRegistry* registry = ExtensionRegistry::Get(context_);
  const Extension* extension =
      registry->GetExtensionById(extension_id, ExtensionRegistry::EVERYTHING);

  if (!extension)
    return;

  ContentVerifierDelegate::Mode mode = delegate_->ShouldBeVerified(*extension);
  // If the failure was due to hashes missing, only "enforce_strict" would
  // disable the extension, but not "enforce".
  if (reason == ContentVerifyJob::MISSING_ALL_HASHES &&
      mode != ContentVerifierDelegate::ENFORCE_STRICT) {
    return;
  }

  delegate_->VerifyFailed(extension_id, reason);
}

void ContentVerifier::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (shutdown_on_ui_)
    return;

  ContentVerifierDelegate::Mode mode = delegate_->ShouldBeVerified(*extension);
  if (mode != ContentVerifierDelegate::NONE) {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(&ContentVerifier::OnExtensionLoadedOnIO, this,
                       extension->id(), extension->path(), extension->version(),
                       CreateIOData(extension, delegate_.get())));
  }
}

void ContentVerifier::OnExtensionLoadedOnIO(
    const ExtensionId& extension_id,
    const base::FilePath& extension_root,
    const base::Version& extension_version,
    std::unique_ptr<ContentVerifierIOData::ExtensionData> data) {
  if (shutdown_on_io_)
    return;

  io_data_->AddData(extension_id, std::move(data));
  GetContentHash(extension_id, extension_root, extension_version,
                 false /* force_missing_computed_hashes_creation */,
                 // HashHelper will respond directly to OnFetchComplete().
                 base::DoNothing());
}

void ContentVerifier::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  if (shutdown_on_ui_)
    return;
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&ContentVerifier::OnExtensionUnloadedOnIO, this,
                     extension->id(), extension->version()));
}

GURL ContentVerifier::GetSignatureFetchUrlForTest(
    const ExtensionId& extension_id,
    const base::Version& extension_version) {
  return delegate_->GetSignatureFetchUrl(extension_id, extension_version);
}

void ContentVerifier::OnExtensionUnloadedOnIO(
    const ExtensionId& extension_id,
    const base::Version& extension_version) {
  if (shutdown_on_io_)
    return;
  io_data_->RemoveData(extension_id);
  HashHelper* hash_helper = GetOrCreateHashHelper();
  if (hash_helper)
    hash_helper->Cancel(extension_id, extension_version);
}

void ContentVerifier::OnFetchComplete(
    const scoped_refptr<const ContentHash>& content_hash) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  ExtensionId extension_id = content_hash->extension_key().extension_id;
  if (g_content_verifier_test_observer) {
    g_content_verifier_test_observer->OnFetchComplete(
        extension_id, content_hash->has_verified_contents());
  }

  VLOG(1) << "OnFetchComplete " << extension_id
          << " success:" << content_hash->succeeded();

  const bool did_hash_mismatch = ShouldVerifyAnyPaths(
      extension_id, content_hash->extension_key().extension_root,
      content_hash->hash_mismatch_unix_paths());
  if (!did_hash_mismatch)
    return;

  VerifyFailed(extension_id, ContentVerifyJob::HASH_MISMATCH);
}

ContentHash::FetchParams ContentVerifier::GetFetchParams(
    const ExtensionId& extension_id,
    const base::Version& extension_version) {
  return ContentHash::FetchParams(
      request_context_getter_,
      delegate_->GetSignatureFetchUrl(extension_id, extension_version));
}

bool ContentVerifier::ShouldVerifyAnyPaths(
    const std::string& extension_id,
    const base::FilePath& extension_root,
    const std::set<base::FilePath>& relative_unix_paths) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  const ContentVerifierIOData::ExtensionData* data =
      io_data_->GetData(extension_id);
  if (!data)
    return false;

  const std::set<base::FilePath>& browser_images = *(data->browser_image_paths);
  const std::set<base::FilePath>& background_or_content_paths =
      *(data->background_or_content_paths);

  base::FilePath locales_dir = extension_root.Append(kLocaleFolder);
  std::unique_ptr<std::set<std::string>> all_locales;

  const base::FilePath manifest_file(kManifestFilename);
  const base::FilePath messages_file(kMessagesFilename);
  for (const base::FilePath& relative_unix_path : relative_unix_paths) {
    if (relative_unix_path.empty())
      continue;

    if (relative_unix_path == manifest_file)
      continue;

    // JavaScript and HTML files should always be verified.
    if (HasScriptFileExt(relative_unix_path) ||
        HasPageFileExt(relative_unix_path)) {
      return true;
    }

    // Background pages, scripts and content scripts should always be verified
    // regardless of their file type.
    if (base::ContainsKey(background_or_content_paths, relative_unix_path))
      return true;

    if (base::ContainsKey(browser_images, relative_unix_path))
      continue;

    base::FilePath full_path =
        extension_root.Append(relative_unix_path.NormalizePathSeparators());

    if (full_path == file_util::GetIndexedRulesetPath(extension_root))
      continue;

    if (locales_dir.IsParent(full_path)) {
      if (!all_locales) {
        // TODO(asargent) - see if we can cache this list longer to avoid
        // having to fetch it more than once for a given run of the
        // browser. Maybe it can never change at runtime? (Or if it can, maybe
        // there is an event we can listen for to know to drop our cache).
        all_locales.reset(new std::set<std::string>);
        extension_l10n_util::GetAllLocales(all_locales.get());
      }

      // Since message catalogs get transcoded during installation, we want
      // to skip those paths. See if this path looks like
      // _locales/<some locale>/messages.json - if so then skip it.
      if (full_path.BaseName() == messages_file &&
          full_path.DirName().DirName() == locales_dir &&
          base::ContainsKey(*all_locales,
                            full_path.DirName().BaseName().MaybeAsASCII())) {
        continue;
      }
    }
    return true;
  }
  return false;
}

ContentVerifier::HashHelper* ContentVerifier::GetOrCreateHashHelper() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!shutdown_on_io_) << "Creating HashHelper after IO shutdown";
  // Just checking |hash_helper_| against nullptr isn't enough because we reset
  // hash_helper_ in Shutdown(), and we shouldn't be re-creating it in that
  // case.
  if (!hash_helper_created_) {
    DCHECK(!hash_helper_);
    hash_helper_ =
        std::unique_ptr<HashHelper, content::BrowserThread::DeleteOnIOThread>(
            new HashHelper(this));
    hash_helper_created_ = true;
  }
  return hash_helper_.get();
}

void ContentVerifier::ResetIODataForTesting(const Extension* extension) {
  io_data_->AddData(extension->id(), CreateIOData(extension, delegate_.get()));
}

}  // namespace extensions
