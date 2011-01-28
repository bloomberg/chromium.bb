// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines specific implementation of BrowserDistribution class for
// Google Chrome.

#include <algorithm>

#include <atlbase.h>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/installer/util/language_selector.h"

namespace {

const installer::LanguageSelector& GetLanguageSelector() {
  static const installer::LanguageSelector instance;

  return instance;
}

}  // namespace

namespace installer {

std::wstring GetLocalizedString(int base_message_id) {
  std::wstring localized_string;

  int message_id = base_message_id + GetLanguageSelector().offset();
  const ATLSTRINGRESOURCEIMAGE* image = AtlGetStringResourceImage(
      _AtlBaseModule.GetModuleInstance(), message_id);
  if (image) {
    localized_string = std::wstring(image->achString, image->nLength);
  } else {
    NOTREACHED() << "Unable to find resource id " << message_id;
  }

  return localized_string;
}

// Here we generate the url spec with the Microsoft res:// scheme which is
// explained here : http://support.microsoft.com/kb/220830
std::wstring GetLocalizedEulaResource() {
  wchar_t full_exe_path[MAX_PATH];
  int len = ::GetModuleFileName(NULL, full_exe_path, MAX_PATH);
  if (len == 0 || len == MAX_PATH)
    return L"";

  // The resource names are more or less the upcased language names.
  std::wstring language(GetLanguageSelector().selected_translation());
  std::replace(language.begin(), language.end(), L'-', L'_');
  StringToUpperASCII(&language);

  std::wstring resource(L"IDR_OEMPG_");
  resource.append(language).append(L".HTML");

  // Fall back on "en" if we don't have a resource for this language.
  if (NULL == FindResource(NULL, resource.c_str(), RT_HTML))
    resource = L"IDR_OEMPG_EN.HTML";

  // Spaces and DOS paths must be url encoded.
  std::wstring url_path =
      StringPrintf(L"res://%ls/#23/%ls", full_exe_path, resource.c_str());

  // The cast is safe because url_path has limited length
  // (see the definition of full_exe_path and resource).
  DCHECK(kuint32max > (url_path.size() * 3));
  DWORD count = static_cast<DWORD>(url_path.size() * 3);
  scoped_array<wchar_t> url_canon(new wchar_t[count]);
  HRESULT hr = ::UrlCanonicalizeW(url_path.c_str(), url_canon.get(),
                                  &count, URL_ESCAPE_UNSAFE);
  if (SUCCEEDED(hr))
    return std::wstring(url_canon.get());
  return url_path;
}

}  // namespace installer
