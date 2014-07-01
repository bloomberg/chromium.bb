// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/font_list.h"

#include <windows.h>
#include <dwrite.h>

#include <set>

#include "base/strings/string16.h"
#include "base/values.h"
#include "base/win/scoped_comptr.h"
#include "content/common/sandbox_win.h"

namespace content {

static int CALLBACK EnumFontFamExProc(ENUMLOGFONTEXW* logical_font,
                                      NEWTEXTMETRICEXW* physical_font,
                                      DWORD font_type,
                                      LPARAM lparam) {
  std::set<base::string16>* font_names =
      reinterpret_cast<std::set<base::string16>*>(lparam);
  if (font_names) {
    const LOGFONTW& lf = logical_font->elfLogFont;
    if (lf.lfFaceName[0] && lf.lfFaceName[0] != '@') {
      base::string16 face_name(lf.lfFaceName);
      font_names->insert(face_name);
    }
  }
  return 1;
}

static void GetFontListInternal_GDI(base::ListValue* font_list) {
  std::set<base::string16> font_names;

  LOGFONTW logfont;
  memset(&logfont, 0, sizeof(logfont));
  logfont.lfCharSet = DEFAULT_CHARSET;

  HDC hdc = ::GetDC(NULL);
  ::EnumFontFamiliesExW(hdc, &logfont, (FONTENUMPROCW)&EnumFontFamExProc,
                        (LPARAM)&font_names, 0);
  ::ReleaseDC(NULL, hdc);

  std::set<base::string16>::iterator iter;
  for (iter = font_names.begin(); iter != font_names.end(); ++iter) {
    base::ListValue* font_item = new base::ListValue();
    font_item->Append(new base::StringValue(*iter));
    font_item->Append(new base::StringValue(*iter));
    font_list->Append(font_item);
  }
}

static void AddLocalizedFontFamily(base::ListValue* font_list,
                                   IDWriteFontFamily* font_family,
                                   wchar_t* user_locale) {
  base::win::ScopedComPtr<IDWriteLocalizedStrings> family_names;
  if (SUCCEEDED(font_family->GetFamilyNames(family_names.Receive()))) {
    UINT32 index = 0;
    BOOL exists = false;

    // Try to get name of font in the users locale first, failing that fall back
    // on US English and then the first name listed.
    family_names->FindLocaleName(user_locale, &index, &exists);
    if (!exists)
      family_names->FindLocaleName(L"en-us", &index, &exists);
    if (!exists)
      index = 0;

    UINT32 len = 0;
    if (SUCCEEDED(family_names->GetStringLength(index, &len))) {
      wchar_t* name = new wchar_t[len + 1];
      if (name) {
        if (SUCCEEDED(family_names->GetString(index, name, len + 1)) &&
            name[0] != '@') {
          base::ListValue* font_item = new base::ListValue();
          // First field is the name displayed to the user, second is the name
          // used internally and passed to the font system. For fonts the
          // localized name is used for both.
          font_item->Append(new base::StringValue(name));
          font_item->Append(new base::StringValue(name));
          font_list->Append(font_item);
        }
        delete[] name;
      }
    }
  }
}

static base::win::ScopedComPtr<IDWriteFactory> CreateDirectWriteFactory() {
  base::win::ScopedComPtr<IDWriteFactory> factory;
  typedef decltype(DWriteCreateFactory) * DWriteCreateFactoryProc;
  HMODULE dwrite_dll = LoadLibraryW(L"dwrite.dll");
  if (dwrite_dll) {
    DWriteCreateFactoryProc dwrite_create_factory_proc =
        reinterpret_cast<DWriteCreateFactoryProc>(
            GetProcAddress(dwrite_dll, "DWriteCreateFactory"));
    if (dwrite_create_factory_proc) {
      dwrite_create_factory_proc(
          DWRITE_FACTORY_TYPE_ISOLATED,
          __uuidof(IDWriteFactory),
          reinterpret_cast<IUnknown**>(factory.Receive()));
    }
  }
  return factory;
}

static void GetFontListInternal_DirectWrite(base::ListValue* font_list) {
  wchar_t user_locale[LOCALE_NAME_MAX_LENGTH];
  GetUserDefaultLocaleName(user_locale, LOCALE_NAME_MAX_LENGTH);

  base::win::ScopedComPtr<IDWriteFactory> factory = CreateDirectWriteFactory();

  base::win::ScopedComPtr<IDWriteFontCollection> font_collection;
  BOOL check_for_updates = false;
  if (factory && SUCCEEDED(factory->GetSystemFontCollection(
                     font_collection.Receive(), check_for_updates))) {
    UINT32 family_count = font_collection->GetFontFamilyCount();
    for (UINT32 i = 0; i < family_count; i++) {
      base::win::ScopedComPtr<IDWriteFontFamily> font_family;
      if (SUCCEEDED(font_collection->GetFontFamily(i, font_family.Receive())))
        AddLocalizedFontFamily(font_list, font_family, user_locale);
    }
  }
}

scoped_ptr<base::ListValue> GetFontList_SlowBlocking() {
  scoped_ptr<base::ListValue> font_list(new base::ListValue);

  if (ShouldUseDirectWrite())
    GetFontListInternal_DirectWrite(font_list.get());

  // Fallback on GDI if DirectWrite isn't enabled or if it failed to populate
  // the font list.
  if (font_list->GetSize() == 0)
    GetFontListInternal_GDI(font_list.get());

  return font_list.Pass();
}

}  // namespace content
