// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_verifier.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/metrics/field_trial.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/content_hash_fetcher.h"
#include "extensions/browser/content_hash_reader.h"
#include "extensions/browser/content_verifier_delegate.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/switches.h"

namespace {

const char kExperimentName[] = "ExtensionContentVerification";

}  // namespace

namespace extensions {

ContentVerifier::ContentVerifier(content::BrowserContext* context,
                                 ContentVerifierDelegate* delegate)
    : mode_(GetMode()),
      context_(context),
      delegate_(delegate),
      fetcher_(new ContentHashFetcher(context, delegate)) {
}

ContentVerifier::~ContentVerifier() {
}

void ContentVerifier::Start() {
  if (mode_ >= BOOTSTRAP)
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
  if (!delegate_)
    return NULL;

  ExtensionRegistry* registry = ExtensionRegistry::Get(context_);
  const Extension* extension =
      registry->GetExtensionById(extension_id, ExtensionRegistry::EVERYTHING);

  if (!extension || !delegate_->ShouldBeVerified(*extension) ||
      !extension->version())
    return NULL;

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

  if (!delegate_ || mode_ < ENFORCE)
    return;

  if (reason == ContentVerifyJob::NO_HASHES && mode_ < ENFORCE_STRICT &&
      fetcher_.get()) {
    // If we failed because there were no hashes yet for this extension, just
    // request some.
    ExtensionRegistry* registry = ExtensionRegistry::Get(context_);
    const Extension* extension =
        registry->GetExtensionById(extension_id, ExtensionRegistry::EVERYTHING);
    if (extension)
      fetcher_->DoFetch(extension);
    return;
  }
  delegate_->VerifyFailed(extension_id);
}

// static
ContentVerifier::Mode ContentVerifier::GetMode() {
  Mode experiment_value = NONE;
  const std::string group = base::FieldTrialList::FindFullName(kExperimentName);
  if (group == "EnforceStrict")
    experiment_value = ENFORCE_STRICT;
  else if (group == "Enforce")
    experiment_value = ENFORCE;
  else if (group == "Bootstrap")
    experiment_value = BOOTSTRAP;

  Mode cmdline_value = NONE;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kExtensionContentVerification)) {
    std::string switch_value = command_line->GetSwitchValueASCII(
        switches::kExtensionContentVerification);
    if (switch_value == switches::kExtensionContentVerificationBootstrap)
      cmdline_value = BOOTSTRAP;
    else if (switch_value == switches::kExtensionContentVerificationEnforce)
      cmdline_value = ENFORCE;
    else if (switch_value ==
             switches::kExtensionContentVerificationEnforceStrict)
      cmdline_value = ENFORCE_STRICT;
    else
      // If no value was provided (or the wrong one), just default to enforce.
      cmdline_value = ENFORCE;
  }

  // We don't want to allow the command-line flags to eg disable enforcement if
  // the experiment group says it should be on, or malware may just modify the
  // command line flags. So return the more restrictive of the 2 values.
  return std::max(experiment_value, cmdline_value);
}

}  // namespace extensions
