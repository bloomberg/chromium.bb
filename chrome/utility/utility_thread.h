// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_UTILITY_THREAD_H_
#define CHROME_UTILITY_UTILITY_THREAD_H_
#pragma once

#include <string>
#include <vector>

#include "base/platform_file.h"
#include "chrome/common/child_thread.h"
#include "printing/native_metafile.h"

class GURL;
class IndexedDBKey;
class SerializedScriptValue;
class SkBitmap;

namespace gfx {
class Rect;
}  // namespace gfx

namespace printing {
struct PageRange;
}  // namespace printing

// This class represents the background thread where the utility task runs.
class UtilityThread : public ChildThread {
 public:
  UtilityThread();
  virtual ~UtilityThread();

  // Returns the one utility thread.
  static UtilityThread* current() {
    return static_cast<UtilityThread*>(ChildThread::current());
  }

 private:
  // IPC messages
  virtual bool OnControlMessageReceived(const IPC::Message& msg);
  void OnUnpackExtension(const FilePath& extension_path);

  // IPC messages for web resource service.
  void OnUnpackWebResource(const std::string& resource_data);

  // IPC for parsing an extensions auto-update manifest xml file.
  void OnParseUpdateManifest(const std::string& xml);

  // IPC for decoding an image.
  void OnDecodeImage(const std::vector<unsigned char>& encoded_data);

  // IPC to render a PDF into a platform metafile.
  void OnRenderPDFPagesToMetafile(
      base::PlatformFile pdf_file,
      const FilePath& metafile_path,
      const gfx::Rect& render_area,
      int render_dpi,
      const std::vector<printing::PageRange>& page_ranges);

#if defined(OS_WIN)
  // Helper method for Windows.
  // |highest_rendered_page_number| is set to -1 on failure to render any page.
  bool RenderPDFToWinMetafile(
    base::PlatformFile pdf_file,
    const FilePath& metafile_path,
    const gfx::Rect& render_area,
    int render_dpi,
    const std::vector<printing::PageRange>& page_ranges,
    printing::NativeMetafile* metafile,
    int* highest_rendered_page_number);
#endif   // defined(OS_WIN)

  // IPC for extracting IDBKeys from SerializedScriptValues, used by IndexedDB.
  void OnIDBKeysFromValuesAndKeyPath(
      int id,
      const std::vector<SerializedScriptValue>& serialized_script_values,
      const string16& idb_key_path);

  // IPC for injecting an IndexedDB key into a SerializedScriptValue.
  void OnInjectIDBKey(const IndexedDBKey& key,
                      const SerializedScriptValue& value,
                      const string16& key_path);

  // IPC to notify we'll be running in batch mode instead of quitting after
  // any of the IPCs above, we'll only quit during OnBatchModeFinished().
  void OnBatchModeStarted();

  // IPC to notify batch mode has finished and we should now quit.
  void OnBatchModeFinished();

  // IPC to get capabilities and defaults for the specified
  // printer. Used on Windows to isolate the service process from printer driver
  // crashes by executing this in a separate process. This does not run in a
  // sandbox.
  void OnGetPrinterCapsAndDefaults(const std::string& printer_name);

  // Releases the process if we are not (or no longer) in batch mode.
  void ReleaseProcessIfNeeded();

  // True when we're running in batch mode.
  bool batch_mode_;

  DISALLOW_COPY_AND_ASSIGN(UtilityThread);
};

#endif  // CHROME_UTILITY_UTILITY_THREAD_H_
