// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_CLIPBOARD_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_CLIPBOARD_HOST_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "build/build_config.h"
#include "content/common/clipboard.mojom.h"
#include "content/common/clipboard_format.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_associated_interface.h"
#include "content/public/browser/browser_message_filter.h"
#include "ui/base/clipboard/clipboard.h"

class GURL;

namespace gfx {
class Size;
}

namespace ui {
class ScopedClipboardWriter;
}  // namespace ui

namespace content {

class ChromeBlobStorageContext;
class ClipboardHostImplTest;

class CONTENT_EXPORT ClipboardHostImpl : public mojom::ClipboardHost {
 public:
  ~ClipboardHostImpl() override;

  static void Create(
      scoped_refptr<ChromeBlobStorageContext> blob_storage_context,
      mojom::ClipboardHostRequest request);

 private:
  friend class ClipboardHostImplTest;

  explicit ClipboardHostImpl(
      scoped_refptr<ChromeBlobStorageContext> blob_storage_context);

  void ReadAndEncodeImage(const SkBitmap& bitmap, ReadImageCallback callback);
  void OnReadAndEncodeImageFinished(std::vector<uint8_t> png_data,
                                    ReadImageCallback callback);

  // content::mojom::ClipboardHost
  void GetSequenceNumber(ui::ClipboardType type,
                         GetSequenceNumberCallback callback) override;
  void IsFormatAvailable(content::ClipboardFormat format,
                         ui::ClipboardType type,
                         IsFormatAvailableCallback callback) override;
  void ReadAvailableTypes(ui::ClipboardType type,
                          ReadAvailableTypesCallback callback) override;
  void ReadText(ui::ClipboardType type, ReadTextCallback callback) override;
  void ReadHtml(ui::ClipboardType type, ReadHtmlCallback callback) override;
  void ReadRtf(ui::ClipboardType type, ReadRtfCallback callback) override;
  void ReadImage(ui::ClipboardType type, ReadImageCallback callback) override;
  void ReadCustomData(ui::ClipboardType clipboard_type,
                      const base::string16& type,
                      ReadCustomDataCallback callback) override;
  void WriteText(ui::ClipboardType type, const base::string16& text) override;
  void WriteHtml(ui::ClipboardType type,
                 const base::string16& markup,
                 const GURL& url) override;
  void WriteSmartPasteMarker(ui::ClipboardType type) override;
  void WriteCustomData(
      ui::ClipboardType type,
      const std::unordered_map<base::string16, base::string16>& data) override;
  void WriteBookmark(ui::ClipboardType type,
                     const std::string& url,
                     const base::string16& title) override;
  void WriteImage(ui::ClipboardType type,
                  const gfx::Size& size_in_pixels,
                  mojo::ScopedSharedBufferHandle shared_buffer_handle) override;
  void CommitWrite(ui::ClipboardType type) override;
  void WriteStringToFindPboard(const base::string16& text) override;

  ui::Clipboard* clipboard_;  // Not owned
  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;
  std::unique_ptr<ui::ScopedClipboardWriter> clipboard_writer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_CLIPBOARD_HOST_IMPL_H_
