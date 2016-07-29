// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/nqe/ui_network_quality_estimator_service_test_util.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "content/public/browser/browser_thread.h"
#include "net/nqe/network_quality_estimator.h"

namespace nqe_test_util {

namespace {

// Reports |type| to all of NetworkQualityEstimator's
// EffectiveConnectionTypeObservers.
void OverrideEffectiveConnectionTypeOnIO(net::EffectiveConnectionType type,
                                         IOThread* io_thread) {
  if (!io_thread->globals()->network_quality_estimator)
    return;
  net::NetworkQualityEstimator* network_quality_estimator =
      io_thread->globals()->network_quality_estimator.get();
  if (!network_quality_estimator)
    return;
  network_quality_estimator->ReportEffectiveConnectionTypeForTesting(type);
}

}  // namespace

void OverrideEffectiveConnectionTypeAndWait(net::EffectiveConnectionType type) {
  // Block |run_loop| until OverrideEffectiveConnectionTypeOnIO has completed.
  // Any UI tasks posted by calling OverrideEffectiveConnectionTypeOnIO will
  // complete before the reply unblocks |run_loop|.
  base::RunLoop run_loop;
  content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&OverrideEffectiveConnectionTypeOnIO, type,
                 g_browser_process->io_thread()),
      run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace nqe_test_util
