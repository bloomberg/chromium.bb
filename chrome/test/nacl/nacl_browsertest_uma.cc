// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/uma_histogram_helper.h"
#include "chrome/test/nacl/nacl_browsertest_util.h"
#include "components/nacl/browser/nacl_browser.h"
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

  // Check validation cache usage:
  if (IsAPnaclTest()) {
    // Should have received 3 validation queries:
    // - One for IRT for actual application
    // - Two for two translator nexes
    // The translators don't currently use the IRT, so there is no IRT cache
    // query for those two loads. The PNaCl main nexe comes from a
    // delete-on-close temp file, so it doesn't have a stable identity
    // for validation caching. All three query results should be misses.
    histograms.ExpectUniqueSample("NaCl.ValidationCache.Query",
                                  nacl::NaClBrowser::CACHE_MISS, 3);
    // Should have received a cache setting afterwards for IRT and translators.
    histograms.ExpectUniqueSample("NaCl.ValidationCache.Set",
                                  nacl::NaClBrowser::CACHE_HIT, 3);
  } else {
    // For the open-web, only the IRT is considered a "safe" and
    // identity-cachable file. The nexes and .so files are not.
    // Should have one cache query for the IRT.
    histograms.ExpectUniqueSample("NaCl.ValidationCache.Query",
                                  nacl::NaClBrowser::CACHE_MISS, 1);
    // Should have received a cache setting afterwards for IRT and translators.
    histograms.ExpectUniqueSample("NaCl.ValidationCache.Set",
                                  nacl::NaClBrowser::CACHE_HIT, 1);
  }

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

class NaClBrowserTestNewlibVcacheExtension:
      public NaClBrowserTestNewlibExtension {
 public:
  virtual base::FilePath::StringType Variant() OVERRIDE {
    return FILE_PATH_LITERAL("extension_vcache_test/newlib");
  }
};

IN_PROC_BROWSER_TEST_F(NaClBrowserTestNewlibVcacheExtension,
                       ValidationCacheOfMainNexe) {
  // Hardcoded extension AppID that corresponds to the hardcoded
  // public key in the manifest.json file. We need to load the extension
  // nexe from the same origin, so we can't just try to load the extension
  // nexe as a mime-type handler from a non-extension URL.
  base::FilePath::StringType full_url =
      FILE_PATH_LITERAL("chrome-extension://cbcdidchbppangcjoddlpdjlenngjldk/")
      FILE_PATH_LITERAL("extension_validation_cache.html");
  RunNaClIntegrationTest(full_url, true);

  // Make sure histograms from child processes have been accumulated in the
  // browser process.
  UMAHistogramHelper histograms;
  histograms.Fetch();
  // Should have received 2 validation queries (one for IRT and one for NEXE),
  // and responded with a miss.
  histograms.ExpectBucketCount("NaCl.ValidationCache.Query",
                               nacl::NaClBrowser::CACHE_MISS, 2);
  // TOTAL should then be 2 queries so far.
  histograms.ExpectTotalCount("NaCl.ValidationCache.Query", 2);
  // Should have received a cache setting afterwards for IRT and nexe.
  histograms.ExpectBucketCount("NaCl.ValidationCache.Set",
                               nacl::NaClBrowser::CACHE_HIT, 2);

  // Load it again to hit the cache.
  RunNaClIntegrationTest(full_url, true);
  histograms.Fetch();
  // Should have received 2 more validation queries later (IRT and NEXE),
  // and responded with a hit.
  histograms.ExpectBucketCount("NaCl.ValidationCache.Query",
                               nacl::NaClBrowser::CACHE_HIT, 2);
  // TOTAL should then be 4 queries now.
  histograms.ExpectTotalCount("NaCl.ValidationCache.Query", 4);
  // Still only 2 settings.
  histograms.ExpectTotalCount("NaCl.ValidationCache.Set", 2);
}

class NaClBrowserTestGLibcVcacheExtension:
      public NaClBrowserTestGLibcExtension {
 public:
  virtual base::FilePath::StringType Variant() OVERRIDE {
    return FILE_PATH_LITERAL("extension_vcache_test/glibc");
  }
};

