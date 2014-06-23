// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_CONTENT_VERIFIER_H_
#define EXTENSIONS_BROWSER_CONTENT_VERIFIER_H_

#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "extensions/browser/content_verify_job.h"

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace extensions {

class Extension;
class ContentHashFetcher;
class ContentVerifierDelegate;

// Used for managing overall content verification - both fetching content
// hashes as needed, and supplying job objects to verify file contents as they
// are read.
class ContentVerifier : public base::RefCountedThreadSafe<ContentVerifier> {
 public:
  // Takes ownership of |delegate|.
  ContentVerifier(content::BrowserContext* context,
                  ContentVerifierDelegate* delegate);
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

  void OnFetchComplete(const std::string& extension_id,
                       bool success,
                       bool was_force_check,
                       const std::set<base::FilePath>& hash_mismatch_paths);

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentVerifier);

  friend class base::RefCountedThreadSafe<ContentVerifier>;
  virtual ~ContentVerifier();

  // Returns true if any of the paths in |relative_paths| *should* have their
  // contents verified. (Some files get transcoded during the install process,
  // so we don't want to verify their contents because they are expected not
  // to match).
  bool ShouldVerifyAnyPaths(const Extension* extension,
                            const std::set<base::FilePath>& relative_paths);

  content::BrowserContext* context_;

  scoped_ptr<ContentVerifierDelegate> delegate_;

  // For fetching content hash signatures.
  scoped_ptr<ContentHashFetcher> fetcher_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_CONTENT_VERIFIER_H_
