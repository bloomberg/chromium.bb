// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_CONTENT_VERIFIER_H_
#define EXTENSIONS_BROWSER_CONTENT_VERIFIER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list_threadsafe.h"
#include "extensions/browser/content_verifier_filter.h"
#include "extensions/browser/content_verify_job.h"

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace extensions {

// Interface for clients of ContentVerifier.
class ContentVerifierObserver {
 public:
  // Called when the content verifier detects that a read of a file inside
  // an extension did not match its expected hash.
  virtual void ContentVerifyFailed(const std::string& extension_id) = 0;
};

// Used for managing overall content verification - both fetching content
// hashes as needed, and supplying job objects to verify file contents as they
// are read.
class ContentVerifier : public base::RefCountedThreadSafe<ContentVerifier> {
 public:
  ContentVerifier(content::BrowserContext* context,
                  const ContentVerifierFilter& filter);
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

  // Observers will be called back on the same thread that they call
  // AddObserver on.
  void AddObserver(ContentVerifierObserver* observer);
  void RemoveObserver(ContentVerifierObserver* observer);

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentVerifier);

  friend class base::RefCountedThreadSafe<ContentVerifier>;
  virtual ~ContentVerifier();

  // Attempts to fetch content hashes for |extension_id|.
  void RequestFetch(const std::string& extension_id);

  // Note that it is important for these to appear in increasing "severity"
  // order, because we use this to let command line flags increase, but not
  // decrease, the mode you're running in compared to the experiment group.
  enum Mode {
    // Do not try to fetch content hashes if they are missing, and do not
    // enforce them if they are present.
    NONE = 0,

    // If content hashes are missing, try to fetch them, but do not enforce.
    BOOTSTRAP,

    // If hashes are present, enforce them. If they are missing, try to fetch
    // them.
    ENFORCE,

    // Treat the absence of hashes the same as a verification failure.
    ENFORCE_STRICT
  };

  static Mode GetMode();

  // The mode we're running in - set once at creation.
  const Mode mode_;

  // The filter we use to decide whether to return a ContentVerifyJob.
  ContentVerifierFilter filter_;

  // The associated BrowserContext.
  content::BrowserContext* context_;

  // The set of objects interested in verification failures.
  scoped_refptr<ObserverListThreadSafe<ContentVerifierObserver> > observers_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_CONTENT_VERIFIER_H_