IN_PROC_BROWSER_TEST_F(NaClBrowserTestGLibcVcacheExtension,
                       ValidationCacheOfMainNexe) {
  // Hardcoded extension AppID that corresponds to the hardcoded
  // public key in the manifest.json file. We need to load the extension
  // nexe from the same origin, so we can't just try to load the extension
  // nexe as a mime-type handler from a non-extension URL.
  base::FilePath::StringType full_url =
      FILE_PATH_LITERAL("chrome-extension://cbcdidchbppangcjoddlpdjlenngjldk/")
      FILE_PATH_LITERAL("extension_validation_cache.html");
  RunNaClIntegrationTest(full_url, true);

  // Make sure histograms from child processes have been accumulated in the
  // browser process.
  UMAHistogramHelper histograms;
  histograms.Fetch();
  // Should have received 9 validation queries, which respond with misses:
  //   - the IRT
  //   - ld.so (the initial nexe)
  //   - main.nexe
  //   - libppapi_cpp.so
  //   - libpthread.so.9b15f6a6
  //   - libstdc++.so.6
  //   - libgcc_s.so.1
  //   - libc.so.9b15f6a6
  //   - libm.so.9b15f6a6
  histograms.ExpectBucketCount("NaCl.ValidationCache.Query",
                               nacl::NaClBrowser::CACHE_MISS, 9);
  // TOTAL should then be 9 queries so far.
  histograms.ExpectTotalCount("NaCl.ValidationCache.Query", 9);
  // Should have received a cache setting afterwards for IRT and nexe.
  histograms.ExpectBucketCount("NaCl.ValidationCache.Set",
                               nacl::NaClBrowser::CACHE_HIT, 9);

  // Load it again to hit the cache.
  RunNaClIntegrationTest(full_url, true);
  histograms.Fetch();
  // Should have received 9 more validation queries and responded with hits.
  histograms.ExpectBucketCount("NaCl.ValidationCache.Query",
                               nacl::NaClBrowser::CACHE_HIT, 9);
  histograms.ExpectTotalCount("NaCl.ValidationCache.Query", 18);
  histograms.ExpectTotalCount("NaCl.ValidationCache.Set", 9);
}

// Test that validation for the 2 PNaCl translator nexes can be cached.
IN_PROC_BROWSER_TEST_F(NaClBrowserTestPnacl,
                       ValidationCacheOfTranslatorNexes) {
  // Run a load test w/ one pexe cache identity.
  RunLoadTest(FILE_PATH_LITERAL("pnacl_options.html?use_nmf=o_0"));

  UMAHistogramHelper histograms;
  histograms.Fetch();
  // Should have received 3 validation queries:
  // - One for IRT for actual application
  // - Two for two translator nexes
  // The translators don't currently use the IRT, so there is no IRT cache
  // query for those two loads. The PNaCl main nexe comes from a
  // delete-on-close temp file, so it doesn't have a stable identity
  // for validation caching. All three query results should be misses.
  histograms.ExpectUniqueSample("NaCl.ValidationCache.Query",
                                nacl::NaClBrowser::CACHE_MISS, 3);
  // Should have received a cache setting afterwards for IRT and translators.
  histograms.ExpectUniqueSample("NaCl.ValidationCache.Set",
                               nacl::NaClBrowser::CACHE_HIT, 3);

  // Load the same pexe, but with a different cache identity.
  // This means that translation will actually be redone,
  // forcing the translators to be loaded a second time (but now with
  // cache hits!)
  RunLoadTest(FILE_PATH_LITERAL("pnacl_options.html?use_nmf=o_2"));

  // Should now have 3 more queries on top of the previous ones.
  histograms.ExpectTotalCount("NaCl.ValidationCache.Query", 6);
  // With the 3 extra queries being cache hits.
  histograms.ExpectBucketCount("NaCl.ValidationCache.Query",
                               nacl::NaClBrowser::CACHE_HIT, 3);
  // No extra cache settings.
  histograms.ExpectUniqueSample("NaCl.ValidationCache.Set",
                               nacl::NaClBrowser::CACHE_HIT, 3);
}


// TODO(ncbray) convert the rest of nacl_uma.py (currently in the NaCl repo.)
// Test validation failures and crashes.

}  // namespace
