// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_CHROME_CONTENT_UTILITY_CLIENT_H_
#define CHROME_UTILITY_CHROME_CONTENT_UTILITY_CLIENT_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "content/utility/content_utility_client.h"

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

class ChromeContentUtilityClient : public content::ContentUtilityClient {
 public:
  ChromeContentUtilityClient();
  ~ChromeContentUtilityClient();

  virtual void UtilityThreadStarted() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  virtual bool Send(IPC::Message* message);

  // IPC message handlers.
  void OnUnpackExtension(const FilePath& extension_path);
  void OnUnpackWebResource(const std::string& resource_data);
  void OnParseUpdateManifest(const std::string& xml);
  void OnDecodeImage(const std::vector<unsigned char>& encoded_data);
  void OnDecodeImageBase64(const std::string& encoded_data);
  void OnRenderPDFPagesToMetafile(
      base::PlatformFile pdf_file,
      const FilePath& metafile_path,
      const gfx::Rect& render_area,
      int render_dpi,
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
      const std::vector<printing::PageRange>& page_ranges,
      int* highest_rendered_page_number);
#endif   // defined(OS_WIN)

  void OnGetPrinterCapsAndDefaults(const std::string& printer_name);

  void OnImportStart(
      const importer::SourceProfile& source_profile,
      uint16 items,
      const base::DictionaryValue& localized_strings);
  void OnImportCancel();
  void OnImportItemFinished(uint16 item);

  // The following are used with out of process profile import:
  void ImporterCleanup();

  // Thread that importer runs on, while ProfileImportThread handles messages
  // from the browser process.
  scoped_ptr<base::Thread> import_thread_;

  // Bridge object is passed to importer, so that it can send IPC calls
  // directly back to the ProfileImportProcessHost.
  scoped_refptr<ExternalProcessImporterBridge> bridge_;

  // A bitmask of importer::ImportItem.
  uint16 items_to_import_;

  // Importer of the appropriate type (Firefox, Safari, IE, etc.)
  scoped_refptr<Importer> importer_;
};

}  // namespace chrome

#endif  // CHROME_UTILITY_CHROME_CONTENT_UTILITY_CLIENT_H_
