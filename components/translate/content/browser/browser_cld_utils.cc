// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/browser/browser_cld_utils.h"

#include "base/logging.h"
#include "components/translate/content/browser/browser_cld_data_provider_factory.h"
#include "components/translate/content/common/cld_data_source.h"

namespace translate {

// static
void BrowserCldUtils::ConfigureDefaultDataProvider() {
  if (!BrowserCldDataProviderFactory::IsInitialized()) {
    DVLOG(1) << "Configuring default BrowserCldDataProviderFactory";
    BrowserCldDataProviderFactory* factory = NULL;
    CldDataSource* data_source = NULL;

    // Maintainers: Customize platform defaults here as necessary.
    // It is appropriate to ifdef this code and customize per-platform.
    //
    // Remember to update GYP/GN dependencies of high-level targets as well:
    // - For the NONE or STATIC data sources, depend upon
    //   third_party/cld_2/cld_2.gyp:cld2_static.
    // - For the COMPONENT or STANDALONE data sources, depend upon
    //   third_party/cld_2/cld_2.gyp:cld2_dynamic.
    // - For any other sources, the embedder should already have set a factory
    //   and so this code should never be invoked.
    // -----------------------------------------------------------------------
    factory = new BrowserCldDataProviderFactory();
    data_source = CldDataSource::GetStaticDataSource();
    // -----------------------------------------------------------------------

    // Apply the values defined above
    BrowserCldDataProviderFactory::SetDefault(factory);
    CldDataSource::SetDefault(data_source);
  }
}

}  // namespace translate
