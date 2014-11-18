// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_COMMON_CLD_DATA_SOURCE_H_
#define COMPONENTS_TRANSLATE_CONTENT_COMMON_CLD_DATA_SOURCE_H_

#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"

namespace component_updater {
  // For friend-class declaration, see private section at bottom of class.
  class CldComponentInstallerTest;
}

namespace translate {

// Provides high-level functionality related to a CLD Data Source.
class CldDataSource {

 public:
  // Generally not used by Chromium code, but available for embedders to
  // configure additional data sources as subclasses.
  // Chromium code should use the getters (GetStaticDataSource(),
  // GetStandaloneDataSource(), and GetComponentDataSource()) and checkers
  // (IsUsingStaticDataSource(), IsUsingStandaloneDataSource() and
  // IsUsingComponentDataSource()) instead as appropriate.
  CldDataSource();
  virtual ~CldDataSource() {}

  // Returns the symbolic name of the data source. In the Chromium
  // open-source tree, the following data sources exist:
  // static       uses the static_[browser|renderer]_cld_data_provider
  //              implementations.
  // standalone   uses the data_file_[browser|renderer]_cld_data_provider
  //              implementations.
  // component    also uses the data_file_[browser|renderer]_cld_data_provider
  //              implementations.
  //
  // Other implementations based upon Chromium may provide CLD differently and
  // may have other names.
  // This method is threadsafe.
  virtual std::string GetName();

  // For data sources that support a separate CLD data file, configures the path
  // of that data file.
  //
  // The 'component' and 'standalone' data sources need this method to be called
  // in order to locate the CLD data on disk.
  // If the data source doesn't need or doesn't support such configuration, this
  // function is a no-op. This is the case for, e.g., the static data source.
  // This method is threadsafe.
  virtual void SetCldDataFilePath(const base::FilePath& path);

  // Returns the path most recently set by SetCldDataFilePath. The initial value
  // prior to any such call is the empty path. If the data source doesn't
  // support a data file, returns the empty path.
  // This method is threadsafe.
  virtual base::FilePath GetCldDataFilePath();

  // Sets the default data source for this process, i.e. the data source to be
  // used unless the embedder calls Set(CldDatasource*). This is the method
  // that normal (i.e., non-test) Chromium code should use; embedders can and
  // should use the unconditional Set(CldDataSource*) method instead. If a
  // default data source has already been set, this method does nothing.
  static void SetDefault(CldDataSource* data_source);

  // Unconditionally sets the data source for this process, overwriting any
  // previously-configured default. Normal Chromium code should never use this
  // method; it is provided for embedders to inject a data source from outside
  // of the Chromium code base. Test code can also use this method to force the
  // runtime to have a desired behavior.
  static void Set(CldDataSource* data_source);

  // Returns the data source for this process. Guaranteed to never be null.
  // If no instance has been set, this returns the same object obtained by
  // calling GetStaticDataSource(), which is always safe but may fail to
  // function if the CLD data is not *actually* statically linked.
  static CldDataSource* Get();

  // Fetch the global instance of the "static" data source.
  // Only use to call SetDefault(CldDataSource*) or Set(CldDataSource*).
  static CldDataSource* GetStaticDataSource();

  // Returns true if and only if the data source returned by Get() is the
  // "static" data source.
  static bool IsUsingStaticDataSource();

  // Fetch the global instance of the "standalone" data source.
  // Only use to call SetDefault(CldDataSource*) or Set(CldDataSource*).
  static CldDataSource* GetStandaloneDataSource();

  // Returns true if and only if the data source returned by Get() is the
  // "static" data source.
  static bool IsUsingStandaloneDataSource();

  // Fetch the global instance of the "component" data source.
  // Only use to call SetDefault(CldDataSource*) or Set(CldDataSource*).
  static CldDataSource* GetComponentDataSource();

  // Returns true if and only if the data source returned by Get() is the
  // "static" data source.
  static bool IsUsingComponentDataSource();

 private:
  friend class component_updater::CldComponentInstallerTest;

  // For unit test code ONLY. Under normal circumstances the calls to
  // SetCldDataFilePath() and GetCldDataFilePath() have a DHECK intended to
  // perform a sanity check on the runtime CLD data source configuration; no
  // production code should be calling SetCldDataFilePath() or
  // GetCldDataFilePath() unless the "component" or "standalone" data source is
  // being used. Unit tests will generally be built with the "static" data
  // source, and this method allows tests to bypass the DCHECK for testing
  // purposes.
  //
  // Unit tests that use this function should use it in SetUp(), and then call
  // EnableSanityChecksForTest() in TearDown() for maximum safety.
  static void DisableSanityChecksForTest();

  // This method [re-]enables the sanity check disabled by
  // DisableSanityChecksForTest().
  static void EnableSanityChecksForTest();

  base::FilePath m_cached_filepath;  // Guarded by m_file_lock
  base::Lock m_file_lock;  // Guards m_cached_filepath

  DISALLOW_COPY_AND_ASSIGN(CldDataSource);
};

}  // namespace translate
#endif  // COMPONENTS_TRANSLATE_CONTENT_COMMON_CLD_DATA_SOURCE_H_
