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
  void GetSequenceNumber(blink::mojom::ClipboardBuffer buffer,
                         GetSequenceNumberCallback callback) override;
  void IsFormatAvailable(blink::mojom::ClipboardFormat format,
                         blink::mojom::ClipboardBuffer buffer,
                         IsFormatAvailableCallback callback) override;
  void ReadAvailableTypes(blink::mojom::ClipboardBuffer buffer,
                          ReadAvailableTypesCallback callback) override;
  void ReadText(blink::mojom::ClipboardBuffer buffer,
                ReadTextCallback callback) override;
  void ReadHtml(blink::mojom::ClipboardBuffer buffer,
                ReadHtmlCallback callback) override;
  void ReadRtf(blink::mojom::ClipboardBuffer buffer,
               ReadRtfCallback callback) override;
  void ReadImage(blink::mojom::ClipboardBuffer buffer,
                 ReadImageCallback callback) override;
  void ReadCustomData(blink::mojom::ClipboardBuffer buffer,
                      const base::string16& type,
                      ReadCustomDataCallback callback) override;
  void WriteText(blink::mojom::ClipboardBuffer buffer,
                 const base::string16& text) override;
  void WriteHtml(blink::mojom::ClipboardBuffer buffer,
                 const base::string16& markup,
                 const GURL& url) override;
  void WriteSmartPasteMarker(blink::mojom::ClipboardBuffer buffer) override;
  void WriteCustomData(
      blink::mojom::ClipboardBuffer buffer,
      const std::unordered_map<base::string16, base::string16>& data) override;
  void WriteBookmark(blink::mojom::ClipboardBuffer buffer,
                     const std::string& url,
                     const base::string16& title) override;
  void WriteImage(blink::mojom::ClipboardBuffer buffer,
                  const gfx::Size& size_in_pixels,
                  mojo::ScopedSharedBufferHandle shared_buffer_handle) override;
  void CommitWrite(blink::mojom::ClipboardBuffer buffer) override;
  void WriteStringToFindPboard(const base::string16& text) override;

  void ConvertBufferType(blink::mojom::ClipboardBuffer, ui::ClipboardType*);

  ui::Clipboard* clipboard_;  // Not owned
  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;
  std::unique_ptr<ui::ScopedClipboardWriter> clipboard_writer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_CLIPBOARD_HOST_IMPL_H_
