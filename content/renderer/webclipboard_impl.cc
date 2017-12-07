// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webclipboard_impl.h"

#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/common/drop_data.h"
#include "content/renderer/clipboard_utils.h"
#include "content/renderer/drop_data_builder.h"
#include "content/renderer/render_thread_impl.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "third_party/WebKit/public/platform/WebDragData.h"
#include "third_party/WebKit/public/platform/WebImage.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "url/gurl.h"

using blink::WebBlobInfo;
using blink::WebClipboard;
using blink::WebDragData;
using blink::WebImage;
using blink::WebString;
using blink::WebURL;
using blink::WebVector;

namespace content {

WebClipboardImpl::WebClipboardImpl(mojom::ClipboardHost& clipboard)
    : clipboard_(clipboard) {}

WebClipboardImpl::~WebClipboardImpl() = default;

uint64_t WebClipboardImpl::SequenceNumber(
    blink::mojom::ClipboardBuffer buffer) {
  uint64_t result = 0;
  if (!IsValidBufferType(buffer))
    return 0;

  clipboard_.GetSequenceNumber(buffer, &result);
  return result;
}

bool WebClipboardImpl::IsFormatAvailable(blink::mojom::ClipboardFormat format,
                                         blink::mojom::ClipboardBuffer buffer) {
  if (!IsValidBufferType(buffer))
    return false;

  bool result = false;
  clipboard_.IsFormatAvailable(format, buffer, &result);
  return result;
}

WebVector<WebString> WebClipboardImpl::ReadAvailableTypes(
    blink::mojom::ClipboardBuffer buffer,
    bool* contains_filenames) {
  std::vector<base::string16> types;
  if (IsValidBufferType(buffer)) {
    clipboard_.ReadAvailableTypes(buffer, &types, contains_filenames);
  }
  WebVector<WebString> web_types(types.size());
  std::transform(
      types.begin(), types.end(), web_types.begin(),
      [](const base::string16& s) { return WebString::FromUTF16(s); });
  return web_types;
}

WebString WebClipboardImpl::ReadPlainText(
    blink::mojom::ClipboardBuffer buffer) {
  if (!IsValidBufferType(buffer))
    return WebString();

  base::string16 text;
  clipboard_.ReadText(buffer, &text);
  return WebString::FromUTF16(text);
}

WebString WebClipboardImpl::ReadHTML(blink::mojom::ClipboardBuffer buffer,
                                     WebURL* source_url,
                                     unsigned* fragment_start,
                                     unsigned* fragment_end) {
  if (!IsValidBufferType(buffer))
    return WebString();

  base::string16 html_stdstr;
  GURL gurl;
  clipboard_.ReadHtml(buffer, &html_stdstr, &gurl,
                      static_cast<uint32_t*>(fragment_start),
                      static_cast<uint32_t*>(fragment_end));
  *source_url = gurl;
  return WebString::FromUTF16(html_stdstr);
}

WebString WebClipboardImpl::ReadRTF(blink::mojom::ClipboardBuffer buffer) {
  if (!IsValidBufferType(buffer))
    return WebString();

  std::string rtf;
  clipboard_.ReadRtf(buffer, &rtf);
  return WebString::FromLatin1(rtf);
}

WebBlobInfo WebClipboardImpl::ReadImage(blink::mojom::ClipboardBuffer buffer) {
  if (!IsValidBufferType(buffer))
    return WebBlobInfo();

  std::string blob_uuid;
  std::string type;
  int64_t size = -1;
  clipboard_.ReadImage(buffer, &blob_uuid, &type, &size);
  if (size < 0)
    return WebBlobInfo();
  return WebBlobInfo(WebString::FromASCII(blob_uuid), WebString::FromUTF8(type),
                     size);
}

WebString WebClipboardImpl::ReadCustomData(blink::mojom::ClipboardBuffer buffer,
                                           const WebString& type) {
  if (!IsValidBufferType(buffer))
    return WebString();

  base::string16 data;
  clipboard_.ReadCustomData(buffer, type.Utf16(), &data);
  return WebString::FromUTF16(data);
}

void WebClipboardImpl::WritePlainText(const WebString& plain_text) {
  clipboard_.WriteText(blink::mojom::ClipboardBuffer::kStandard,
                       plain_text.Utf16());
  clipboard_.CommitWrite(blink::mojom::ClipboardBuffer::kStandard);
}

void WebClipboardImpl::WriteHTML(const WebString& html_text,
                                 const WebURL& source_url,
                                 const WebString& plain_text,
                                 bool write_smart_paste) {
  clipboard_.WriteHtml(blink::mojom::ClipboardBuffer::kStandard,
                       html_text.Utf16(), source_url);
  clipboard_.WriteText(blink::mojom::ClipboardBuffer::kStandard,
                       plain_text.Utf16());

  if (write_smart_paste)
    clipboard_.WriteSmartPasteMarker(blink::mojom::ClipboardBuffer::kStandard);
  clipboard_.CommitWrite(blink::mojom::ClipboardBuffer::kStandard);
}

void WebClipboardImpl::WriteImage(const WebImage& image,
                                  const WebURL& url,
                                  const WebString& title) {
  DCHECK(!image.IsNull());
  if (!WriteImageToClipboard(blink::mojom::ClipboardBuffer::kStandard,
                             image.GetSkBitmap()))
    return;

#if !defined(OS_MACOSX)
  if (!url.IsEmpty()) {
    // When writing the image, we also write the image markup so that pasting
    // into rich text editors, such as Gmail, reveals the image. We also don't
    // want to call writeText(), since some applications (WordPad) don't pick
    // the image if there is also a text format on the clipboard.
    // We also don't want to write HTML on a Mac, since Mail.app prefers to use
    // the image markup over attaching the actual image. See
    // http://crbug.com/33016 for details.
    clipboard_.WriteHtml(blink::mojom::ClipboardBuffer::kStandard,
                         base::UTF8ToUTF16(URLToImageMarkup(url, title)),
                         GURL());
    // Only write a bookmark (url/title pair) if we're writing markup,
    // otherwise web applications such as Gmail frequently prefer string
    // content (assuming it to be markup, even if noted as text/plain) over
    // the image, resulting in the URL being pasted. https://crbug.com/736439
    GURL gurl(url);
    clipboard_.WriteBookmark(blink::mojom::ClipboardBuffer::kStandard,
                             gurl.spec(), title.Utf16());
  }
#endif
  clipboard_.CommitWrite(blink::mojom::ClipboardBuffer::kStandard);
}

void WebClipboardImpl::WriteDataObject(const WebDragData& data) {
  const DropData& data_object = DropDataBuilder::Build(data);
  // TODO(dcheng): Properly support text/uri-list here.
  // Avoid calling the WriteFoo functions if there is no data associated with a
  // type. This prevents stomping on clipboard contents that might have been
  // written by extension functions such as chrome.bookmarkManagerPrivate.copy.
  if (!data_object.text.is_null())
    clipboard_.WriteText(blink::mojom::ClipboardBuffer::kStandard,
                         data_object.text.string());
  if (!data_object.html.is_null())
    clipboard_.WriteHtml(blink::mojom::ClipboardBuffer::kStandard,
                         data_object.html.string(), GURL());
  if (!data_object.custom_data.empty()) {
    clipboard_.WriteCustomData(blink::mojom::ClipboardBuffer::kStandard,
                               std::move(data_object.custom_data));
  }
  clipboard_.CommitWrite(blink::mojom::ClipboardBuffer::kStandard);
}

bool WebClipboardImpl::IsValidBufferType(blink::mojom::ClipboardBuffer buffer) {
  switch (buffer) {
    case blink::mojom::ClipboardBuffer::kStandard:
      return true;
    case blink::mojom::ClipboardBuffer::kSelection:
#if defined(USE_X11)
      return true;
#else
      // Chrome OS and non-X11 unix builds do not support
      // the X selection clipboad.
      // TODO: remove the need for this case, see http://crbug.com/361753
      return false;
#endif
  }
  return true;
}

bool WebClipboardImpl::WriteImageToClipboard(
    blink::mojom::ClipboardBuffer buffer,
    const SkBitmap& bitmap) {
  // Only 32-bit bitmaps are supported.
  DCHECK_EQ(bitmap.colorType(), kN32_SkColorType);

  const gfx::Size size(bitmap.width(), bitmap.height());

  void* pixels = bitmap.getPixels();
  // TODO(piman): this should not be NULL, but it is. crbug.com/369621
  if (!pixels)
    return false;

  base::CheckedNumeric<uint32_t> checked_buf_size = 4;
  checked_buf_size *= size.width();
  checked_buf_size *= size.height();
  if (!checked_buf_size.IsValid())
    return false;

  // Allocate a shared memory buffer to hold the bitmap bits.
  uint32_t buf_size = checked_buf_size.ValueOrDie();
  auto shared_buffer = mojo::SharedBufferHandle::Create(buf_size);
  auto mapping = shared_buffer->Map(buf_size);
  memcpy(mapping.get(), pixels, buf_size);

  clipboard_.WriteImage(buffer, size, std::move(shared_buffer));
  return true;
}

}  // namespace content
