// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_external_delegate.h"

#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"

AutofillExternalDelegate* AutofillExternalDelegate::Create(
    TabContents* tab_contents,
    AutofillManager* manager) {
  NOTIMPLEMENTED() << "TODO(jrg): Upstream AutofillExternalDelegateAndroid";
  return NULL;
}
