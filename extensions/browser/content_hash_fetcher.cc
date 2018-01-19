// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_hash_fetcher.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/task_scheduler/post_task.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/content_verifier/content_hash.h"
#include "extensions/browser/content_verifier_delegate.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/browser/verified_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"

namespace extensions {

namespace {

std::unique_ptr<ContentHash> ReadContentHashAfterVerifiedContentsWritten(
    const ContentHash::ExtensionKey& extension_key,
    bool force,
    const ContentHash::IsCancelledCallback& is_cancelled) {
  // This function is called after writing verified_contents.json, so we'd
  // expect a valid file at this point. Even in the case where the file turns
  // out to be invalid right after writing it (malicous behavior?) don't delete
  // it.
  int mode = 0;

  if (force)
    mode |= ContentHash::Mode::kForceRecreateExistingComputedHashesFile;

  return ContentHash::Create(extension_key, mode, is_cancelled);
}

}  // namespace

// This class takes care of doing the disk and network I/O work to ensure we
// have both verified_contents.json files from the webstore and
// computed_hashes.json files computed over the files in an extension's
// directory.
class ContentHashFetcherJob
    : public base::RefCountedThreadSafe<ContentHashFetcherJob>,
      public net::URLFetcherDelegate {
 public:
  using CompletionCallback =
      base::Callback<void(scoped_refptr<ContentHashFetcherJob>)>;
  ContentHashFetcherJob(net::URLRequestContextGetter* request_context,
                        const ContentHash::ExtensionKey& extension_key,
                        const GURL& fetch_url,
                        bool force,
                        const CompletionCallback& callback);

  void Start();

  // Cancels this job, which will attempt to stop I/O operations sooner than
  // just waiting for the entire job to complete. Safe to call from any thread.
  void Cancel();

  // Checks whether this job has been cancelled. Safe to call from any thread.
  bool IsCancelled();

  // Returns whether this job was successful (we have both verified contents
  // and computed hashes). Even if the job was a success, there might have been
  // files that were found to have contents not matching expectations; these
  // are available by calling hash_mismatch_unix_paths().
  bool success() { return success_; }

  bool force() const { return force_; }

  const std::string& extension_id() { return extension_key_.extension_id; }

  // Returns the set of paths (with unix style '/' separators) that had a hash
  // mismatch.
  const std::set<base::FilePath>& hash_mismatch_unix_paths() {
    return hash_mismatch_unix_paths_;
  }

 private:
  friend class base::RefCountedThreadSafe<ContentHashFetcherJob>;
  ~ContentHashFetcherJob() override;

  // Methods called when job completes.
  void Succeeded(const std::set<base::FilePath>& hash_mismatch_unix_paths);
  void Failed();

  // Returns content hashes after trying to load them.
  // Invalid verified_contents.json will be removed from disk.
  std::unique_ptr<ContentHash> LoadVerifiedContents();

  // Callback for when we're done doing file I/O to see if we already have
  // content hashes. If we don't have verified_contents.json, this will kick off
  // a network request to get one.
  void DoneCheckingForVerifiedContents(
      std::unique_ptr<ContentHash> content_hash);

  // URLFetcherDelegate interface
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // Callback for when we've read verified contents after fetching it from
  // network and writing it to disk.
  void ReadCompleteAfterWrite(std::unique_ptr<ContentHash> content_hash);

  // Callback for the job to write the verified contents to the filesystem.
  void OnVerifiedContentsWritten(size_t expected_size, int write_result);

  // Will call the callback, if we haven't been cancelled.
  void DispatchCallback();

  net::URLRequestContextGetter* request_context_;

  // Note: Read from multiple threads/sequences.
  const ContentHash::ExtensionKey extension_key_;

  // The url we'll need to use to fetch a verified_contents.json file.
  GURL fetch_url_;

  const bool force_;

  CompletionCallback callback_;
  content::BrowserThread::ID creation_thread_;

  // Used for fetching content signatures.
  // Created and destroyed on |creation_thread_|.
  std::unique_ptr<net::URLFetcher> url_fetcher_;

  // The parsed contents of the verified_contents.json file, either read from
  // disk or fetched from the network and then written to disk.
  std::unique_ptr<VerifiedContents> verified_contents_;

  // Whether this job succeeded.
  bool success_;

  // Paths that were found to have a mismatching hash.
  std::set<base::FilePath> hash_mismatch_unix_paths_;

  // Note: this may be accessed from multiple threads, so all access should
  // be protected by |cancelled_lock_|.
  bool cancelled_;

  // A lock for synchronizing access to |cancelled_|.
  base::Lock cancelled_lock_;

  DISALLOW_COPY_AND_ASSIGN(ContentHashFetcherJob);
};

ContentHashFetcherJob::ContentHashFetcherJob(
    net::URLRequestContextGetter* request_context,
    const ContentHash::ExtensionKey& extension_key,
    const GURL& fetch_url,
    bool force,
    const CompletionCallback& callback)
    : request_context_(request_context),
      extension_key_(extension_key),
      fetch_url_(fetch_url),
      force_(force),
      callback_(callback),
      success_(false),
      cancelled_(false) {
  bool got_id =
      content::BrowserThread::GetCurrentThreadIdentifier(&creation_thread_);
  DCHECK(got_id);
}

void ContentHashFetcherJob::Start() {
  base::PostTaskAndReplyWithResult(
      GetExtensionFileTaskRunner().get(), FROM_HERE,
      base::BindOnce(&ContentHashFetcherJob::LoadVerifiedContents, this),
      base::BindOnce(&ContentHashFetcherJob::DoneCheckingForVerifiedContents,
                     this));
}

void ContentHashFetcherJob::Cancel() {
  base::AutoLock autolock(cancelled_lock_);
  cancelled_ = true;
}

bool ContentHashFetcherJob::IsCancelled() {
  base::AutoLock autolock(cancelled_lock_);
  bool result = cancelled_;
  return result;
}

ContentHashFetcherJob::~ContentHashFetcherJob() {
  // Destroy |url_fetcher_| on correct thread.
  // It was possible for it to be deleted on blocking pool. See
  // https://crbug.com/702300 for details.
  // TODO(lazyboy): Make ContentHashFetcherJob non ref-counted.
  if (url_fetcher_ && !content::BrowserThread::CurrentlyOn(creation_thread_)) {
    content::BrowserThread::DeleteSoon(creation_thread_, FROM_HERE,
                                       url_fetcher_.release());
  }
}

std::unique_ptr<ContentHash> ContentHashFetcherJob::LoadVerifiedContents() {
  // Will delete invalid verified contents file.
  int mode = ContentHash::Mode::kDeleteInvalidVerifiedContents;
  if (force_)
    mode |= ContentHash::Mode::kForceRecreateExistingComputedHashesFile;

  return ContentHash::Create(
      extension_key_, mode,
      base::BindRepeating(&ContentHashFetcherJob::IsCancelled, this));
}

void ContentHashFetcherJob::DoneCheckingForVerifiedContents(
    std::unique_ptr<ContentHash> content_hash) {
  if (IsCancelled())
    return;
  if (content_hash->has_verified_contents()) {
    VLOG(1) << "Found verified contents for " << extension_key_.extension_id;
    if (content_hash->succeeded()) {
      // We've found both verified_contents.json and computed_hashes.json, we're
      // done.
      Succeeded(content_hash->hash_mismatch_unix_paths());
    } else {
      // Even though we attempted to write computed_hashes.json after reading
      // verified_contents.json, something went wrong.
      Failed();
    }
  } else {
    VLOG(1) << "Missing verified contents for " << extension_key_.extension_id
            << ", fetching...";
    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation("content_hash_verification_job", R"(
          semantics {
            sender: "Web Store Content Verification"
            description:
              "The request sent to retrieve the file required for content "
              "verification for an extension from the Web Store."
            trigger:
              "An extension from the Web Store is missing the "
              "verified_contents.json file required for extension content "
              "verification."
            data: "The extension id and extension version."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            setting:
              "This feature cannot be directly disabled; it is enabled if any "
              "extension from the webstore is installed in the browser."
            policy_exception_justification:
              "Not implemented, not required. If the user has extensions from "
              "the Web Store, this feature is required to ensure the "
              "extensions match what is distributed by the store."
          })");
    url_fetcher_ = net::URLFetcher::Create(fetch_url_, net::URLFetcher::GET,
                                           this, traffic_annotation);
    url_fetcher_->SetRequestContext(request_context_);
    url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                               net::LOAD_DO_NOT_SAVE_COOKIES |
                               net::LOAD_DISABLE_CACHE);
    url_fetcher_->SetAutomaticallyRetryOnNetworkChanges(3);
    url_fetcher_->Start();
  }
}

