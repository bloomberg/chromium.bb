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
#include "components/nux/email/constants.h"
#include "components/nux/show_promo_delegate.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/resource/resource_bundle.h"

namespace nux {

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

constexpr const char* kEmailNames[] = {
    "Gmail", "Yahoo", "Outlook", "AOL", "iCloud",
};

// TODO(hcarmona): get correct URLs
constexpr const char* kEmailUrls[] = {
    "https://gmail.com",       "https://youtube.com",
    "https://maps.google.com", "https://translate.google.com",
    "https://news.google.com",
};

constexpr const int kEmailIconSize = 48;  // Pixels.
constexpr const int kEmailIcons[] = {
    // TODO(hcarmona): populate list of icons
};

static_assert(base::size(kEmailNames) == base::size(kEmailUrls),
              "names and urls must match");
static_assert(base::size(kEmailNames) == (size_t)EmailProviders::kCount,
              "names and histograms must match");
/*static_assert(base::size(kEmailNames) == base::size(kEmailIcons),
              "names and icons must match");*/

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
      GURL app_url = GURL(kEmailUrls[i]);
      bookmark_model_->AddURL(bookmark_model_->bookmark_bar_node(),
                              bookmarkIndex++,
                              base::ASCIIToUTF16(kEmailNames[i]), app_url);

      // Preload the favicon cache with Chrome-bundled images. Otherwise, the
      // pre-populated bookmarks don't have favicons and look bad. Favicons are
      // updated automatically when a user visits a site.
      favicon_service_->MergeFavicon(
          app_url, app_url, favicon_base::IconType::kFavicon,
          ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
              kEmailIcons[i]),
          gfx::Size(kEmailIconSize, kEmailIconSize));
    }
  }

  // Enable bookmark bar.
  prefs_->SetBoolean(bookmarks::prefs::kShowBookmarkBar, true);

  // Show bookmark bubble.
  /* TODO(hcarmona): Show promo bubble.
  ShowPromoDelegate::CreatePromoDelegate(
      IDS_NUX_EMAIL_DESCRIPTION_PROMO_BUBBLE)
      ->ShowForNode(bookmark_model_->bookmark_bar_node()->GetChild(0));
  */

  /* TODO(hcarmona): Add histograms and uncomment this code.
  UMA_HISTOGRAM_ENUMERATION(kEmailInteractionHistogram,
                            EmailInteraction::kGetStarted,
                            EmailInteraction::kCount);
  */
}

void EmailHandler::AddSources(content::WebUIDataSource* html_source) {
  // Localized strings.

  // Add required resources.
  html_source->AddResourcePath("email", IDR_NUX_EMAIL_HTML);

  // Add icons
}

}  // namespace nux