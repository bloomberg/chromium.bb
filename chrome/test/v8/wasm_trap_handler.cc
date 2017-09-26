// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// These tests focus on Wasm out of bounds behavior to make sure trap-based
// bounds checks work when integrated with all of Chrome.

#include "base/base_switches.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"

class WasmTrapHandler : public InProcessBrowserTest {
 public:
  WasmTrapHandler() {}

 protected:
  void RunJSTest(const std::string& js) const {
    auto* const tab = browser()->tab_strip_model()->GetActiveWebContents();
    bool result = false;

    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(tab, js, &result));
    ASSERT_TRUE(result);
  }

  void RunJSTestAndEnsureTrapHandlerRan(const std::string& js) const {
#if defined(OS_LINUX) && defined(ARCH_CPU_X86_64) && !defined(OS_ANDROID)
    const bool is_trap_handler_supported = true;
#else
    const bool is_trap_handler_supported = false;
#endif

    if (is_trap_handler_supported &&
        base::FeatureList::IsEnabled(features::kWebAssemblyTrapHandler)) {
      const auto* get_fault_count =
          "domAutomationController.send(%GetWasmRecoveredTrapCount())";
      int original_count = 0;
      auto* const tab = browser()->tab_strip_model()->GetActiveWebContents();
      ASSERT_TRUE(content::ExecuteScriptAndExtractInt(tab, get_fault_count,
                                                      &original_count));
      ASSERT_NO_FATAL_FAILURE(RunJSTest(js));
      int new_count = 0;
      ASSERT_TRUE(content::ExecuteScriptAndExtractInt(tab, get_fault_count,
                                                      &new_count));
      ASSERT_GT(new_count, original_count);
    } else {
      ASSERT_NO_FATAL_FAILURE(RunJSTest(js));
    }
  }

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
// kEnableCrashReporterForTesting only exists on POSIX systems
#if defined(OS_POSIX)
    command_line->AppendSwitch(switches::kEnableCrashReporterForTesting);
#endif
    command_line->AppendSwitchASCII(switches::kJavaScriptFlags,
                                    "--allow-natives-syntax");
  }

  DISALLOW_COPY_AND_ASSIGN(WasmTrapHandler);
};

IN_PROC_BROWSER_TEST_F(WasmTrapHandler, OutOfBounds) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const auto& url = embedded_test_server()->GetURL("/wasm/out_of_bounds.html");
  ui_test_utils::NavigateToURL(browser(), url);

  ASSERT_NO_FATAL_FAILURE(RunJSTest("peek_in_bounds()"));
  ASSERT_NO_FATAL_FAILURE(
      RunJSTestAndEnsureTrapHandlerRan("peek_out_of_bounds()"));
  ASSERT_NO_FATAL_FAILURE(RunJSTestAndEnsureTrapHandlerRan(
      "peek_out_of_bounds_grow_memory_from_zero_js()"));
  ASSERT_NO_FATAL_FAILURE(
      RunJSTestAndEnsureTrapHandlerRan("peek_out_of_bounds_grow_memory_js()"));
  ASSERT_NO_FATAL_FAILURE(RunJSTestAndEnsureTrapHandlerRan(
      "peek_out_of_bounds_grow_memory_from_zero_wasm()"));
  ASSERT_NO_FATAL_FAILURE(RunJSTestAndEnsureTrapHandlerRan(
      "peek_out_of_bounds_grow_memory_wasm()"));

  ASSERT_NO_FATAL_FAILURE(RunJSTest("poke_in_bounds()"));
  ASSERT_NO_FATAL_FAILURE(
      RunJSTestAndEnsureTrapHandlerRan("poke_out_of_bounds()"));
  ASSERT_NO_FATAL_FAILURE(RunJSTestAndEnsureTrapHandlerRan(
      "poke_out_of_bounds_grow_memory_from_zero_js()"));
  ASSERT_NO_FATAL_FAILURE(
      RunJSTestAndEnsureTrapHandlerRan("poke_out_of_bounds_grow_memory_js()"));
  ASSERT_NO_FATAL_FAILURE(RunJSTestAndEnsureTrapHandlerRan(
      "poke_out_of_bounds_grow_memory_from_zero_wasm()"));
  ASSERT_NO_FATAL_FAILURE(RunJSTestAndEnsureTrapHandlerRan(
      "poke_out_of_bounds_grow_memory_wasm()"));
}
