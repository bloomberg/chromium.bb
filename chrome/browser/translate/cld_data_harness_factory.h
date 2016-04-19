// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_CLD_DATA_HARNESS_FACTORY_H_
#define CHROME_BROWSER_TRANSLATE_CLD_DATA_HARNESS_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/translate/cld_data_harness.h"

namespace test {

// Testing class that allows embedders to inject an arbitrary CldDataHarness
// implementation into the runtime by defining a subclass of
// CldDataHarnessFactory and setting the global instance of it with
// Set(CldDataHarnessFactory*). Chromium code should use the factory instance
// returned by Get() to obtain CldDataHarness instances.
//
// There is significant nuance to obtaining a CldDataHarness object at runtime.
// For each of the "stock" CldDataSource implementations ("standalone",
// "static" and "component"), a CldDataHarness implementation is available and
// the default implementation of the CldDataHarnessFactory class knows how to
// do the right thing; but embedders can provider entirely custom CldDataSource
// implementations, and browser tests need to be able to use those data sources
// correctly at test time. Embedders implement a subclass of
// CldDataHarnessFactory and set it using Set(), below, to enable browser tests
// to function as realistically as possible with the embedder's data source.
class CldDataHarnessFactory {
 public:
  CldDataHarnessFactory() {}
  virtual ~CldDataHarnessFactory() {}

  // Create a new CldDataHarness.
  // The default implementation returns a simple CldDataHarness, which is
  // likely to be incorrect for most non-static CLD use cases.
  virtual std::unique_ptr<CldDataHarness> CreateCldDataHarness();

  // Unconditionally sets the factory for this process, overwriting any
  // previously-configured value. Open-source Chromium test code should almost
  // never use this method; it is provided for embedders to inject a factory
  // from outside of the Chromium code base. By default, the choice of which
  // factory to use is based on the currently-configured CldDataSource when
  // calling Get() (see below).
  static void Set(CldDataHarnessFactory* instance);

  // Returns the CldDataHarnessFactory instance previously set for the process,
  // if one has been set; otherwise, if the currently configured CldDataSource
  // is one of the available open-source types defined in CldDataSource, returns
  // a compatible harness. Otherwise (e.g., no specific instance has been set
  // and the CldDataSource is not one of the known open-source implementations),
  // debug builds will crash while release builds will return a no-op harness.
  static CldDataHarnessFactory* Get();

 private:
  DISALLOW_COPY_AND_ASSIGN(CldDataHarnessFactory);

  // Fetch the global instance of the "static" factory that produces a harness
  // suitable for use with the "static" CLD data source.
  // Only use to call Set(CldDataHarnessFactory*).
  static CldDataHarnessFactory* GetStaticDataHarnessFactory();

  // Fetch the global instance of the "standalone" factory that produces a
  // harness suitable for use with the "standalone" CLD data source.
  // Only use to call Set(CldDataHarnessFactory*).
  static CldDataHarnessFactory* GetStandaloneDataHarnessFactory();

  // Fetch the global instance of the "component" factory that produces a
  // harness suitable for use with the "component" CLD data source.
  // Only use to call Set(CldDataHarnessFactory*).
  static CldDataHarnessFactory* GetComponentDataHarnessFactory();
};
}  // namespace test

#endif  // CHROME_BROWSER_TRANSLATE_CLD_DATA_HARNESS_FACTORY_H_
