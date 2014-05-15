// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_hash_fetcher.h"

#include <algorithm>

#include "base/base64.h"
#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/stl_util.h"
#include "base/synchronization/lock.h"
#include "base/task_runner_util.h"
#include "base/version.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "extensions/browser/computed_hashes.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"

namespace {

typedef std::set<base::FilePath> SortedFilePathSet;

}  // namespace

namespace extensions {

// This class takes care of doing the disk and network I/O work to ensure we
// have both verified_contents.json files from the webstore and
// computed_hashes.json files computed over the files in an extension's
// directory.
class ContentHashFetcherJob
    : public base::RefCountedThreadSafe<ContentHashFetcherJob>,
      public net::URLFetcherDelegate {
 public:
  typedef base::Callback<void(ContentHashFetcherJob*)> CompletionCallback;
  ContentHashFetcherJob(net::URLRequestContextGetter* request_context,
                        const std::string& extension_id,
                        const base::FilePath& extension_path,
                        const GURL& fetch_url,
                        const CompletionCallback& callback);

  void Start();

  // Cancels this job, which will attempt to stop I/O operations sooner than
  // just waiting for the entire job to complete. Safe to call from any thread.
  void Cancel();

  // Returns whether this job was completely successful (we have both verified
  // contents and computed hashes).
  bool success() { return success_; }

  // Do we have a verified_contents.json file?
  bool have_verified_contents() { return have_verified_contents_; }

 private:
  friend class base::RefCountedThreadSafe<ContentHashFetcherJob>;
  virtual ~ContentHashFetcherJob();

  // Checks whether this job has been cancelled. Safe to call from any thread.
  bool IsCancelled();

  // Callback for when we're done doing file I/O to see if we already have
  // a verified contents file. If we don't, this will kick off a network
  // request to get one.
  void DoneCheckingForVerifiedContents(bool found);

  // URLFetcherDelegate interface
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // Callback for when we're done ensuring we have verified contents, and are
  // ready to move on to MaybeCreateHashes.
  void DoneFetchingVerifiedContents(bool success);

  // Callback for the job to write the verified contents to the filesystem.
  void OnVerifiedContentsWritten(size_t expected_size, int write_result);

  // The verified contents file from the webstore only contains the treehash
  // root hash, but for performance we want to cache the individual block level
  // hashes. This function will create that cache with block-level hashes for
  // each file in the extension if needed (the treehash root hash for each of
  // these should equal what is in the verified contents file from the
  // webstore).
  void MaybeCreateHashes();

  // Computes hashes for all files in |extension_path_|, and uses a
  // ComputedHashes::Writer to write that information into
  // |hashes_file|. Returns true on success.
  bool CreateHashes(const base::FilePath& hashes_file);

  // Will call the callback, if we haven't been cancelled.
  void DispatchCallback();

  net::URLRequestContextGetter* request_context_;
  std::string extension_id_;
  base::FilePath extension_path_;

  // The url we'll need to use to fetch a verified_contents.json file.
  GURL fetch_url_;

  CompletionCallback callback_;
  content::BrowserThread::ID creation_thread_;

  // Used for fetching content signatures.
  scoped_ptr<net::URLFetcher> url_fetcher_;

  // Whether this job succeeded.
  bool success_;

  // Whether we either found a verified contents file, or were successful in
  // fetching one and saving it to disk.
  bool have_verified_contents_;

  // The block size to use for hashing.
  int block_size_;

  // Note: this may be accessed from multiple threads, so all access should
  // be protected by |cancelled_lock_|.
  bool cancelled_;

  // A lock for synchronizing access to |cancelled_|.
  base::Lock cancelled_lock_;
};

ContentHashFetcherJob::ContentHashFetcherJob(
    net::URLRequestContextGetter* request_context,
    const std::string& extension_id,
    const base::FilePath& extension_path,
    const GURL& fetch_url,
    const CompletionCallback& callback)
    : request_context_(request_context),
      extension_id_(extension_id),
      extension_path_(extension_path),
      fetch_url_(fetch_url),
      callback_(callback),
      success_(false),
      have_verified_contents_(false),
      // TODO(asargent) - use the value from verified_contents.json for each
      // file, instead of using a constant.
      block_size_(4096),
      cancelled_(false) {
  DCHECK(content::BrowserThread::GetCurrentThreadIdentifier(&creation_thread_));
}

void ContentHashFetcherJob::Start() {
  base::FilePath verified_contents_path =
      file_util::GetVerifiedContentsPath(extension_path_);
  base::PostTaskAndReplyWithResult(
      content::BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&base::PathExists, verified_contents_path),
      base::Bind(&ContentHashFetcherJob::DoneCheckingForVerifiedContents,
                 this));
}

