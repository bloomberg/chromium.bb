// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/uma_histogram_helper.h"
#include "chrome/test/nacl/nacl_browsertest_util.h"
#include "native_client/src/trusted/service_runtime/nacl_error_code.h"
#include "ppapi/native_client/src/trusted/plugin/plugin_error.h"

namespace {

// This test fails on Linux ASAN bots: <http://crbug.com/161709>.
#if defined(OS_LINUX) && defined(ADDRESS_SANITIZER)
#define MAYBE_SuccessfulLoadUMA DISABLED_SuccessfulLoadUMA
#else
#define MAYBE_SuccessfulLoadUMA SuccessfulLoadUMA
#endif

NACL_BROWSER_TEST_F(NaClBrowserTest, MAYBE_SuccessfulLoadUMA, {
  // Load a NaCl module to generate UMA data.
  RunLoadTest(FILE_PATH_LITERAL("nacl_load_test.html"));

  // Make sure histograms from child processes have been accumulated in the
  // browser brocess.
  UMAHistogramHelper histograms;
  histograms.Fetch();

  // Did the plugin report success?
  histograms.ExpectUniqueSample("NaCl.LoadStatus.Plugin",
                                plugin::ERROR_LOAD_SUCCESS, 1);

  // Did the sel_ldr report success?
  histograms.ExpectUniqueSample("NaCl.LoadStatus.SelLdr",
                                LOAD_OK, 1);

  // Make sure we have other important histograms.
  if (!IsPnacl()) {
    histograms.ExpectTotalCount("NaCl.Perf.StartupTime.LoadModule", 1);
    histograms.ExpectTotalCount("NaCl.Perf.StartupTime.Total", 1);
    histograms.ExpectTotalCount("NaCl.Perf.Size.Manifest", 1);
    histograms.ExpectTotalCount("NaCl.Perf.Size.Nexe", 1);
  } else {
    histograms.ExpectTotalCount("NaCl.Perf.Size.Manifest", 1);
    histograms.ExpectTotalCount("NaCl.Perf.Size.Pexe", 1);
    histograms.ExpectTotalCount("NaCl.Perf.Size.PNaClTranslatedNexe", 1);
    histograms.ExpectTotalCount("NaCl.Perf.Size.PexeNexeSizePct", 1);
    histograms.ExpectTotalCount("NaCl.Perf.PNaClLoadTime.LoadCompiler", 1);
    histograms.ExpectTotalCount("NaCl.Perf.PNaClLoadTime.LoadLinker", 1);
    histograms.ExpectTotalCount("NaCl.Perf.PNaClLoadTime.CompileTime", 1);
    histograms.ExpectTotalCount("NaCl.Perf.PNaClLoadTime.CompileKBPerSec", 1);
    histograms.ExpectTotalCount("NaCl.Perf.PNaClLoadTime.LinkTime", 1);
    histograms.ExpectTotalCount("NaCl.Perf.PNaClLoadTime.TotalUncachedTime", 1);
    histograms.ExpectTotalCount(
        "NaCl.Perf.PNaClLoadTime.TotalUncachedKBPerSec", 1);
    histograms.ExpectTotalCount("NaCl.Perf.PNaClCache.IsHit", 1);
  }
})

// TODO(ncbray) convert the rest of nacl_uma.py (currently in the NaCl repo.)
// Test validation failures and crashes.

}  // namespace anonymous
