// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_DWRITE_TEXT_ANALYSIS_SOURCE_WIN_H_
#define CONTENT_COMMON_DWRITE_TEXT_ANALYSIS_SOURCE_WIN_H_

#include <dwrite.h>
#include <wrl.h>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"

namespace content {

// Implements an IDWriteTextAnalysisSource, describing a single pre-defined
// chunk of text with a uniform locale, reading direction, and number
// substitution.
class CONTENT_EXPORT TextAnalysisSource
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IDWriteTextAnalysisSource> {
 public:
  TextAnalysisSource();

  // IDWriteTextAnalysisSource:
  HRESULT STDMETHODCALLTYPE GetLocaleName(UINT32 text_position,
                                          UINT32* text_length,
                                          const WCHAR** locale_name) override;
  HRESULT STDMETHODCALLTYPE GetNumberSubstitution(
      UINT32 text_position,
      UINT32* text_length,
      IDWriteNumberSubstitution** number_substitution) override;
  DWRITE_READING_DIRECTION STDMETHODCALLTYPE
  GetParagraphReadingDirection() override;
  HRESULT STDMETHODCALLTYPE GetTextAtPosition(UINT32 text_position,
                                              const WCHAR** text_string,
                                              UINT32* text_length) override;
  HRESULT STDMETHODCALLTYPE GetTextBeforePosition(UINT32 text_position,
                                                  const WCHAR** text_string,
                                                  UINT32* text_length) override;

  HRESULT STDMETHODCALLTYPE
  RuntimeClassInitialize(const base::string16& text,
                         const base::string16& locale_name,
                         IDWriteNumberSubstitution* number_substitution,
                         DWRITE_READING_DIRECTION reading_direction);

 protected:
  ~TextAnalysisSource() override;

 private:
  base::string16 text_;
  base::string16 locale_name_;
  Microsoft::WRL::ComPtr<IDWriteNumberSubstitution> number_substitution_;
  DWRITE_READING_DIRECTION reading_direction_;

  DISALLOW_ASSIGN(TextAnalysisSource);
};

}  // namespace content

#endif  // CONTENT_COMMON_DWRITE_TEXT_ANALYSIS_SOURCE_WIN_H_
