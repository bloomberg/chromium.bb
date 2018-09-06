// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nux/email/email_handler.h"

#include "base/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/favicon/core/favicon_service.h"
#include "components/grit/components_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "components/nux/show_promo_delegate.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/resource/resource_bundle.h"

namespace nux {

struct EmailProviderType {
  const char* name;
  const char* url;
  const int icon;
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class EmailProviders {
  kGmail = 0,
  kYahoo = 1,
  kOutlook = 2,
  kAol = 3,
  kiCloud = 4,
  kCount,
};

const char* kEmailInteractionHistogram =
    "FirstRun.NewUserExperience.EmailInteraction";

// Strings in costants not translated because this is an experiment.
// Translate before wide release.
// TODO(hcarmona): populate with icon ids.
const EmailProviderType kEmail[] = {
    {"Gmail", "https://accounts.google.com/b/0/AddMailService", 0},
    {"Yahoo", "https://mail.yahoo.com", 0},
    {"Outlook", "https://login.live.com/login.srf?", 0},
    {"AOL", "https://mail.aol.com", 0},
    {"iCloud", "https://www.icloud.com/mail", 0},
};

constexpr const int kEmailIconSize = 48;  // Pixels.

static_assert(base::size(kEmail) == (size_t)EmailProviders::kCount,
              "names and histograms must match");

EmailHandler::EmailHandler(PrefService* prefs,
                           favicon::FaviconService* favicon_service,
                           bookmarks::BookmarkModel* bookmark_model)
    : prefs_(prefs),
      favicon_service_(favicon_service),
      bookmark_model_(bookmark_model) {}

EmailHandler::~EmailHandler() {}

void EmailHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "rejectEmails", base::BindRepeating(&EmailHandler::HandleRejectEmails,
                                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "addEmails", base::BindRepeating(&EmailHandler::HandleAddEmails,
                                       base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "toggleBookmarkBar",
      base::BindRepeating(&EmailHandler::HandleToggleBookmarkBar,
                          base::Unretained(this)));
}

void EmailHandler::HandleRejectEmails(const base::ListValue* args) {
  /* TODO(hcarmona): Add histograms and uncomment this code.
  UMA_HISTOGRAM_ENUMERATION(kEmailInteractionHistogram,
                            EmailInteraction::kNoThanks,
                            EmailInteraction::kCount);
  */
}

void EmailHandler::HandleAddEmails(const base::ListValue* args) {
  // Add bookmarks for all selected emails.
  int bookmarkIndex = 0;
  for (size_t i = 0; i < (size_t)EmailProviders::kCount; ++i) {
    bool selected = false;
    CHECK(args->GetBoolean(i, &selected));
    if (selected) {
      /* TODO(hcarmona): Add histograms and uncomment this code.
      UMA_HISTOGRAM_ENUMERATION("FirstRun.NewUserExperience.EmailSelection",
                                (EmailProviders)i,
                                EmailProviders::kCount);
      */
      GURL app_url = GURL(kEmail[i].url);
      bookmark_model_->AddURL(bookmark_model_->bookmark_bar_node(),
                              bookmarkIndex++,
                              base::ASCIIToUTF16(kEmail[i].name), app_url);

      // Preload the favicon cache with Chrome-bundled images. Otherwise, the
      // pre-populated bookmarks don't have favicons and look bad. Favicons are
      // updated automatically when a user visits a site.
      favicon_service_->MergeFavicon(
          app_url, app_url, favicon_base::IconType::kFavicon,
          ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
              kEmail[i].icon),
          gfx::Size(kEmailIconSize, kEmailIconSize));
    }
  }

  /* TODO(hcarmona): Add histograms and uncomment this code.
  UMA_HISTOGRAM_ENUMERATION(kEmailInteractionHistogram,
                            EmailInteraction::kGetStarted,
                            EmailInteraction::kCount);
  */
}

void EmailHandler::HandleToggleBookmarkBar(const base::ListValue* args) {
  bool show = false;
  CHECK(args->GetBoolean(0, &show));
  prefs_->SetBoolean(bookmarks::prefs::kShowBookmarkBar, show);
}

void EmailHandler::AddSources(content::WebUIDataSource* html_source,
                              PrefService* prefs) {
  // Localized strings.
  html_source->AddLocalizedString("noThanks", IDS_NO_THANKS);
  html_source->AddLocalizedString("getStarted", IDS_NUX_EMAIL_GET_STARTED);
  html_source->AddLocalizedString("welcomeTitle", IDS_NUX_EMAIL_WELCOME_TITLE);
  html_source->AddLocalizedString("emailPrompt", IDS_NUX_EMAIL_PROMPT);

  // Add required resources.
  html_source->AddResourcePath("email", IDR_NUX_EMAIL_HTML);
  html_source->AddResourcePath("email/nux_email.js", IDR_NUX_EMAIL_JS);

  html_source->AddResourcePath("email/nux_email_proxy.html",
                               IDR_NUX_EMAIL_PROXY_HTML);
  html_source->AddResourcePath("email/nux_email_proxy.js",
                               IDR_NUX_EMAIL_PROXY_JS);

  html_source->AddResourcePath("email/email_chooser.html",
                               IDR_NUX_EMAIL_CHOOSER_HTML);
  html_source->AddResourcePath("email/email_chooser.js",
                               IDR_NUX_EMAIL_CHOOSER_JS);

  // Add icons
  html_source->AddResourcePath("email/aol_1x.png", IDR_NUX_EMAIL_AOL_1X);
  html_source->AddResourcePath("email/aol_2x.png", IDR_NUX_EMAIL_AOL_2X);
  html_source->AddResourcePath("email/gmail_1x.png", IDR_NUX_EMAIL_GMAIL_1X);
  html_source->AddResourcePath("email/gmail_2x.png", IDR_NUX_EMAIL_GMAIL_2X);
  html_source->AddResourcePath("email/icloud_1x.png", IDR_NUX_EMAIL_ICLOUD_1X);
  html_source->AddResourcePath("email/icloud_2x.png", IDR_NUX_EMAIL_ICLOUD_2X);
  html_source->AddResourcePath("email/outlook_1x.png",
                               IDR_NUX_EMAIL_OUTLOOK_1X);
  html_source->AddResourcePath("email/outlook_2x.png",
                               IDR_NUX_EMAIL_OUTLOOK_2X);
  html_source->AddResourcePath("email/yahoo_1x.png", IDR_NUX_EMAIL_YAHOO_1X);
  html_source->AddResourcePath("email/yahoo_2x.png", IDR_NUX_EMAIL_YAHOO_2X);

  // Add constants to loadtime data
  for (size_t i = 0; i < (size_t)EmailProviders::kCount; ++i) {
    html_source->AddString("email_name_" + std::to_string(i), kEmail[i].name);
    html_source->AddString("email_url_" + std::to_string(i), kEmail[i].url);
  }
  html_source->AddInteger("email_count", (int)EmailProviders::kCount);
  html_source->AddBoolean(
      "bookmark_bar_shown",
      prefs->GetBoolean(bookmarks::prefs::kShowBookmarkBar));
  html_source->SetJsonPath("strings.js");
}

}  // namespace nux