// Helper function to let us pass ownership of a string via base::Bind with the
// contents to be written into a file. Also ensures that the directory for
// |path| exists, creating it if needed.
static int WriteFileHelper(const base::FilePath& path,
                           std::unique_ptr<std::string> content) {
  base::FilePath dir = path.DirName();
  if (!base::CreateDirectoryAndGetError(dir, nullptr))
    return -1;
  return base::WriteFile(path, content->data(), content->size());
}

void ContentHashFetcherJob::OnURLFetchComplete(const net::URLFetcher* source) {
  VLOG(1) << "URLFetchComplete for " << extension_key_.extension_id
          << " is_success:" << url_fetcher_->GetStatus().is_success() << " "
          << fetch_url_.possibly_invalid_spec();
  // Delete |url_fetcher_| once we no longer need it.
  std::unique_ptr<net::URLFetcher> url_fetcher = std::move(url_fetcher_);

  if (IsCancelled())
    return;
  std::unique_ptr<std::string> response(new std::string);
  if (!url_fetcher->GetStatus().is_success() ||
      !url_fetcher->GetResponseAsString(response.get())) {
    Failed();
    return;
  }

  // Parse the response to make sure it is valid json (on staging sometimes it
  // can be a login redirect html, xml file, etc. if you aren't logged in with
  // the right cookies).  TODO(asargent) - It would be a nice enhancement to
  // move to parsing this in a sandboxed helper (crbug.com/372878).
  std::unique_ptr<base::Value> parsed(base::JSONReader::Read(*response));
  if (!parsed) {
    Failed();
    return;
  }

  VLOG(1) << "JSON parsed ok for " << extension_key_.extension_id;

  parsed.reset();  // no longer needed
  base::FilePath destination =
      file_util::GetVerifiedContentsPath(extension_key_.extension_root);
  size_t size = response->size();
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      // TODO(lazyboy): Consider creating VerifiedContents directly from
      // |response| instead of reading the file again.
      base::BindOnce(&WriteFileHelper, destination, base::Passed(&response)),
      base::BindOnce(&ContentHashFetcherJob::OnVerifiedContentsWritten, this,
                     size));
}

