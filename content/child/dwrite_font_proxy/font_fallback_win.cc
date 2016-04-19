// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/dwrite_font_proxy/font_fallback_win.h"

#include "base/strings/string16.h"

#include "content/child/dwrite_font_proxy/dwrite_font_proxy_win.h"
#include "content/common/dwrite_font_proxy_messages.h"
#include "content/public/child/child_thread.h"
#include "ipc/ipc_sender.h"

namespace mswr = Microsoft::WRL;

namespace content {

FontFallback::FontFallback() = default;
FontFallback::~FontFallback() = default;

HRESULT FontFallback::MapCharacters(IDWriteTextAnalysisSource* source,
                                    UINT32 text_position,
                                    UINT32 text_length,
                                    IDWriteFontCollection* base_font_collection,
                                    const wchar_t* base_family_name,
                                    DWRITE_FONT_WEIGHT base_weight,
                                    DWRITE_FONT_STYLE base_style,
                                    DWRITE_FONT_STRETCH base_stretch,
                                    UINT32* mapped_length,
                                    IDWriteFont** mapped_font,
                                    FLOAT* scale) {
  *mapped_font = nullptr;
  *mapped_length = 1;
  *scale = 1.0;

  const WCHAR* text = nullptr;
  UINT32 chunk_length = 0;
  if (FAILED(source->GetTextAtPosition(text_position, &text, &chunk_length))) {
    DCHECK(false);
    return E_FAIL;
  }
  base::string16 text_chunk(text, chunk_length);

  const WCHAR* locale = nullptr;
  // |locale_text_length| is actually the length of text with the locale, not
  // the length of the locale string itself.
  UINT32 locale_text_length = 0;
  source->GetLocaleName(text_position /*textPosition*/, &locale_text_length,
                        &locale);

  if (locale == nullptr)
    locale = L"";

  DWriteFontStyle style;
  style.font_weight = base_weight;
  style.font_slant = base_style;
  style.font_stretch = base_stretch;

  MapCharactersResult result;

  IPC::Sender* sender =
      sender_override_ ? sender_override_ : ChildThread::Get();
  if (!sender->Send(new DWriteFontProxyMsg_MapCharacters(
          text_chunk, style, locale, source->GetParagraphReadingDirection(),
          base_family_name ? base_family_name : L"", &result)))
    return E_FAIL;

  *mapped_length = result.mapped_length;
  *scale = result.scale;

  if (result.family_index == UINT32_MAX)
    return S_OK;

  mswr::ComPtr<IDWriteFontFamily> family;
  // It would be nice to find a way to determine at runtime if |collection_| is
  // a proxy collection, or just a generic IDWriteFontCollection. Unfortunately
  // I can't find a way to get QueryInterface to return the actual class when
  // using mswr::RuntimeClass. If we could use QI, we can fallback on
  // FindFontFamily if the proxy is not available.
  if (!collection_->GetFontFamily(result.family_index, result.family_name,
                                  &family)) {
    DCHECK(false);
    return E_FAIL;
  }

  if (FAILED(family->GetFirstMatchingFont(
          static_cast<DWRITE_FONT_WEIGHT>(result.font_style.font_weight),
          static_cast<DWRITE_FONT_STRETCH>(result.font_style.font_stretch),
          static_cast<DWRITE_FONT_STYLE>(result.font_style.font_slant),
          mapped_font))) {
    DCHECK(false);
    return E_FAIL;
  }

  DCHECK(*mapped_font);

  return S_OK;
}

HRESULT STDMETHODCALLTYPE
FontFallback::RuntimeClassInitialize(DWriteFontCollectionProxy* collection,
                                     IPC::Sender* sender_override) {
  sender_override_ = sender_override;
  collection_ = collection;
  return S_OK;
}

}  // namespace content
