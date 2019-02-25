// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webshare/share_target_pref_helper.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "net/base/escape.h"

void UpdateShareTargetInPrefs(const GURL& manifest_url,
                              const blink::Manifest& manifest,
                              PrefService* pref_service) {
  DictionaryPrefUpdate update(pref_service, prefs::kWebShareVisitedTargets);
  base::DictionaryValue* share_target_dict = update.Get();

  // Manifest does not contain a share_target field, or it does but the
  // action is not a valid URL.
  if (!manifest.share_target.has_value() ||
      !manifest.share_target->action.is_valid()) {
    share_target_dict->RemoveWithoutPathExpansion(manifest_url.spec(), nullptr);
    return;
  }

  // TODO(mgiuca): This DCHECK is known to fail due to https://crbug.com/762388.
  // Currently, this can only happen if flags are turned on. These cases should
  // be fixed before this feature is rolled out.
  DCHECK(manifest_url.is_valid());

  constexpr char kNameKey[] = "name";
  constexpr char kActionKey[] = "action";
  constexpr char kMethodKey[] = "method";
  constexpr char kEnctypeKey[] = "enctype";
  constexpr char kTitleKey[] = "title";
  constexpr char kTextKey[] = "text";
  constexpr char kUrlKey[] = "url";
  constexpr char kFilesKey[] = "files";
  constexpr char kAcceptKey[] = "accept";

  std::unique_ptr<base::DictionaryValue> origin_dict(new base::DictionaryValue);

  if (!manifest.name.is_null()) {
    origin_dict->SetKey(kNameKey, base::Value(manifest.name.string()));
  }
  url::Replacements<char> replacements;
  replacements.ClearQuery();
  origin_dict->SetKey(
      kActionKey,
      base::Value(manifest.share_target->action.ReplaceComponents(replacements)
                      .spec()));
  origin_dict->SetKey(
      kMethodKey,
      base::Value(manifest.share_target->method ==
                          blink::Manifest::ShareTarget::Method::kPost
                      ? "POST"
                      : "GET"));
  origin_dict->SetKey(
      kEnctypeKey,
      base::Value(manifest.share_target->enctype ==
                          blink::Manifest::ShareTarget::Enctype::kMultipart
                      ? "multipart/form-data"
                      : "application/x-www-form-urlencoded"));
  if (!manifest.share_target->params.text.is_null()) {
    origin_dict->SetKey(
        kTextKey, base::Value(manifest.share_target->params.text.string()));
  }
  if (!manifest.share_target->params.title.is_null()) {
    origin_dict->SetKey(
        kTitleKey, base::Value(manifest.share_target->params.title.string()));
  }
  if (!manifest.share_target->params.url.is_null()) {
    origin_dict->SetKey(
        kUrlKey, base::Value(manifest.share_target->params.url.string()));
  }

  if (!manifest.share_target->params.files.empty()) {
    std::vector<base::Value> files;
    files.reserve(manifest.share_target->params.files.size());
    for (const blink::Manifest::ShareTargetFile& share_target_file :
         manifest.share_target->params.files) {
      std::vector<base::Value> accept;
      accept.reserve(share_target_file.accept.size());
      for (const base::string16& entry : share_target_file.accept) {
        accept.push_back(base::Value(entry));
      }

      files.push_back(base::DictionaryValue());
      base::DictionaryValue& file_dict =
          static_cast<base::DictionaryValue&>(files.back());
      file_dict.SetKey(kNameKey, base::Value(share_target_file.name));
      file_dict.SetKey(kAcceptKey, base::ListValue(std::move(accept)));
    }
    origin_dict->SetKey(kFilesKey, base::Value(files));
  }

  share_target_dict->SetWithoutPathExpansion(manifest_url.spec(),
                                             std::move(origin_dict));
}
