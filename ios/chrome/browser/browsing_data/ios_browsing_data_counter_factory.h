// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSING_DATA_IOS_BROWSING_DATA_COUNTER_FACTORY_H_
#define IOS_CHROME_BROWSER_BROWSING_DATA_IOS_BROWSING_DATA_COUNTER_FACTORY_H_

#include <memory>

#include "base/strings/string_piece.h"

namespace ios {
class ChromeBrowserState;
}

namespace browsing_data {
class BrowsingDataCounter;
}

class IOSBrowsingDataCounterFactory {
 public:
  // Creates a new instance of BrowsingDataCounter that is counting the data
  // for |browser_state|, related to a given deletion preference |pref_name|.
  static std::unique_ptr<browsing_data::BrowsingDataCounter>
  GetForBrowserStateAndPref(ios::ChromeBrowserState* browser_state,
                            base::StringPiece pref_name);

 private:
  IOSBrowsingDataCounterFactory();
  ~IOSBrowsingDataCounterFactory();

  DISALLOW_COPY_AND_ASSIGN(IOSBrowsingDataCounterFactory);
};

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_IOS_BROWSING_DATA_COUNTER_FACTORY_H_
