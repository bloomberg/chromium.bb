// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_VERIFIED_RULESET_DEALER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_VERIFIED_RULESET_DEALER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "components/subresource_filter/content/common/ruleset_dealer.h"

namespace base {
class File;
}  // namespace base

namespace subresource_filter {

class MemoryMappedRuleset;

// The integrity verification status of a given ruleset version.
//
// A ruleset file starts from the NOT_VERIFIED state, after which it can be
// classified as INTACT or CORRUPT upon integrity verification.
enum class RulesetVerificationStatus {
  NOT_VERIFIED,
  INTACT,
  CORRUPT,
};

// This class is the same as RulesetDealer, but additionally does a one-time
// integrity checking on the ruleset before handing it out from GetRuleset().
//
// The |status| of verification is persisted throughout the entire lifetime of
// |this| object, and is reset to NOT_VERIFIED only when a new ruleset is
// supplied to SetRulesetFile() method.
class VerifiedRulesetDealer : public RulesetDealer {
 public:
  class Handle;

  VerifiedRulesetDealer();
  ~VerifiedRulesetDealer() override;

  // RulesetDealer:
  void SetRulesetFile(base::File ruleset_file) override;
  scoped_refptr<const MemoryMappedRuleset> GetRuleset() override;

  // For tests only.
  RulesetVerificationStatus status() const { return status_; }

 private:
  RulesetVerificationStatus status_ = RulesetVerificationStatus::NOT_VERIFIED;

  DISALLOW_COPY_AND_ASSIGN(VerifiedRulesetDealer);
};

// The UI-thread handle that owns a VerifiedRulesetDealer living on a dedicated
// sequenced |task_runner|. Provides asynchronous access to the instance, and
// destroys it asynchronously.
class VerifiedRulesetDealer::Handle {
 public:
  // Creates a VerifiedRulesetDealer that is owned by this handle, accessed
  // through this handle, but lives on |task_runner|.
  explicit Handle(scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~Handle();

  // Returns the |task_runner| on which the VerifiedRulesetDealer, as well as
  // the MemoryMappedRulesets it creates, should be accessed.
  base::SequencedTaskRunner* task_runner() { return task_runner_; }

  // Invokes |callback| on |task_runner|, providing a pointer to the underlying
  // VerifiedRulesetDealer as an argument. The pointer is guaranteed to be valid
  // at least until the callback returns.
  void GetDealerAsync(base::Callback<void(VerifiedRulesetDealer*)> callback);

  // Sets the ruleset |file| that the VerifiedRulesetDealer should distribute
  // from now on.
  void SetRulesetFile(base::File file);

 private:
  // Note: Raw pointer, |dealer_| already holds a reference to |task_runner_|.
  base::SequencedTaskRunner* task_runner_;
  std::unique_ptr<VerifiedRulesetDealer, base::OnTaskRunnerDeleter> dealer_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(Handle);
};

// Holds a strong reference to MemoryMappedRuleset, and provides acceess to it.
//
// Initially holds an empty reference, allowing this object to be created on the
// UI-thread synchronously, hence letting the Handle to be created synchronously
// and be immediately used by clients on the UI-thread, while the
// MemoryMappedRuleset is retrieved on the |task_runner| in a deferred manner.
class VerifiedRuleset {
 public:
  class Handle;

  VerifiedRuleset();
  ~VerifiedRuleset();

  // Can return nullptr even after initialization in case no ruleset is
  // available, or if the ruleset is corrupted.
  const MemoryMappedRuleset* Get() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return ruleset_.get();
  }

 private:
  // Initializes |ruleset_| with the one provided by the ruleset |dealer|.
  void Initialize(VerifiedRulesetDealer* dealer);

  scoped_refptr<const MemoryMappedRuleset> ruleset_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(VerifiedRuleset);
};

// The UI-thread handle that owns a VerifiedRuleset living on a dedicated
// sequenced |task_runner|, same as the VerifiedRulesetDealer lives on. Provides
// asynchronous access to the instance, and destroys it asynchronously.
class VerifiedRuleset::Handle {
 public:
  // Creates a VerifiedRuleset and initializes it asynchronously on a
  // |task_runner| using |dealer_handle|. The instance remains owned by this
  // handle, but living and accessed on the |task_runner|.
  explicit Handle(VerifiedRulesetDealer::Handle* dealer_handle);
  ~Handle();

  // Returns the |task_runner| on which the VerifiedRuleset, as well as the
  // MemoryMappedRuleset it holds a reference to, should be accessed.
  base::SequencedTaskRunner* task_runner() { return task_runner_; }

  // Invokes |callback| on |task_runner|, providing a pointer to the underlying
  // VerifiedRuleset as an argument. The pointer is guaranteed to be valid at
  // least until the callback returns.
  void GetRulesetAsync(base::Callback<void(VerifiedRuleset*)> callback);

 private:
  // This is to allow ADSF to post |ruleset_.get()| pointer to |task_runner_|.
  friend class AsyncDocumentSubresourceFilter;

  // Note: Raw pointer, |ruleset_| already holds a reference to |task_runner_|.
  base::SequencedTaskRunner* task_runner_;
  std::unique_ptr<VerifiedRuleset, base::OnTaskRunnerDeleter> ruleset_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(Handle);
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_VERIFIED_RULESET_DEALER_H_
