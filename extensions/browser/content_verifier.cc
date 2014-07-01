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
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_l10n_util.h"

namespace extensions {

ContentVerifier::ContentVerifier(content::BrowserContext* context,
                                 ContentVerifierDelegate* delegate)
    : context_(context),
      delegate_(delegate),
      fetcher_(new ContentHashFetcher(
          context,
          delegate,
          base::Bind(&ContentVerifier::OnFetchComplete, this))) {
}

ContentVerifier::~ContentVerifier() {
}

void ContentVerifier::Start() {
  fetcher_->Start();
}

void ContentVerifier::Shutdown() {
  fetcher_.reset();
  delegate_.reset();
}

ContentVerifyJob* ContentVerifier::CreateJobFor(
    const std::string& extension_id,
    const base::FilePath& extension_root,
    const base::FilePath& relative_path) {
  if (!fetcher_ || !delegate_)
    return NULL;

  ExtensionRegistry* registry = ExtensionRegistry::Get(context_);
  const Extension* extension =
      registry->GetExtensionById(extension_id, ExtensionRegistry::EVERYTHING);

  std::set<base::FilePath> paths;
  paths.insert(relative_path);
  if (!ShouldVerifyAnyPaths(extension, paths))
    return NULL;

  // TODO(asargent) - we can probably get some good performance wins by having
  // a cache of ContentHashReader's that we hold onto past the end of each job.
  return new ContentVerifyJob(
      new ContentHashReader(extension_id,
                            *extension->version(),
                            extension_root,
                            relative_path,
                            delegate_->PublicKey()),
      base::Bind(&ContentVerifier::VerifyFailed, this, extension->id()));
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

  VLOG(1) << "VerifyFailed " << extension_id << " reason:" << reason;

  ExtensionRegistry* registry = ExtensionRegistry::Get(context_);
  const Extension* extension =
      registry->GetExtensionById(extension_id, ExtensionRegistry::EVERYTHING);

  if (!delegate_ || !extension)
    return;

  ContentVerifierDelegate::Mode mode = delegate_->ShouldBeVerified(*extension);
  if (mode < ContentVerifierDelegate::ENFORCE)
    return;

  if (reason == ContentVerifyJob::MISSING_ALL_HASHES) {
    // If we failed because there were no hashes yet for this extension, just
    // request some.
    fetcher_->DoFetch(extension, true /* force */);
  } else {
    delegate_->VerifyFailed(extension_id);
  }
}

void ContentVerifier::OnFetchComplete(
    const std::string& extension_id,
    bool success,
    bool was_force_check,
    const std::set<base::FilePath>& hash_mismatch_paths) {
  VLOG(1) << "OnFetchComplete " << extension_id << " success:" << success;

  ExtensionRegistry* registry = ExtensionRegistry::Get(context_);
  const Extension* extension =
      registry->GetExtensionById(extension_id, ExtensionRegistry::EVERYTHING);
  if (!delegate_ || !extension)
    return;

  ContentVerifierDelegate::Mode mode = delegate_->ShouldBeVerified(*extension);
  if (mode < ContentVerifierDelegate::ENFORCE)
    return;

  if (!success && mode < ContentVerifierDelegate::ENFORCE_STRICT)
    return;

  if ((was_force_check && !success) ||
      ShouldVerifyAnyPaths(extension, hash_mismatch_paths))
    delegate_->VerifyFailed(extension_id);
}

bool ContentVerifier::ShouldVerifyAnyPaths(
    const Extension* extension,
    const std::set<base::FilePath>& relative_paths) {
  if (!delegate_ || !extension || !extension->version())
    return false;

  ContentVerifierDelegate::Mode mode = delegate_->ShouldBeVerified(*extension);
  if (mode < ContentVerifierDelegate::ENFORCE)
    return false;

  // Images used in the browser get transcoded during install, so skip
  // checking them for now.  TODO(asargent) - see if we can cache this list
  // for a given extension id/version pair.
  std::set<base::FilePath> browser_images =
      delegate_->GetBrowserImagePaths(extension);

  base::FilePath locales_dir = extension->path().Append(kLocaleFolder);
  scoped_ptr<std::set<std::string> > all_locales;

  for (std::set<base::FilePath>::const_iterator i = relative_paths.begin();
       i != relative_paths.end();
       ++i) {
    const base::FilePath& relative_path = *i;

    if (relative_path == base::FilePath(kManifestFilename))
      continue;

    if (ContainsKey(browser_images, relative_path))
      continue;

    base::FilePath full_path = extension->path().Append(relative_path);
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
