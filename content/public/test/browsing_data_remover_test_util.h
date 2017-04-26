// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_BROWSING_DATA_REMOVER_TEST_UTIL_H_
#define CONTENT_PUBLIC_TEST_BROWSING_DATA_REMOVER_TEST_UTIL_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_observer.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/test/test_utils.h"

namespace content {

// This class can be used to wait for a BrowsingDataRemover to complete
// operation. It is not suitable for repeated use.
class BrowsingDataRemoverCompletionObserver
    : public BrowsingDataRemover::Observer {
 public:
  explicit BrowsingDataRemoverCompletionObserver(BrowsingDataRemover* remover);
  ~BrowsingDataRemoverCompletionObserver() override;

  void BlockUntilCompletion();

 protected:
  // BrowsingDataRemover::Observer:
  void OnBrowsingDataRemoverDone() override;

 private:
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
  ScopedObserver<BrowsingDataRemover, BrowsingDataRemover::Observer> observer_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataRemoverCompletionObserver);
};

// The completion inhibitor can artificially delay completion of the browsing
// data removal process. It is used during testing to simulate scenarios in
// which the deletion stalls or takes a very long time.
//
// This class will detach itself from |remover| upon its destruction.
// If |remover| is destroyed during a test (e.g. in profile shutdown tests),
// users must call Reset() to detach it in advance.
class BrowsingDataRemoverCompletionInhibitor {
 public:
  explicit BrowsingDataRemoverCompletionInhibitor(BrowsingDataRemover* remover);
  virtual ~BrowsingDataRemoverCompletionInhibitor();

  void Reset();

  void BlockUntilNearCompletion();
  void ContinueToCompletion();

 protected:
  virtual void OnBrowsingDataRemoverWouldComplete(
      const base::Closure& continue_to_completion);

 private:
  // Not owned by this class. If the pointer becomes invalid, the owner of
  // this class is responsible for calling Reset().
  BrowsingDataRemover* remover_;

  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  base::Closure continue_to_completion_callback_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataRemoverCompletionInhibitor);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_BROWSING_DATA_REMOVER_TEST_UTIL_H_
