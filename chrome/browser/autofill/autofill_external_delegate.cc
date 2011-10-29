// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_external_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/autofill_messages.h"
#include "content/browser/renderer_host/render_view_host.h"

AutofillExternalDelegate::~AutofillExternalDelegate() {
}

AutofillExternalDelegate::AutofillExternalDelegate(
    TabContentsWrapper* tab_contents_wrapper)
    : tab_contents_wrapper_(tab_contents_wrapper) {
}

void AutofillExternalDelegate::SelectAutofillSuggestionAtIndex(int listIndex) {
  RenderViewHost* host = tab_contents_wrapper_->render_view_host();
  host->Send(new AutofillMsg_SelectAutofillSuggestionAtIndex(
                 host->routing_id(),
                 listIndex));
}


// Add a "!defined(OS_YOUROS) for each platform that implements this
// in an autofill_external_delegate_YOUROS.cc.  Currently there are
// none, so all platforms use the default.

#if !defined(OS_ANDROID)

AutofillExternalDelegate* AutofillExternalDelegate::Create(
    TabContentsWrapper*, AutofillManager*) {
  return NULL;
}

#endif
