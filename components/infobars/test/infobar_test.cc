// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"

// Provides definitions of static variables and functions that are declared in
// the component but defined in the embedder.
// TODO(blundell): The component shouldn't be declaring statics that it's not
// defining; instead, this information should be obtained via a client,
// which can have a test implementation. crbug.com/386171

// Some components' unittests exercise code that requires that
// ConfirmInfoBarDelegate::CreateInfoBar() return a non-NULL infobar.
scoped_ptr<infobars::InfoBar> ConfirmInfoBarDelegate::CreateInfoBar(
    scoped_ptr<ConfirmInfoBarDelegate> delegate) {
  return scoped_ptr<infobars::InfoBar>(new infobars::InfoBar(delegate.Pass()));
}
