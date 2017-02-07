// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/share_target_pref_helper.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

void UpdateShareTargetInPrefs(const GURL& manifest_url,
                              const content::Manifest& manifest,
                              PrefService* pref_service) {
  DictionaryPrefUpdate update(pref_service, prefs::kWebShareVisitedTargets);
  base::DictionaryValue* share_target_dict = update.Get();

  // Manifest does not contain a share_target field, or it does but there is no
  // url_template field.
  if (!manifest.share_target.has_value() ||
      manifest.share_target.value().url_template.is_null()) {
    share_target_dict->RemoveWithoutPathExpansion(manifest_url.spec(), nullptr);
    return;
  }

  std::string url_template =
      base::UTF16ToUTF8(manifest.share_target.value().url_template.string());

  constexpr char kNameKey[] = "name";
  constexpr char kUrlTemplateKey[] = "url_template";

  std::unique_ptr<base::DictionaryValue> origin_dict(new base::DictionaryValue);

  if (!manifest.name.is_null()) {
    origin_dict->SetStringWithoutPathExpansion(kNameKey,
                                               manifest.name.string());
  }
  origin_dict->SetStringWithoutPathExpansion(kUrlTemplateKey,
                                             url_template);

  share_target_dict->SetWithoutPathExpansion(manifest_url.spec(),
                                             std::move(origin_dict));
}