void ContentHashFetcherJob::OnVerifiedContentsWritten(size_t expected_size,
                                                      int write_result) {
  bool success =
      write_result >= 0 && static_cast<size_t>(write_result) == expected_size;

  if (!success) {
    Failed();
    return;
  }

  base::PostTaskAndReplyWithResult(
      GetExtensionFileTaskRunner().get(), FROM_HERE,
      // TODO(lazyboy): Consider creating VerifiedContents directly from
      // |response| instead of reading the file again.
      base::BindOnce(
          &ReadContentHashAfterVerifiedContentsWritten, extension_key_, force_,
          base::BindRepeating(&ContentHashFetcherJob::IsCancelled, this)),
      base::BindOnce(&ContentHashFetcherJob::ReadCompleteAfterWrite, this));
}

void ContentHashFetcherJob::ReadCompleteAfterWrite(
    std::unique_ptr<ContentHash> content_hash) {
  if (IsCancelled())
    return;

  if (content_hash->succeeded()) {
    Succeeded(content_hash->hash_mismatch_unix_paths());
  } else {
    Failed();
  }
}

void ContentHashFetcherJob::Failed() {
  success_ = false;
  DispatchCallback();
}

void ContentHashFetcherJob::Succeeded(
    const std::set<base::FilePath>& hash_mismatch_unix_paths) {
  success_ = true;

  // TODO(lazyboy): Avoid this copy.
  hash_mismatch_unix_paths_ = hash_mismatch_unix_paths;

  DispatchCallback();
}

