// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_view_manager_basic.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(printing::PrintViewManagerBasic);

namespace printing {

PrintViewManagerBasic::PrintViewManagerBasic(content::WebContents* web_contents)
    : PrintViewManagerBase(web_contents) {
}

PrintViewManagerBasic::~PrintViewManagerBasic() {
}

}  // namespace printing
