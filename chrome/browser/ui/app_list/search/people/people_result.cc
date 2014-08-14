// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/people/people_result.h"

#include <vector>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/app_list/search/common/url_icon_source.h"
#include "chrome/browser/ui/app_list/search/people/person.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/common/extensions/api/hangouts_private.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/user_metrics.h"
#include "extensions/browser/event_router.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace OnHangoutRequested =
    extensions::api::hangouts_private::OnHangoutRequested;

using extensions::api::hangouts_private::User;
using extensions::api::hangouts_private::HangoutRequest;

namespace {

const char kImageSizePath[] = "s64-p/";
const char kEmailUrlPrefix[] = "mailto:";

const char* const kHangoutsExtensionIds[] = {
  "nckgahadagoaajjgafhacjanaoiihapd",
  "ljclpkphhpbpinifbeabbhlfddcpfdde",
  "ppleadejekpmccmnpjdimmlfljlkdfej",
  "eggnbpckecmjlblplehfpjjdhhidfdoj",
  "jfjjdfefebklmdbmenmlehlopoocnoeh",
  "knipolnnllmklapflnccelgolnpehhpl"
};

// Add a query parameter to specify the size to fetch the image in. The
// original profile image can be of an arbitrary size, we ask the server to
// crop it to a square 64x64 using its smart cropping algorithm.
GURL GetImageUrl(const GURL& url) {
  std::string image_filename = url.ExtractFileName();
  if (image_filename.empty())
    return url;

  return url.Resolve(kImageSizePath + image_filename);
}

}  // namespace

namespace app_list {

PeopleResult::PeopleResult(Profile* profile, scoped_ptr<Person> person)
    : profile_(profile), person_(person.Pass()), weak_factory_(this) {
  set_id(person_->id);
  set_title(base::UTF8ToUTF16(person_->display_name));
  set_relevance(person_->interaction_rank);
  set_details(base::UTF8ToUTF16(person_->email));

  RefreshHangoutsExtensionId();
  SetDefaultActions();

  int icon_size = GetPreferredIconDimension();
  image_ = gfx::ImageSkia(
      new UrlIconSource(
          base::Bind(&PeopleResult::OnIconLoaded, weak_factory_.GetWeakPtr()),
          profile_->GetRequestContext(),
          GetImageUrl(person_->image_url),
          icon_size,
          IDR_PROFILE_PICTURE_LOADING),
      gfx::Size(icon_size, icon_size));
  SetIcon(image_);
}

PeopleResult::~PeopleResult() {
}

void PeopleResult::Open(int event_flags) {
  // Action 0 will always be our default action.
  InvokeAction(0, event_flags);
}

void PeopleResult::InvokeAction(int action_index, int event_flags) {
  if (hangouts_extension_id_.empty()) {
    // If the hangouts app is not available, the only option we are showing
    // to the user is 'Send Email'.
    SendEmail();
  } else {
    switch (action_index) {
      case 0:
        OpenChat();
        break;
      case 1:
        SendEmail();
        break;
      default:
        LOG(ERROR) << "Invalid people search action: " << action_index;
    }
  }
}

scoped_ptr<ChromeSearchResult> PeopleResult::Duplicate() {
  return scoped_ptr<ChromeSearchResult>(
      new PeopleResult(profile_, person_->Duplicate().Pass())).Pass();
}

void PeopleResult::OnIconLoaded() {
  // Remove the existing image reps since the icon data is loaded and they
  // need to be re-created.
  const std::vector<gfx::ImageSkiaRep>& image_reps = image_.image_reps();
  for (size_t i = 0; i < image_reps.size(); ++i)
    image_.RemoveRepresentation(image_reps[i].scale());

  SetIcon(image_);
}

void PeopleResult::SetDefaultActions() {
  Actions actions;

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  if (!hangouts_extension_id_.empty()) {
    actions.push_back(Action(
        *bundle.GetImageSkiaNamed(IDR_PEOPLE_SEARCH_ACTION_CHAT),
        *bundle.GetImageSkiaNamed(IDR_PEOPLE_SEARCH_ACTION_CHAT_HOVER),
        *bundle.GetImageSkiaNamed(IDR_PEOPLE_SEARCH_ACTION_CHAT_PRESSED),
        l10n_util::GetStringUTF16(IDS_PEOPLE_SEARCH_ACTION_CHAT_TOOLTIP)));
  }
  actions.push_back(Action(
      *bundle.GetImageSkiaNamed(IDR_PEOPLE_SEARCH_ACTION_EMAIL),
      *bundle.GetImageSkiaNamed(IDR_PEOPLE_SEARCH_ACTION_EMAIL_HOVER),
      *bundle.GetImageSkiaNamed(IDR_PEOPLE_SEARCH_ACTION_EMAIL_PRESSED),
      l10n_util::GetStringUTF16(IDS_PEOPLE_SEARCH_ACTION_EMAIL_TOOLTIP)));
  SetActions(actions);
}

void PeopleResult::OpenChat() {
  HangoutRequest request;

  request.type = extensions::api::hangouts_private::HANGOUT_TYPE_CHAT;

  // from: the user this chat request is originating from.
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetInstance()->GetForProfile(profile_);
  DCHECK(signin_manager);
  request.from = signin_manager->GetAuthenticatedAccountId();

  // to: list of users with whom to start this hangout is with.
  linked_ptr<User> target(new User());
  target->id = person_->owner_id;
  request.to.push_back(target);

  scoped_ptr<extensions::Event> event(
      new extensions::Event(OnHangoutRequested::kEventName,
                            OnHangoutRequested::Create(request)));

  // TODO(rkc): Change this once we remove the hangoutsPrivate API.
  // See crbug.com/306672
  extensions::EventRouter::Get(profile_)
      ->DispatchEventToExtension(hangouts_extension_id_, event.Pass());

  content::RecordAction(base::UserMetricsAction("PeopleSearch_OpenChat"));
}

void PeopleResult::SendEmail() {
  chrome::NavigateParams params(profile_,
                                GURL(kEmailUrlPrefix + person_->email),
                                content::PAGE_TRANSITION_LINK);
  // If no window exists, this will open a new window this one tab.
  params.disposition = NEW_FOREGROUND_TAB;
  chrome::Navigate(&params);
  content::RecordAction(base::UserMetricsAction("PeopleSearch_SendEmail"));
}

void PeopleResult::RefreshHangoutsExtensionId() {
  // TODO(rkc): Change this once we remove the hangoutsPrivate API.
  // See crbug.com/306672
  for (size_t i = 0; i < arraysize(kHangoutsExtensionIds); ++i) {
    if (extensions::EventRouter::Get(profile_)->ExtensionHasEventListener(
            kHangoutsExtensionIds[i], OnHangoutRequested::kEventName)) {
      hangouts_extension_id_ = kHangoutsExtensionIds[i];
      return;
    }
  }
  hangouts_extension_id_.clear();
}

ChromeSearchResultType PeopleResult::GetType() {
  return SEARCH_PEOPLE_SEARCH_RESULT;
}

}  // namespace app_list