void ContentHashFetcherJob::DispatchCallback() {
  {
    base::AutoLock autolock(cancelled_lock_);
    if (cancelled_)
      return;
  }
  callback_.Run(base::WrapRefCounted(this));
}

// ----

ContentHashFetcher::ContentHashFetcher(
    net::URLRequestContextGetter* context_getter,
    ContentVerifierDelegate* delegate,
    const FetchCallback& callback)
    : context_getter_(context_getter),
      delegate_(delegate),
      fetch_callback_(callback),
      weak_ptr_factory_(this) {}

ContentHashFetcher::~ContentHashFetcher() {
  for (JobMap::iterator i = jobs_.begin(); i != jobs_.end(); ++i) {
    i->second->Cancel();
  }
}

void ContentHashFetcher::DoFetch(const Extension* extension, bool force) {
  DCHECK(extension);

  IdAndVersion key(extension->id(), extension->version().GetString());
  JobMap::iterator found = jobs_.find(key);
  if (found != jobs_.end()) {
    if (!force || found->second->force()) {
      // Just let the existing job keep running.
      return;
    } else {
      // Kill the existing non-force job, so we can start a new one below.
      found->second->Cancel();
      jobs_.erase(found);
    }
  }

  // TODO(asargent) - we should do something here to remember recent attempts
  // to fetch signatures by extension id, and use exponential backoff to avoid
  // hammering the server when we aren't successful in getting them.
  // crbug.com/373397

  GURL url =
      delegate_->GetSignatureFetchUrl(extension->id(), extension->version());
  ContentHashFetcherJob* job = new ContentHashFetcherJob(
      context_getter_,
      ContentHash::ExtensionKey(extension->id(), extension->path(),
                                extension->version(),
                                delegate_->GetPublicKey()),
      url, force,
      // TODO(lazyboy): Use BindOnce, ContentHashFetcherJob should respond at
      // most once.
      base::BindRepeating(&ContentHashFetcher::JobFinished,
                          weak_ptr_factory_.GetWeakPtr()));
  jobs_.insert(std::make_pair(key, job));
  job->Start();
}

void ContentHashFetcher::ExtensionLoaded(const Extension* extension) {
  CHECK(extension);
  DoFetch(extension, false);
}

void ContentHashFetcher::ExtensionUnloaded(const Extension* extension) {
  CHECK(extension);
  IdAndVersion key(extension->id(), extension->version().GetString());
  JobMap::iterator found = jobs_.find(key);
  if (found != jobs_.end()) {
    found->second->Cancel();
    jobs_.erase(found);
  }
}

void ContentHashFetcher::JobFinished(scoped_refptr<ContentHashFetcherJob> job) {
  if (!job->IsCancelled()) {
    // Note: Run can result in ContentHashFetcher::ExtensionUnloaded.
    //
    // TODO(lazyboy): Add a unit test to cover the case where Run can result in
    // ContentHashFetcher::ExtensionUnloaded, once https://crbug.com/702300 is
    // fixed.
    fetch_callback_.Run(job->extension_id(), job->success(), job->force(),
                        job->hash_mismatch_unix_paths());
  }

  for (JobMap::iterator i = jobs_.begin(); i != jobs_.end(); ++i) {
    if (i->second.get() == job.get()) {
      jobs_.erase(i);
      break;
    }
  }
}

}  // namespace extensions