void ContentHashFetcherJob::Cancel() {
  base::AutoLock autolock(cancelled_lock_);
  cancelled_ = true;
}

ContentHashFetcherJob::~ContentHashFetcherJob() {
}

bool ContentHashFetcherJob::IsCancelled() {
  base::AutoLock autolock(cancelled_lock_);
  bool result = cancelled_;
  return result;
}

void ContentHashFetcherJob::DoneCheckingForVerifiedContents(bool found) {
  if (IsCancelled())
    return;
  if (found) {
    DoneFetchingVerifiedContents(true);
  } else {
    url_fetcher_.reset(
        net::URLFetcher::Create(fetch_url_, net::URLFetcher::GET, this));
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
                           scoped_ptr<std::string> content) {
  base::FilePath dir = path.DirName();
  return (base::CreateDirectoryAndGetError(dir, NULL) &&
          base::WriteFile(path, content->data(), content->size()));
}

void ContentHashFetcherJob::OnURLFetchComplete(const net::URLFetcher* source) {
  if (IsCancelled())
    return;
  scoped_ptr<std::string> response(new std::string);
  if (!url_fetcher_->GetStatus().is_success() ||
      !url_fetcher_->GetResponseAsString(response.get())) {
    DoneFetchingVerifiedContents(false);
    return;
  }

  // Parse the response to make sure it is valid json (on staging sometimes it
  // can be a login redirect html, xml file, etc. if you aren't logged in with
  // the right cookies).  TODO(asargent) - It would be a nice enhancement to
  // move to parsing this in a sandboxed helper (crbug.com/372878).
  scoped_ptr<base::Value> parsed(base::JSONReader::Read(*response));
  if (parsed) {
    parsed.reset();  // no longer needed
    base::FilePath destination =
        file_util::GetVerifiedContentsPath(extension_path_);
    size_t size = response->size();
    base::PostTaskAndReplyWithResult(
        content::BrowserThread::GetBlockingPool(),
        FROM_HERE,
        base::Bind(&WriteFileHelper, destination, base::Passed(&response)),
        base::Bind(
            &ContentHashFetcherJob::OnVerifiedContentsWritten, this, size));
  } else {
    DoneFetchingVerifiedContents(false);
  }
}

void ContentHashFetcherJob::OnVerifiedContentsWritten(size_t expected_size,
                                                      int write_result) {
  bool success =
      (write_result >= 0 && static_cast<size_t>(write_result) == expected_size);
  DoneFetchingVerifiedContents(success);
}

void ContentHashFetcherJob::DoneFetchingVerifiedContents(bool success) {
  have_verified_contents_ = success;

  if (IsCancelled())
    return;

  // TODO(asargent) - eventually we should abort here on !success, but for
  // testing purposes it's actually still helpful to continue on to create the
  // computed hashes.

  content::BrowserThread::PostBlockingPoolSequencedTask(
      "ContentHashFetcher",
      FROM_HERE,
      base::Bind(&ContentHashFetcherJob::MaybeCreateHashes, this));
}

void ContentHashFetcherJob::MaybeCreateHashes() {
  if (IsCancelled())
    return;
  base::FilePath hashes_file =
      file_util::GetComputedHashesPath(extension_path_);

  if (base::PathExists(hashes_file))
    success_ = true;
  else
    success_ = CreateHashes(hashes_file);

  content::BrowserThread::PostTask(
      creation_thread_,
      FROM_HERE,
      base::Bind(&ContentHashFetcherJob::DispatchCallback, this));
}

bool ContentHashFetcherJob::CreateHashes(const base::FilePath& hashes_file) {
  if (IsCancelled())
    return false;
  // Make sure the directory exists.
  if (!base::CreateDirectoryAndGetError(hashes_file.DirName(), NULL))
    return false;

  base::FileEnumerator enumerator(extension_path_,
                                  true, /* recursive */
                                  base::FileEnumerator::FILES);
  // First discover all the file paths and put them in a sorted set.
  SortedFilePathSet paths;
  for (;;) {
    if (IsCancelled())
      return false;

    base::FilePath full_path = enumerator.Next();
    if (full_path.empty())
      break;
    paths.insert(full_path);
  }

  // Now iterate over all the paths in sorted order and compute the block hashes
  // for each one.
  ComputedHashes::Writer writer;
  for (SortedFilePathSet::iterator i = paths.begin(); i != paths.end(); ++i) {
    if (IsCancelled())
      return false;
    const base::FilePath& full_path = *i;
    base::FilePath relative_path;
    extension_path_.AppendRelativePath(full_path, &relative_path);
    std::string contents;
    if (!base::ReadFileToString(full_path, &contents)) {
      LOG(ERROR) << "Could not read " << full_path.MaybeAsASCII();
      continue;
    }

    // Iterate through taking the hash of each block of size (block_size_) of
    // the file.
    std::vector<std::string> hashes;
    size_t offset = 0;
    while (offset < contents.size()) {
      if (IsCancelled())
        return false;
      const char* block_start = contents.data() + offset;
      size_t bytes_to_read =
          std::min(contents.size() - offset, static_cast<size_t>(block_size_));
      DCHECK(bytes_to_read > 0);
      scoped_ptr<crypto::SecureHash> hash(
          crypto::SecureHash::Create(crypto::SecureHash::SHA256));
      hash->Update(block_start, bytes_to_read);

      hashes.push_back(std::string());
      std::string* buffer = &hashes.back();
      buffer->resize(crypto::kSHA256Length);
      hash->Finish(string_as_array(buffer), buffer->size());

      // Get ready for next iteration.
      offset += bytes_to_read;
    }
    writer.AddHashes(relative_path, block_size_, hashes);
  }
  return writer.WriteToFile(hashes_file);
}

void ContentHashFetcherJob::DispatchCallback() {
  {
    base::AutoLock autolock(cancelled_lock_);
    if (cancelled_)
      return;
  }
  callback_.Run(this);
}

// ----

ContentHashFetcher::ContentHashFetcher(content::BrowserContext* context,
                                       ContentVerifierDelegate* delegate)
    : context_(context),
      delegate_(delegate),
      observer_(this),
      weak_ptr_factory_(this) {
}

ContentHashFetcher::~ContentHashFetcher() {
  for (JobMap::iterator i = jobs_.begin(); i != jobs_.end(); ++i) {
    i->second->Cancel();
  }
}

void ContentHashFetcher::Start() {
  ExtensionRegistry* registry = ExtensionRegistry::Get(context_);
  observer_.Add(registry);
}

void ContentHashFetcher::DoFetch(const Extension* extension) {
  if (!extension || !delegate_->ShouldBeVerified(*extension))
    return;

  IdAndVersion key(extension->id(), extension->version()->GetString());
  if (ContainsKey(jobs_, key))
    return;

  // TODO(asargent) - we should do something here to remember recent attempts
  // to fetch signatures by extension id, and use exponential backoff to avoid
  // hammering the server when we aren't successful in getting them.
  // crbug.com/373397

  DCHECK(extension->version());
  GURL url =
      delegate_->GetSignatureFetchUrl(extension->id(), *extension->version());
  ContentHashFetcherJob* job =
      new ContentHashFetcherJob(context_->GetRequestContext(),
                                extension->id(),
                                extension->path(),
                                url,
                                base::Bind(&ContentHashFetcher::JobFinished,
                                           weak_ptr_factory_.GetWeakPtr()));
  jobs_.insert(std::make_pair(key, job));
  job->Start();
}

void ContentHashFetcher::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  CHECK(extension);
  DoFetch(extension);
}

void ContentHashFetcher::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  CHECK(extension);
  IdAndVersion key(extension->id(), extension->version()->GetString());
  JobMap::iterator found = jobs_.find(key);
  if (found != jobs_.end())
    jobs_.erase(found);
}

void ContentHashFetcher::JobFinished(ContentHashFetcherJob* job) {
  for (JobMap::iterator i = jobs_.begin(); i != jobs_.end(); ++i) {
    if (i->second.get() == job) {
      jobs_.erase(i);
      break;
    }
  }
}

}  // namespace extensions
