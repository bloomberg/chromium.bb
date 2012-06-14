// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/obsolete_os.h"

#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace browser {

string16 LocalizedObsoleteOSString() {
  // TODO(mark): Change kEndOfTheLine to true immediately prior to the last
  // build on the Chrome 21 branch.
  const bool kEndOfTheLine = false;

  return l10n_util::GetStringFUTF16(
      kEndOfTheLine ? IDS_MAC_10_5_LEOPARD_OBSOLETE_NOW :
                      IDS_MAC_10_5_LEOPARD_OBSOLETE_SOON,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
}

}  // namespace browser
