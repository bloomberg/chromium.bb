// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/people/people_result.h"

#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/common/url_icon_source.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

const int kIconSize = 32;
const char kImageSizePath[] = "s32-p/";
const char kEmailUrlPrefix[] = "mailto:";

// Add a query parameter to specify the size to fetch the image in. The
// original profile image can be of an arbitrary size, we ask the server to
// crop it to a square 32x32 using its smart cropping algorithm.
GURL GetImageUrl(const GURL& url) {
  std::string image_filename = url.ExtractFileName();
  if (image_filename.empty())
    return url;

  return url.Resolve(kImageSizePath + image_filename);
}

}  // namespace

namespace app_list {

PeopleResult::PeopleResult(Profile* profile,
                           const std::string& id,
                           const std::string& display_name,
                           const std::string& email,
                           double interaction_rank,
                           const GURL& image_url)
    : profile_(profile),
      id_(id),
      display_name_(display_name),
      email_(email),
      interaction_rank_(interaction_rank),
      image_url_(image_url),
      weak_factory_(this) {
  set_id(id_);
  set_title(UTF8ToUTF16(display_name_));
  set_relevance(interaction_rank_);
  set_details(UTF8ToUTF16(email_));

  SetDefaultActions();

  image_ = gfx::ImageSkia(
      new UrlIconSource(base::Bind(&PeopleResult::OnIconLoaded,
                                   weak_factory_.GetWeakPtr()),
                        profile_->GetRequestContext(),
                        GetImageUrl(image_url_),
                        kIconSize,
                        IDR_PROFILE_PICTURE_LOADING),
      gfx::Size(kIconSize, kIconSize));
  SetIcon(image_);
}

PeopleResult::~PeopleResult() {
}

void PeopleResult::Open(int event_flags) {
  InvokeAction(0, event_flags);
}

void PeopleResult::InvokeAction(int action_index, int event_flags) {
  DCHECK_EQ(0, action_index);
  // Currently we support only one action, sending mail.
  SendEmail();
}

scoped_ptr<ChromeSearchResult> PeopleResult::Duplicate() {
  return scoped_ptr<ChromeSearchResult>(new PeopleResult(profile_, id_,
                                                         display_name_,
                                                         email_,
                                                         interaction_rank_,
                                                         image_url_)).Pass();
}

void PeopleResult::OnIconLoaded() {
  // Remove the existing image reps since the icon data is loaded and they
  // need to be re-created.
  const std::vector<gfx::ImageSkiaRep>& image_reps = image_.image_reps();
  for (size_t i = 0; i < image_reps.size(); ++i)
    image_.RemoveRepresentation(image_reps[i].scale_factor());

  SetIcon(image_);
}

void PeopleResult::SetDefaultActions() {
  Actions actions;

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  actions.push_back(Action(
      *bundle.GetImageSkiaNamed(IDR_PEOPLE_SEARCH_ACTION_EMAIL),
      *bundle.GetImageSkiaNamed(IDR_PEOPLE_SEARCH_ACTION_EMAIL_HOVER),
      *bundle.GetImageSkiaNamed(IDR_PEOPLE_SEARCH_ACTION_EMAIL_PRESSED),
      l10n_util::GetStringUTF16(IDS_PEOPLE_SEARCH_ACTION_EMAIL_TOOLTIP)));
  SetActions(actions);
}

void PeopleResult::SendEmail() {
  chrome::NavigateParams params(profile_,
                                GURL(kEmailUrlPrefix + email_),
                                content::PAGE_TRANSITION_LINK);
  // If no window exists, this will open a new window this one tab.
  params.disposition = NEW_FOREGROUND_TAB;
  chrome::Navigate(&params);
}

ChromeSearchResultType PeopleResult::GetType() {
  return SEARCH_PEOPLE_SEARCH_RESULT;
}

}  // namespace app_list
