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
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const int kIconSize = 32;

}  // namespace

namespace app_list {

PeopleResult::PeopleResult(Profile* profile,
                           const std::string& id,
                           const std::string& display_name,
                           double interaction_rank,
                           const GURL& image_url)
    : profile_(profile),
      id_(id),
      display_name_(display_name),
      interaction_rank_(interaction_rank),
      image_url_(image_url),
      weak_factory_(this) {
  set_id(id_);
  set_title(UTF8ToUTF16(display_name_));
  set_relevance(interaction_rank_);

  image_ = gfx::ImageSkia(
      new UrlIconSource(base::Bind(&PeopleResult::OnIconLoaded,
                                   weak_factory_.GetWeakPtr()),
                        profile_->GetRequestContext(),
                        image_url_,
                        kIconSize,
                        IDR_PROFILE_PICTURE_LOADING),
      gfx::Size(kIconSize, kIconSize));
  SetIcon(image_);
}

PeopleResult::~PeopleResult() {
}

void PeopleResult::Open(int event_flags) {
  // TODO(rkc): Navigate to the person's profile?
}

void PeopleResult::InvokeAction(int action_index, int event_flags) {
  DCHECK_EQ(0, action_index);
  // TODO(rkc): Decide what to do here.
}

scoped_ptr<ChromeSearchResult> PeopleResult::Duplicate() {
  return scoped_ptr<ChromeSearchResult>(new PeopleResult(profile_, id_,
                                                         display_name_,
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

ChromeSearchResultType PeopleResult::GetType() {
  return SEARCH_PEOPLE_SEARCH_RESULT;
}

}  // namespace app_list
