// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/cld_data_harness_factory.h"

#include "base/lazy_instance.h"
#include "chrome/browser/translate/component_cld_data_harness.h"
#include "chrome/browser/translate/standalone_cld_data_harness.h"
#include "components/translate/content/browser/browser_cld_utils.h"
#include "components/translate/content/common/cld_data_source.h"

namespace {

// This is the global instance managed by Get/Set
test::CldDataHarnessFactory* g_instance = NULL;

// The following three instances are used to "cheat" in Create(). Each of them
// is just an instance of this class, but because they are singletons they can
// be compared using "this" to pick different behaviors in Create() at runtime.
// This avoids the need to explicitly define boilerplate classes with no
// significant value.
//
// Embedders can of course specify a different implementation of this class
// using Set(CldDataHarnessFactory*), in which case Create() will have whatever
// custom behavior is desired.
base::LazyInstance<test::CldDataHarnessFactory>::Leaky g_wrapped_component =
    LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<test::CldDataHarnessFactory>::Leaky g_wrapped_standalone =
    LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<test::CldDataHarnessFactory>::Leaky g_wrapped_static =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace test {

std::unique_ptr<CldDataHarness> CldDataHarnessFactory::CreateCldDataHarness() {
  // Cheat: Since the three "canned" implementations are all well-known, just
  // check to see if "this" points to one of the singletons and then return
  // the right answer. Embedder-provided CldDataHarnessFactory implementations
  // will of course not have this behavior.
  if (this == GetStaticDataHarnessFactory()) {
    return CldDataHarness::CreateStaticDataHarness();
  }
  if (this == GetStandaloneDataHarnessFactory()) {
    return CldDataHarness::CreateStandaloneDataHarness();
  }
  if (this == GetComponentDataHarnessFactory()) {
    return CldDataHarness::CreateComponentDataHarness();
  }

  // Embedder did something wrong, failed to override this method.
  NOTREACHED() << "Subclass failed to override CreateCldDataHarness()";
  return CldDataHarness::CreateStaticDataHarness();
}

CldDataHarnessFactory* CldDataHarnessFactory::Get() {
  // First honor any factory set by the embedder.
  if (g_instance != NULL) return g_instance;

  // Fall back to defaults: try to find a compatible harness for the currently-
  // configured CldDataSource.
  translate::BrowserCldUtils::ConfigureDefaultDataProvider();
  if (translate::CldDataSource::IsUsingStaticDataSource()) {
    return GetStaticDataHarnessFactory();
  }
  if (translate::CldDataSource::IsUsingComponentDataSource()) {
    return GetComponentDataHarnessFactory();
  }
  if (translate::CldDataSource::IsUsingStandaloneDataSource()) {
    return GetStandaloneDataHarnessFactory();
  }

  // The CldDataSource must have been set by an embedder, but the embedder has
  // not configured a corresponding CldDataHarnessFactory. This is an error; the
  // embedder must specify a harness that works with the corresponding
  // CldDataSource. Crash for debug builds and return a no-op harness for
  // everything else.
  NOTREACHED() << "No CLD data harness factory was configured!";
  return GetStaticDataHarnessFactory();
}

// static
void CldDataHarnessFactory::Set(CldDataHarnessFactory* instance) {
  g_instance = instance;
}

// static
CldDataHarnessFactory*
CldDataHarnessFactory::GetStaticDataHarnessFactory() {
  return &g_wrapped_static.Get();
}

// static
CldDataHarnessFactory*
CldDataHarnessFactory::GetStandaloneDataHarnessFactory() {
  return &g_wrapped_standalone.Get();
}

// static
CldDataHarnessFactory*
CldDataHarnessFactory::GetComponentDataHarnessFactory() {
  return &g_wrapped_component.Get();
}

}  // namespace test
