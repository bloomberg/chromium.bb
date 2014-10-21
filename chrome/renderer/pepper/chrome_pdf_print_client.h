// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PEPPER_CHROME_PDF_PRINT_CLIENT_H_
#define CHROME_RENDERER_PEPPER_CHROME_PDF_PRINT_CLIENT_H_

#include "base/macros.h"
#include "components/pdf/renderer/ppb_pdf_impl.h"

class ChromePDFPrintClient : public pdf::PPB_PDF_Impl::PrintClient {
 public:
  ChromePDFPrintClient();
  ~ChromePDFPrintClient() override;

 private:
  // pdf::PPB_PDF_Impl::PrintClient:
  bool IsPrintingEnabled(PP_Instance instance_id) override;
  bool Print(PP_Instance instance_id) override;

  DISALLOW_COPY_AND_ASSIGN(ChromePDFPrintClient);
};

#endif  // CHROME_RENDERER_PEPPER_CHROME_PDF_PRINT_CLIENT_H_
