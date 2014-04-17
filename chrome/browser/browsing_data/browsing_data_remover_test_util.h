// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_TEST_UTIL_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_TEST_UTIL_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "content/public/test/test_utils.h"

class BrowsingDataRemoverCompletionObserver
    : public BrowsingDataRemover::Observer {
 public:
  explicit BrowsingDataRemoverCompletionObserver(BrowsingDataRemover* remover);
  virtual ~BrowsingDataRemoverCompletionObserver();

  void BlockUntilCompletion();

 protected:
  // BrowsingDataRemover::Observer:
  virtual void OnBrowsingDataRemoverDone() OVERRIDE;

 private:
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataRemoverCompletionObserver);
};

class BrowsingDataRemoverCompletionInhibitor
    : public BrowsingDataRemover::CompletionInhibitor {
 public:
  BrowsingDataRemoverCompletionInhibitor();
  virtual ~BrowsingDataRemoverCompletionInhibitor();

  void BlockUntilNearCompletion();
  void ContinueToCompletion();

 protected:
  // BrowsingDataRemover::CompletionInhibitor:
  virtual void OnBrowsingDataRemoverWouldComplete(
      BrowsingDataRemover* remover,
      const base::Closure& continue_to_completion) OVERRIDE;

 private:
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  base::Closure continue_to_completion_callback_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataRemoverCompletionInhibitor);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_TEST_UTIL_H_
