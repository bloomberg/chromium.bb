// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/font_handler.h"

#include <stddef.h>
#include <string>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/i18n/rtl.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/character_encoding.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/options/font_settings_utils.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/font_list_async.h"
#include "content/public/browser/web_ui.h"

namespace settings {

FontHandler::FontHandler(content::WebUI* webui)
    : profile_(Profile::FromWebUI(webui)),
      weak_ptr_factory_(this) {
  // Perform validation for saved fonts.
  options::FontSettingsUtilities::ValidateSavedFonts(profile_->GetPrefs());
}

FontHandler::~FontHandler() {}

void FontHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "fetchFontsData", base::Bind(&FontHandler::HandleFetchFontsData,
                                   base::Unretained(this)));
}

void FontHandler::HandleFetchFontsData(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  content::GetFontListAsync(base::Bind(&FontHandler::FontListHasLoaded,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       callback_id));
}

void FontHandler::FontListHasLoaded(std::string callback_id,
                                    std::unique_ptr<base::ListValue> list) {
  // Font list. Selects the directionality for the fonts in the given list.
  for (size_t i = 0; i < list->GetSize(); i++) {
    base::ListValue* font;
    bool has_font = list->GetList(i, &font);
    DCHECK(has_font);

    base::string16 value;
    bool has_value = font->GetString(1, &value);
    DCHECK(has_value);

    bool has_rtl_chars = base::i18n::StringContainsStrongRTLChars(value);
    font->Append(new base::StringValue(has_rtl_chars ? "rtl" : "ltr"));
  }

  // Character encoding list.
  const std::vector<CharacterEncoding::EncodingInfo>* encodings;
  PrefService* pref_service = Profile::FromWebUI(web_ui())->GetPrefs();
  encodings = CharacterEncoding::GetCurrentDisplayEncodings(
      g_browser_process->GetApplicationLocale(),
      pref_service->GetString(prefs::kStaticEncodings),
      pref_service->GetString(prefs::kRecentlySelectedEncoding));
  DCHECK(!encodings->empty());

  std::unique_ptr<base::ListValue> encoding_list(new base::ListValue());
  for (const auto& it : *encodings) {
    std::unique_ptr<base::ListValue> option(new base::ListValue());
    if (it.encoding_id) {
      option->AppendString(
          CharacterEncoding::GetCanonicalEncodingNameByCommandId(
            it.encoding_id));
      option->AppendString(it.encoding_display_name);
      option->AppendString(
          base::i18n::StringContainsStrongRTLChars(it.encoding_display_name)
          ? "rtl"
          : "ltr");
    } else {
      // Add empty value to indicate a separator item.
      option->AppendString(std::string());
    }
    encoding_list->Append(std::move(option));
  }

  base::DictionaryValue response;
  response.Set("fontList", std::move(list));
  response.Set("encodingList", std::move(encoding_list));

  ResolveJavascriptCallback(base::StringValue(callback_id), response);
}

}  // namespace settings
