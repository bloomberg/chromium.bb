// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_verifier.h"

#include <algorithm>

#include "base/files/file_path.h"
#include "base/stl_util.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/content_hash_fetcher.h"
#include "extensions/browser/content_hash_reader.h"
#include "extensions/browser/content_verifier_delegate.h"
#include "extensions/browser/content_verifier_io_data.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_l10n_util.h"

namespace extensions {

ContentVerifier::ContentVerifier(content::BrowserContext* context,
                                 ContentVerifierDelegate* delegate)
    : shutdown_(false),
      context_(context),
      delegate_(delegate),
      fetcher_(new ContentHashFetcher(
          context,
          delegate,
          base::Bind(&ContentVerifier::OnFetchComplete, this))),
      observer_(this),
      io_data_(new ContentVerifierIOData) {
}

ContentVerifier::~ContentVerifier() {
}

void ContentVerifier::Start() {
  ExtensionRegistry* registry = ExtensionRegistry::Get(context_);
  observer_.Add(registry);
}

void ContentVerifier::Shutdown() {
  shutdown_ = true;
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&ContentVerifierIOData::Clear, io_data_));
  observer_.RemoveAll();
  fetcher_.reset();
}

ContentVerifyJob* ContentVerifier::CreateJobFor(
    const std::string& extension_id,
    const base::FilePath& extension_root,
    const base::FilePath& relative_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  const ContentVerifierIOData::ExtensionData* data =
      io_data_->GetData(extension_id);
  if (!data)
    return NULL;

  std::set<base::FilePath> paths;
  paths.insert(relative_path);
  if (!ShouldVerifyAnyPaths(extension_id, extension_root, paths))
    return NULL;

  // TODO(asargent) - we can probably get some good performance wins by having
  // a cache of ContentHashReader's that we hold onto past the end of each job.
  return new ContentVerifyJob(
      new ContentHashReader(extension_id,
                            data->version,
                            extension_root,
                            relative_path,
                            delegate_->PublicKey()),
      base::Bind(&ContentVerifier::VerifyFailed, this, extension_id));
}

void ContentVerifier::VerifyFailed(const std::string& extension_id,
                                   ContentVerifyJob::FailureReason reason) {
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI,
        FROM_HERE,
        base::Bind(&ContentVerifier::VerifyFailed, this, extension_id, reason));
    return;
  }
  if (shutdown_)
    return;

  VLOG(1) << "VerifyFailed " << extension_id << " reason:" << reason;

  ExtensionRegistry* registry = ExtensionRegistry::Get(context_);
  const Extension* extension =
      registry->GetExtensionById(extension_id, ExtensionRegistry::EVERYTHING);

  if (!extension)
    return;

  if (reason == ContentVerifyJob::MISSING_ALL_HASHES) {
    // If we failed because there were no hashes yet for this extension, just
    // request some.
    fetcher_->DoFetch(extension, true /* force */);
  } else {
    delegate_->VerifyFailed(extension_id);
  }
}

void ContentVerifier::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (shutdown_)
    return;

  ContentVerifierDelegate::Mode mode = delegate_->ShouldBeVerified(*extension);
  if (mode != ContentVerifierDelegate::NONE) {
    scoped_ptr<ContentVerifierIOData::ExtensionData> data(
        new ContentVerifierIOData::ExtensionData(
            delegate_->GetBrowserImagePaths(extension),
            extension->version() ? *extension->version() : base::Version()));
    content::BrowserThread::PostTask(content::BrowserThread::IO,
                                     FROM_HERE,
                                     base::Bind(&ContentVerifierIOData::AddData,
                                                io_data_,
                                                extension->id(),
                                                base::Passed(&data)));
    fetcher_->ExtensionLoaded(extension);
  }
}

void ContentVerifier::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  if (shutdown_)
    return;
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &ContentVerifierIOData::RemoveData, io_data_, extension->id()));
  if (fetcher_)
    fetcher_->ExtensionUnloaded(extension);
}

void ContentVerifier::OnFetchCompleteHelper(const std::string& extension_id,
                                            bool shouldVerifyAnyPathsResult) {
  if (shouldVerifyAnyPathsResult)
    delegate_->VerifyFailed(extension_id);
}

void ContentVerifier::OnFetchComplete(
    const std::string& extension_id,
    bool success,
    bool was_force_check,
    const std::set<base::FilePath>& hash_mismatch_paths) {
  if (shutdown_)
    return;

  VLOG(1) << "OnFetchComplete " << extension_id << " success:" << success;

  ExtensionRegistry* registry = ExtensionRegistry::Get(context_);
  const Extension* extension =
      registry->GetExtensionById(extension_id, ExtensionRegistry::EVERYTHING);
  if (!delegate_ || !extension)
    return;

  ContentVerifierDelegate::Mode mode = delegate_->ShouldBeVerified(*extension);
  if (was_force_check && !success &&
      mode == ContentVerifierDelegate::ENFORCE_STRICT) {
    // We weren't able to get verified_contents.json or weren't able to compute
    // hashes.
    delegate_->VerifyFailed(extension_id);
  } else {
    content::BrowserThread::PostTaskAndReplyWithResult(
        content::BrowserThread::IO,
        FROM_HERE,
        base::Bind(&ContentVerifier::ShouldVerifyAnyPaths,
                   this,
                   extension_id,
                   extension->path(),
                   hash_mismatch_paths),
        base::Bind(
            &ContentVerifier::OnFetchCompleteHelper, this, extension_id));
  }
}

bool ContentVerifier::ShouldVerifyAnyPaths(
    const std::string& extension_id,
    const base::FilePath& extension_root,
    const std::set<base::FilePath>& relative_paths) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  const ContentVerifierIOData::ExtensionData* data =
      io_data_->GetData(extension_id);
  if (!data)
    return false;

  const std::set<base::FilePath>& browser_images = data->browser_image_paths;

  base::FilePath locales_dir = extension_root.Append(kLocaleFolder);
  scoped_ptr<std::set<std::string> > all_locales;

  for (std::set<base::FilePath>::const_iterator i = relative_paths.begin();
       i != relative_paths.end();
       ++i) {
    const base::FilePath& relative_path = *i;

    if (relative_path == base::FilePath(kManifestFilename))
      continue;

    if (ContainsKey(browser_images, relative_path))
      continue;

    base::FilePath full_path = extension_root.Append(relative_path);
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
      // to skip those paths.
      if (full_path.DirName().DirName() == locales_dir &&
          !extension_l10n_util::ShouldSkipValidation(
              locales_dir, full_path.DirName(), *all_locales))
        continue;
    }
    return true;
  }
  return false;
}

}  // namespace extensions
