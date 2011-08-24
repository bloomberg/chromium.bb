// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROME_PPB_PDF_IMPL_H_
#define CHROME_RENDERER_CHROME_PPB_PDF_IMPL_H_

struct PPB_PDF;

namespace chrome {

class PPB_PDF_Impl {
 public:
  // Returns a pointer to the interface implementing PPB_PDF that is exposed
  // to the plugin.
  static const PPB_PDF* GetInterface();
};

}  // namespace chrome

#endif  // CHROME_RENDERER_CHROME_PPB_PDF_IMPL_H_

