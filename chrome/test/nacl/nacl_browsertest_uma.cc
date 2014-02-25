// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/uma_histogram_helper.h"
#include "chrome/test/nacl/nacl_browsertest_util.h"
#include "native_client/src/trusted/service_runtime/nacl_error_code.h"
#include "ppapi/c/private/ppb_nacl_private.h"

namespace {

NACL_BROWSER_TEST_F(NaClBrowserTest, SuccessfulLoadUMA, {
  // Load a NaCl module to generate UMA data.
  RunLoadTest(FILE_PATH_LITERAL("nacl_load_test.html"));

  // Make sure histograms from child processes have been accumulated in the
  // browser brocess.
  UMAHistogramHelper histograms;
  histograms.Fetch();

  // Did the plugin report success?
  histograms.ExpectUniqueSample("NaCl.LoadStatus.Plugin",
                                PP_NACL_ERROR_LOAD_SUCCESS, 1);

  // Did the sel_ldr report success?
  histograms.ExpectUniqueSample("NaCl.LoadStatus.SelLdr",
                                LOAD_OK, 1);

  // Make sure we have other important histograms.
  if (!IsAPnaclTest()) {
    histograms.ExpectTotalCount("NaCl.Perf.StartupTime.LoadModule", 1);
    histograms.ExpectTotalCount("NaCl.Perf.StartupTime.Total", 1);
    histograms.ExpectTotalCount("NaCl.Perf.Size.Manifest", 1);
    histograms.ExpectTotalCount("NaCl.Perf.Size.Nexe", 1);
  } else {
    histograms.ExpectTotalCount("NaCl.Options.PNaCl.OptLevel", 1);
    histograms.ExpectTotalCount("NaCl.Perf.Size.Manifest", 1);
    histograms.ExpectTotalCount("NaCl.Perf.Size.Pexe", 1);
    histograms.ExpectTotalCount("NaCl.Perf.Size.PNaClTranslatedNexe", 1);
    histograms.ExpectTotalCount("NaCl.Perf.Size.PexeNexeSizePct", 1);
    histograms.ExpectTotalCount("NaCl.Perf.PNaClLoadTime.LoadCompiler", 1);
    histograms.ExpectTotalCount("NaCl.Perf.PNaClLoadTime.LoadLinker", 1);
    histograms.ExpectTotalCount("NaCl.Perf.PNaClLoadTime.CompileTime", 1);
    histograms.ExpectTotalCount("NaCl.Perf.PNaClLoadTime.CompileKBPerSec", 1);
    histograms.ExpectTotalCount("NaCl.Perf.PNaClLoadTime.LinkTime", 1);
    histograms.ExpectTotalCount(
        "NaCl.Perf.PNaClLoadTime.PctCompiledWhenFullyDownloaded", 1);
    histograms.ExpectTotalCount("NaCl.Perf.PNaClLoadTime.TotalUncachedTime", 1);
    histograms.ExpectTotalCount(
        "NaCl.Perf.PNaClLoadTime.TotalUncachedKBPerSec", 1);
    histograms.ExpectTotalCount("NaCl.Perf.PNaClCache.IsHit", 1);
  }
})

// TODO(ncbray) convert the rest of nacl_uma.py (currently in the NaCl repo.)
// Test validation failures and crashes.

}  // namespace
