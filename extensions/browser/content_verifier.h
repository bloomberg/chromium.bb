// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_CONTENT_VERIFIER_H_
#define EXTENSIONS_BROWSER_CONTENT_VERIFIER_H_

#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_observer.h"
#include "base/version.h"
#include "extensions/browser/content_verifier_delegate.h"
#include "extensions/browser/content_verify_job.h"
#include "extensions/browser/extension_registry_observer.h"

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace extensions {

class Extension;
class ContentHashFetcher;
class ContentVerifierIOData;
class ManagementPolicy;

// Used for managing overall content verification - both fetching content
// hashes as needed, and supplying job objects to verify file contents as they
// are read.
class ContentVerifier : public base::RefCountedThreadSafe<ContentVerifier>,
                        public ExtensionRegistryObserver {
 public:
  class TestObserver {
   public:
    virtual void OnFetchComplete(const std::string& extension_id,
                                 bool success) = 0;
  };
  // Returns true if content verifier should repair the extension (|id|) if it
  // became courrpted.
  // Note that this method doesn't check whether |id| is corrupted or not.
  static bool ShouldRepairIfCorrupted(const ManagementPolicy* management_policy,
                                      const Extension* id);

  static void SetObserverForTests(TestObserver* observer);

  ContentVerifier(content::BrowserContext* context,
                  std::unique_ptr<ContentVerifierDelegate> delegate);
  void Start();
  void Shutdown();

  // Call this before reading a file within an extension. The caller owns the
  // returned job.
  ContentVerifyJob* CreateJobFor(const std::string& extension_id,
                                 const base::FilePath& extension_root,
                                 const base::FilePath& relative_path);

  // Called (typically by a verification job) to indicate that verification
  // failed while reading some file in |extension_id|.
  void VerifyFailed(const std::string& extension_id,
                    ContentVerifyJob::FailureReason reason);

  // ExtensionRegistryObserver interface
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentVerifier);

  friend class base::RefCountedThreadSafe<ContentVerifier>;
  ~ContentVerifier() override;

  void OnFetchComplete(
      const std::string& extension_id,
      bool success,
      bool was_force_check,
      const std::set<base::FilePath>& hash_mismatch_unix_paths);

  void OnFetchCompleteHelper(const std::string& extension_id,
                             bool should_verify_any_paths_result);

  // Returns true if any of the paths in |relative_unix_paths| *should* have
  // their contents verified. (Some files get transcoded during the install
  // process, so we don't want to verify their contents because they are
  // expected not to match).
  bool ShouldVerifyAnyPaths(
      const std::string& extension_id,
      const base::FilePath& extension_root,
      const std::set<base::FilePath>& relative_unix_paths);

  // Set to true once we've begun shutting down.
  bool shutdown_;

  content::BrowserContext* context_;

  std::unique_ptr<ContentVerifierDelegate> delegate_;

  // For fetching content hash signatures.
  std::unique_ptr<ContentHashFetcher> fetcher_;

  // For observing the ExtensionRegistry.
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver> observer_;

  // Data that should only be used on the IO thread.
  scoped_refptr<ContentVerifierIOData> io_data_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_CONTENT_VERIFIER_H_
