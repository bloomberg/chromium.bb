// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/main/athena_content_client.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "content/public/common/pepper_plugin_info.h"
#include "ppapi/shared_impl/ppapi_permissions.h"

namespace athena {

AthenaContentClient::AthenaContentClient() {}

AthenaContentClient::~AthenaContentClient() {}

void AthenaContentClient::AddPepperPlugins(
    std::vector<content::PepperPluginInfo>* plugins) {
  const char kPDFPluginMimeType[] = "application/pdf";
  const char kPDFPluginExtension[] = "pdf";
  const char kPDFPluginDescription[] = "Portable Document Format";
  const char kPDFPluginPrintPreviewMimeType[] =
      "application/x-google-chrome-print-preview-pdf";
  const uint32 kPDFPluginPermissions =
      ppapi::PERMISSION_PRIVATE | ppapi::PERMISSION_DEV;
  const char kPDFPluginName[] = "Chrome PDF Viewer";
  const base::FilePath::CharType kPDFPluginFileName[] =
      FILE_PATH_LITERAL("libpdf.so");

  base::FilePath module;
  if (!PathService::Get(base::DIR_MODULE, &module))
    return;

  content::PepperPluginInfo pdf;
  pdf.path = base::FilePath(module.Append(kPDFPluginFileName));
  pdf.name = kPDFPluginName;
  content::WebPluginMimeType pdf_mime_type(
      kPDFPluginMimeType, kPDFPluginExtension, kPDFPluginDescription);
  content::WebPluginMimeType print_preview_pdf_mime_type(
      kPDFPluginPrintPreviewMimeType,
      kPDFPluginExtension,
      kPDFPluginDescription);
  pdf.mime_types.push_back(pdf_mime_type);
  pdf.mime_types.push_back(print_preview_pdf_mime_type);
  pdf.permissions = kPDFPluginPermissions;
  plugins->push_back(pdf);

  ShellContentClient::AddPepperPlugins(plugins);
}

}  // namespace athena
