// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_view_manager_common.h"

#if defined(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/print_view_manager.h"
#else
#include "chrome/browser/printing/print_view_manager_basic.h"
#endif  // defined(ENABLE_PRINT_PREVIEW)

namespace printing {

void StartPrint(content::WebContents* contents,
                bool print_preview_disabled,
                bool selection_only) {
#if defined(ENABLE_PRINT_PREVIEW)
  using PrintViewManagerImpl = PrintViewManager;
#else
  using PrintViewManagerImpl = PrintViewManagerBasic;
#endif  // defined(ENABLE_PRINT_PREVIEW)

  auto print_view_manager = PrintViewManagerImpl::FromWebContents(contents);
  if (!print_view_manager)
    return;
#if defined(ENABLE_PRINT_PREVIEW)
  if (!print_preview_disabled) {
    print_view_manager->PrintPreviewNow(selection_only);
    return;
  }
#endif  // ENABLE_PRINT_PREVIEW

#if defined(ENABLE_BASIC_PRINTING)
  print_view_manager->PrintNow();
#endif  // ENABLE_BASIC_PRINTING
}

#if defined(ENABLE_BASIC_PRINTING)
void StartBasicPrint(content::WebContents* contents) {
#if defined(ENABLE_PRINT_PREVIEW)
  PrintViewManager* print_view_manager =
      PrintViewManager::FromWebContents(contents);
  if (!print_view_manager)
    return;
  print_view_manager->BasicPrint();
#endif  // ENABLE_PRINT_PREVIEW
}
#endif  // ENABLE_BASIC_PRINTING

}  // namespace printing
