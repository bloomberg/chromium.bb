// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/share_target_pref_helper.h"

#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

void UpdateShareTargetInPrefs(base::StringPiece manifest_url,
                              base::Optional<std::string> url_template,
                              PrefService* pref_service) {
  DictionaryPrefUpdate update(pref_service, prefs::kWebShareVisitedTargets);
  base::DictionaryValue* share_target_dict = update.Get();

  if (!url_template.has_value()) {
    share_target_dict->RemoveWithoutPathExpansion(manifest_url, NULL);
    return;
  }

  constexpr char kUrlTemplateKey[] = "url_template";

  std::unique_ptr<base::DictionaryValue> origin_dict(new base::DictionaryValue);

  origin_dict->SetStringWithoutPathExpansion(kUrlTemplateKey,
                                             url_template.value());

  share_target_dict->SetWithoutPathExpansion(manifest_url,
                                             std::move(origin_dict));
}
