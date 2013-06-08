// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/webstore_result.h"

#include <vector>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/webstore_result_icon_source.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/common/extensions/extension.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace app_list {

WebstoreResult::WebstoreResult(Profile* profile,
                               const std::string& app_id,
                               const std::string& localized_name,
                               const GURL& icon_url)
    : profile_(profile),
      app_id_(app_id),
      localized_name_(localized_name),
      icon_url_(icon_url),
      weak_factory_(this) {
  set_id(extensions::Extension::GetBaseURLFromExtensionId(app_id_).spec());
  set_relevance(0.0);  // What is the right value to use?

  set_title(UTF8ToUTF16(localized_name_));
  SetDefaultDetails();

  const int kIconSize = 32;
  icon_ = gfx::ImageSkia(
      new WebstoreResultIconSource(
          base::Bind(&WebstoreResult::OnIconLoaded,
                     weak_factory_.GetWeakPtr()),
          profile_->GetRequestContext(),
          icon_url_,
          kIconSize),
      gfx::Size(kIconSize, kIconSize));
  SetIcon(icon_);
}

WebstoreResult::~WebstoreResult() {}

void WebstoreResult::Open(int event_flags) {
  const GURL store_url(extension_urls::GetWebstoreItemDetailURLPrefix() +
                       app_id_);
  chrome::NavigateParams params(profile_,
                                store_url,
                                content::PAGE_TRANSITION_LINK);
  params.disposition = ui::DispositionFromEventFlags(event_flags);
  chrome::Navigate(&params);
}

void WebstoreResult::InvokeAction(int action_index, int event_flags) {}

scoped_ptr<ChromeSearchResult> WebstoreResult::Duplicate() {
  return scoped_ptr<ChromeSearchResult>(
      new WebstoreResult(profile_, app_id_, localized_name_, icon_url_)).Pass();
}

void WebstoreResult::SetDefaultDetails() {
  const base::string16 details =
      l10n_util::GetStringUTF16(IDS_EXTENSION_WEB_STORE_TITLE);
  Tags details_tags;
  details_tags.push_back(Tag(SearchResult::Tag::DIM, 0, details.length()));

  set_details(details);
  set_details_tags(details_tags);
}

void WebstoreResult::OnIconLoaded() {
  // Remove the existing image reps since the icon data is loaded and they
  // need to be re-created.
  const std::vector<gfx::ImageSkiaRep>& image_reps = icon_.image_reps();
  for (size_t i = 0; i < image_reps.size(); ++i)
    icon_.RemoveRepresentation(image_reps[i].scale_factor());

  SetIcon(icon_);
}

}  // namespace app_list
