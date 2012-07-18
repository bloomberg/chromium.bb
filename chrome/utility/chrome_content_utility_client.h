// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_CHROME_CONTENT_UTILITY_CLIENT_H_
#define CHROME_UTILITY_CHROME_CONTENT_UTILITY_CLIENT_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "content/public/utility/content_utility_client.h"
#include "printing/pdf_render_settings.h"

class ExternalProcessImporterBridge;
class FilePath;
class Importer;

namespace base {
class DictionaryValue;
class Thread;
}

namespace gfx {
class Rect;
}

namespace importer {
struct SourceProfile;
}

namespace printing {
struct PageRange;
}

namespace chrome {

class ProfileImportHandler;

class ChromeContentUtilityClient : public content::ContentUtilityClient {
 public:
  ChromeContentUtilityClient();
  ~ChromeContentUtilityClient();

  virtual void UtilityThreadStarted() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  virtual bool Send(IPC::Message* message);

  // IPC message handlers.
  void OnUnpackExtension(const FilePath& extension_path,
                         const std::string& extension_id,
                         int location, int creation_flags);
  void OnUnpackWebResource(const std::string& resource_data);
  void OnParseUpdateManifest(const std::string& xml);
  void OnDecodeImage(const std::vector<unsigned char>& encoded_data);
  void OnDecodeImageBase64(const std::string& encoded_data);
  void OnRenderPDFPagesToMetafile(
      base::PlatformFile pdf_file,
      const FilePath& metafile_path,
      const printing::PdfRenderSettings& pdf_render_settings,
      const std::vector<printing::PageRange>& page_ranges);
  void OnParseJSON(const std::string& json);

#if defined(OS_WIN)
  // Helper method for Windows.
  // |highest_rendered_page_number| is set to -1 on failure to render any page.
  bool RenderPDFToWinMetafile(
      base::PlatformFile pdf_file,
      const FilePath& metafile_path,
      const gfx::Rect& render_area,
      int render_dpi,
      bool autorotate,
      const std::vector<printing::PageRange>& page_ranges,
      int* highest_rendered_page_number,
      double* scale_factor);
#endif   // defined(OS_WIN)

  void OnGetPrinterCapsAndDefaults(const std::string& printer_name);

  scoped_ptr<ProfileImportHandler> import_handler_;
};

}  // namespace chrome

#endif  // CHROME_UTILITY_CHROME_CONTENT_UTILITY_CLIENT_H_
