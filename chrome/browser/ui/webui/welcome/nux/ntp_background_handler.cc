// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/nux/ntp_background_handler.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "chrome/browser/search/background/onboarding_ntp_backgrounds.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/onboarding_welcome_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace nux {

enum class NtpBackgrounds {
  kArt = 0,
  kLandscape = 1,
  kCityscape = 2,
  kSeascape = 3,
  kGeometricShapes = 4,
};

NtpBackgroundHandler::NtpBackgroundHandler() {}
NtpBackgroundHandler::~NtpBackgroundHandler() {}

void NtpBackgroundHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getBackgrounds",
      base::BindRepeating(&NtpBackgroundHandler::HandleGetBackgrounds,
                          base::Unretained(this)));
}

void NtpBackgroundHandler::HandleGetBackgrounds(const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(1U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  base::ListValue list_value;
  std::array<GURL, kOnboardingNtpBackgroundsCount> onboardingNtpBackgrounds =
      GetOnboardingNtpBackgrounds();

  auto element = std::make_unique<base::DictionaryValue>();
  int id = static_cast<int>(NtpBackgrounds::kArt);
  element->SetInteger("id", id);
  element->SetString("title",
                     l10n_util::GetStringUTF8(
                         IDS_ONBOARDING_WELCOME_NTP_BACKGROUND_ART_TITLE));
  element->SetString("imageUrl", onboardingNtpBackgrounds[id].spec());
  list_value.Append(std::move(element));

  element = std::make_unique<base::DictionaryValue>();
  id = static_cast<int>(NtpBackgrounds::kLandscape);
  element->SetInteger("id", id);
  element->SetString(
      "title", l10n_util::GetStringUTF8(
                   IDS_ONBOARDING_WELCOME_NTP_BACKGROUND_LANDSCAPE_TITLE));
  element->SetString("imageUrl", onboardingNtpBackgrounds[id].spec());
  list_value.Append(std::move(element));

  element = std::make_unique<base::DictionaryValue>();
  id = static_cast<int>(NtpBackgrounds::kCityscape);
  element->SetInteger("id", id);
  element->SetString(
      "title", l10n_util::GetStringUTF8(
                   IDS_ONBOARDING_WELCOME_NTP_BACKGROUND_CITYSCAPE_TITLE));
  element->SetString("imageUrl", onboardingNtpBackgrounds[id].spec());
  list_value.Append(std::move(element));

  element = std::make_unique<base::DictionaryValue>();
  id = static_cast<int>(NtpBackgrounds::kSeascape);
  element->SetInteger("id", id);
  element->SetString("title",
                     l10n_util::GetStringUTF8(
                         IDS_ONBOARDING_WELCOME_NTP_BACKGROUND_SEASCAPE_TITLE));
  element->SetString("imageUrl", onboardingNtpBackgrounds[id].spec());
  list_value.Append(std::move(element));

  element = std::make_unique<base::DictionaryValue>();
  id = static_cast<int>(NtpBackgrounds::kGeometricShapes);
  element->SetInteger("id", id);
  element->SetString(
      "title",
      l10n_util::GetStringUTF8(
          IDS_ONBOARDING_WELCOME_NTP_BACKGROUND_GEOMETRIC_SHAPES_TITLE));
  element->SetString("imageUrl", onboardingNtpBackgrounds[id].spec());
  list_value.Append(std::move(element));

  ResolveJavascriptCallback(*callback_id, list_value);
}

}  // namespace nux
