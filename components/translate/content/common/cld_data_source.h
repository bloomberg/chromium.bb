// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_COMMON_CLD_DATA_SOURCE_H_
#define COMPONENTS_TRANSLATE_CONTENT_COMMON_CLD_DATA_SOURCE_H_

#include <string>

namespace translate {

// Provides high-level functionality related to a CLD Data Source.
class CldDataSource {

 public:

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
  static std::string GetName();

};

}  // namespace translate
#endif  // COMPONENTS_TRANSLATE_CONTENT_COMMON_CLD_DATA_SOURCE_H_
