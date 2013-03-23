// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search_builder.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autocomplete/autocomplete_controller.h"
#include "chrome/browser/autocomplete/autocomplete_input.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "chrome/browser/autocomplete/autocomplete_result.h"
#include "chrome/browser/extensions/api/omnibox/omnibox_api.h"
#include "chrome/browser/extensions/extension_icon_image.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/common/extensions/api/icons/icons_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/search_box_model.h"
#include "ui/app_list/search_result.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/window_open_disposition.h"

#if defined(OS_CHROMEOS)
#include "base/memory/ref_counted.h"
#include "chrome/browser/autocomplete/contact_provider_chromeos.h"
#include "chrome/browser/chromeos/contacts/contact.pb.h"
#include "chrome/browser/chromeos/contacts/contact_manager.h"
#include "chrome/browser/extensions/api/rtc_private/rtc_private_api.h"
#include "chrome/browser/image_decoder.h"
#include "chrome/common/chrome_switches.h"
#endif

namespace {

int ACMatchStyleToTagStyle(int styles) {
  int tag_styles = 0;
  if (styles & ACMatchClassification::URL)
    tag_styles |= app_list::SearchResult::Tag::URL;
  if (styles & ACMatchClassification::MATCH)
    tag_styles |= app_list::SearchResult::Tag::MATCH;
  if (styles & ACMatchClassification::DIM)
    tag_styles |= app_list::SearchResult::Tag::DIM;

  return tag_styles;
}

// Translates ACMatchClassifications into SearchResult tags.
void ACMatchClassificationsToTags(
    const string16& text,
    const ACMatchClassifications& text_classes,
    app_list::SearchResult::Tags* tags) {
  int tag_styles = app_list::SearchResult::Tag::NONE;
  size_t tag_start = 0;

  for (size_t i = 0; i < text_classes.size(); ++i) {
    const ACMatchClassification& text_class = text_classes[i];

    // Closes current tag.
    if (tag_styles != app_list::SearchResult::Tag::NONE) {
      tags->push_back(app_list::SearchResult::Tag(
          tag_styles, tag_start, text_class.offset));
      tag_styles = app_list::SearchResult::Tag::NONE;
    }

    if (text_class.style == ACMatchClassification::NONE)
      continue;

    tag_start = text_class.offset;
    tag_styles = ACMatchStyleToTagStyle(text_class.style);
  }

  if (tag_styles != app_list::SearchResult::Tag::NONE) {
    tags->push_back(app_list::SearchResult::Tag(
        tag_styles, tag_start, text.length()));
  }
}

const extensions::Extension* GetExtensionByURL(Profile* profile,
                                               const GURL& url) {
  ExtensionService* service = profile->GetExtensionService();
  // Need to explicitly get chrome app because it does not override new tab and
  // not having a web extent to include new tab url, thus GetInstalledApp does
  // not find it.
  return url.spec() == chrome::kChromeUINewTabURL ?
      service->extensions()->GetByID(extension_misc::kChromeAppId) :
      service->GetInstalledApp(url);
}

// SearchBuildResult is an app list SearchResult built from an
// AutocompleteMatch.
class SearchBuilderResult : public app_list::SearchResult {
 public:
  SearchBuilderResult() : profile_(NULL) {}
  virtual ~SearchBuilderResult() {}

  Profile* profile() const { return profile_; }
  const AutocompleteMatch& match() const { return match_; }

  virtual void Init(Profile* profile, const AutocompleteMatch& match) {
    profile_ = profile;
    match_ = match;
    UpdateIcon();
    UpdateTitleAndDetails();
  }

 protected:
  virtual void UpdateIcon() {
    int resource_id = match_.starred ?
        IDR_OMNIBOX_STAR : AutocompleteMatch::TypeToIcon(match_.type);
    SetIcon(*ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        resource_id));
  }

  virtual void UpdateTitleAndDetails() {
    set_title(match_.contents);
    app_list::SearchResult::Tags title_tags;
    ACMatchClassificationsToTags(match_.contents,
                                 match_.contents_class,
                                 &title_tags);
    set_title_tags(title_tags);

    set_details(match_.description);
    app_list::SearchResult::Tags details_tags;
    ACMatchClassificationsToTags(match_.description,
                                 match_.description_class,
                                 &details_tags);
    set_details_tags(details_tags);
  }

 private:
  Profile* profile_;  // not owned

  AutocompleteMatch match_;

  DISALLOW_COPY_AND_ASSIGN(SearchBuilderResult);
};

// SearchBuilderResult implementation for AutocompleteMatch::EXTENSION_APP
// results.
class ExtensionAppResult : public SearchBuilderResult,
                           public extensions::IconImage::Observer {
 public:
  ExtensionAppResult() {}
  virtual ~ExtensionAppResult() {}

 protected:
  virtual void UpdateIcon() OVERRIDE {
    const extensions::Extension* extension =
        GetExtensionByURL(profile(), match().destination_url);
    if (extension)
      LoadExtensionIcon(extension);
    else
      SearchBuilderResult::UpdateIcon();
  }

 private:
  void LoadExtensionIcon(const extensions::Extension* extension) {
    const gfx::ImageSkia default_icon = extensions::OmniboxAPI::Get(profile())->
        GetOmniboxPopupIcon(extension->id()).AsImageSkia();
    icon_.reset(new extensions::IconImage(
        profile(),
        extension,
        extensions::IconsInfo::GetIcons(extension),
        extension_misc::EXTENSION_ICON_SMALL,
        default_icon,
        this));
    SetIcon(icon_->image_skia());
  }

  // Overridden from extensions::IconImage::Observer:
  virtual void OnExtensionIconImageChanged(
      extensions::IconImage* image) OVERRIDE {
    DCHECK_EQ(icon_.get(), image);
    SetIcon(icon_->image_skia());
  }

  scoped_ptr<extensions::IconImage> icon_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionAppResult);
};

#if defined(OS_CHROMEOS)
// SearchBuilderResult implementation for AutocompleteMatch::CONTACT results.
class ContactResult : public SearchBuilderResult,
                      public ImageDecoder::Delegate {
 public:
  ContactResult() {}

  virtual ~ContactResult() {
    if (photo_decoder_.get())
      photo_decoder_->set_delegate(NULL);
  }

  // Returns the contact represented by this result, or NULL if the contact
  // can't be found (e.g. it's been deleted in the meantime).  The returned
  // object is only valid until the UI message loop continues running.
  const contacts::Contact* GetContact() const {
    AutocompleteMatch::AdditionalInfo::const_iterator it =
        match().additional_info.find(ContactProvider::kMatchContactIdKey);
    DCHECK(it != match().additional_info.end());
    return contacts::ContactManager::GetInstance()->GetContactById(
        profile(), it->second);
  }

  // Overridden from SearchBuilderResult:
  virtual void Init(Profile* profile, const AutocompleteMatch& match) OVERRIDE {
    SearchBuilderResult::Init(profile, match);

    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    std::vector<app_list::SearchResult::ActionIconSet> icons;
    icons.push_back(
        app_list::SearchResult::ActionIconSet(
            *bundle.GetImageSkiaNamed(IDR_CONTACT_ACTION_CHAT),
            *bundle.GetImageSkiaNamed(IDR_CONTACT_ACTION_CHAT_H),
            *bundle.GetImageSkiaNamed(IDR_CONTACT_ACTION_CHAT),
            l10n_util::GetStringUTF16(IDS_APP_LIST_CONTACT_CHAT_TOOLTIP)));
    icons.push_back(
        app_list::SearchResult::ActionIconSet(
            *bundle.GetImageSkiaNamed(IDR_CONTACT_ACTION_VIDEO),
            *bundle.GetImageSkiaNamed(IDR_CONTACT_ACTION_VIDEO_H),
            *bundle.GetImageSkiaNamed(IDR_CONTACT_ACTION_VIDEO),
            l10n_util::GetStringUTF16(IDS_APP_LIST_CONTACT_VIDEO_TOOLTIP)));
    icons.push_back(
        app_list::SearchResult::ActionIconSet(
            *bundle.GetImageSkiaNamed(IDR_CONTACT_ACTION_PHONE),
            *bundle.GetImageSkiaNamed(IDR_CONTACT_ACTION_PHONE_H),
            *bundle.GetImageSkiaNamed(IDR_CONTACT_ACTION_PHONE),
            l10n_util::GetStringUTF16(IDS_APP_LIST_CONTACT_PHONE_TOOLTIP)));
    icons.push_back(
        app_list::SearchResult::ActionIconSet(
            *bundle.GetImageSkiaNamed(IDR_CONTACT_ACTION_EMAIL),
            *bundle.GetImageSkiaNamed(IDR_CONTACT_ACTION_EMAIL_H),
            *bundle.GetImageSkiaNamed(IDR_CONTACT_ACTION_EMAIL),
            l10n_util::GetStringUTF16(IDS_APP_LIST_CONTACT_EMAIL_TOOLTIP)));
    SetActionIcons(icons);
  }

 protected:
  // Overridden from SearchBuilderResult:
  virtual void UpdateIcon() OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    const contacts::Contact* contact = GetContact();
    if (contact && contact->has_raw_untrusted_photo()) {
      photo_decoder_ =
          new ImageDecoder(
              this,
              contact->raw_untrusted_photo(),
              ImageDecoder::DEFAULT_CODEC);
      scoped_refptr<base::MessageLoopProxy> task_runner =
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI);
      photo_decoder_->Start(task_runner);
    } else {
      SetIcon(
          *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_CONTACT_DEFAULT_PHOTO));
    }
  }

 private:
  // Overridden from ImageDecoder::Delegate:
  virtual void OnImageDecoded(const ImageDecoder* decoder,
                              const SkBitmap& decoded_image) OVERRIDE {
    DCHECK_EQ(decoder, photo_decoder_);
    SetIcon(gfx::ImageSkia::CreateFrom1xBitmap(decoded_image));
  }

  virtual void OnDecodeImageFailed(const ImageDecoder* decoder) OVERRIDE {
    DCHECK_EQ(decoder, photo_decoder_);
    SetIcon(
        *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_CONTACT_DEFAULT_PHOTO));
  }

  scoped_refptr<ImageDecoder> photo_decoder_;

  DISALLOW_COPY_AND_ASSIGN(ContactResult);
};
#endif

}  // namespace

SearchBuilder::SearchBuilder(
    Profile* profile,
    app_list::SearchBoxModel* search_box,
    app_list::AppListModel::SearchResults* results,
    AppListControllerDelegate* list_controller)
    : profile_(profile),
      search_box_(search_box),
      results_(results),
      list_controller_(list_controller) {
  search_box_->SetHintText(
      l10n_util::GetStringUTF16(IDS_SEARCH_BOX_HINT));
  search_box_->SetIcon(*ui::ResourceBundle::GetSharedInstance().
      GetImageSkiaNamed(IDR_OMNIBOX_SEARCH));
  search_box_->SetUserIconEnabled(list_controller->ShouldShowUserIcon());
  search_box_->SetUserIcon(*ui::ResourceBundle::GetSharedInstance().
      GetImageSkiaNamed(IDR_APP_LIST_USER_INDICATOR));
  search_box_->SetUserIconTooltip(UTF8ToUTF16(profile_->GetProfileName()));

  int providers = AutocompleteProvider::TYPE_EXTENSION_APP;
  bool apps_only = true;
#if defined(OS_CHROMEOS)
  apps_only = CommandLine::ForCurrentProcess()->HasSwitch(
      app_list::switches::kAppListShowAppsOnly);
#endif
  if (!apps_only) {
    // TODO(xiyuan): Consider requesting fewer providers in the non-apps-only
    // case.
    providers |= AutocompleteClassifier::kDefaultOmniboxProviders;
  }
#if defined(OS_CHROMEOS)
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kEnableContacts))
    providers |= AutocompleteProvider::TYPE_CONTACT;
#endif
  controller_.reset(new AutocompleteController(profile, this, providers));
}

SearchBuilder::~SearchBuilder() {
}

void SearchBuilder::StartSearch() {
  // Omnibox features such as keyword selection/accepting and instant query
  // are not implemented.
  // TODO(xiyuan): Figure out the features that need to support here.
  controller_->Start(AutocompleteInput(search_box_->text(), string16::npos,
                                       string16(), GURL(), false, false, true,
                                       AutocompleteInput::ALL_MATCHES));
}

void SearchBuilder::StopSearch() {
  controller_->Stop(true);
}

void SearchBuilder::OpenResult(const app_list::SearchResult& result,
                               int event_flags) {
  const SearchBuilderResult* builder_result =
      static_cast<const SearchBuilderResult*>(&result);
  const AutocompleteMatch& match = builder_result->match();

  // Count AppList.Search here because it is composed of search + action.
  content::RecordAction(content::UserMetricsAction("AppList_Search"));

  if (match.type == AutocompleteMatch::EXTENSION_APP) {
    const extensions::Extension* extension =
        GetExtensionByURL(profile_, match.destination_url);
    if (extension) {
      AppLauncherHandler::RecordAppLaunchType(
        extension_misc::APP_LAUNCH_APP_LIST_SEARCH);
      content::RecordAction(
          content::UserMetricsAction("AppList_ClickOnAppFromSearch"));
      list_controller_->ActivateApp(profile_, extension, event_flags);
    }
#if defined(OS_CHROMEOS)
  } else if (match.type == AutocompleteMatch::CONTACT) {
    // Pass 0 for |action_index| to trigger a chat request.
    InvokeResultAction(result, 0, event_flags);
#endif
  } else {
    // TODO(xiyuan): What should we do for alternate url case?
    chrome::NavigateParams params(profile_,
                                  match.destination_url,
                                  match.transition);
    params.disposition = ui::DispositionFromEventFlags(event_flags);
    chrome::Navigate(&params);
  }
}

void SearchBuilder::InvokeResultAction(const app_list::SearchResult& result,
                                       int action_index,
                                       int event_flags) {
#if defined(OS_CHROMEOS)
  const SearchBuilderResult* builder_result =
      static_cast<const SearchBuilderResult*>(&result);
  const AutocompleteMatch& match = builder_result->match();

  if (match.type == AutocompleteMatch::CONTACT) {
    const contacts::Contact* contact =
        static_cast<const ContactResult*>(builder_result)->GetContact();
    if (!contact)
      return;

    // Cases match the order in which actions were added in
    // ContactResult::Init().
    extensions::RtcPrivateEventRouter::LaunchAction launch_action =
        extensions::RtcPrivateEventRouter::LAUNCH_CHAT;
    switch (action_index) {
      case 0:
        launch_action = extensions::RtcPrivateEventRouter::LAUNCH_CHAT;
        break;
      case 1:
        launch_action = extensions::RtcPrivateEventRouter::LAUNCH_VIDEO;
        break;
      case 2:
        launch_action = extensions::RtcPrivateEventRouter::LAUNCH_VOICE;
        break;
      case 3:
        // TODO(derat): Send email.
        NOTIMPLEMENTED();
        break;
      default:
        NOTREACHED() << "Unexpected action index " << action_index;
    }

    extensions::RtcPrivateEventRouter::DispatchLaunchEvent(
        builder_result->profile(), launch_action, contact);
    return;
  }
#endif

  NOTIMPLEMENTED();
}

void SearchBuilder::PopulateFromACResult(const AutocompleteResult& ac_result) {
  results_->DeleteAll();
  for (ACMatches::const_iterator it = ac_result.begin();
       it != ac_result.end();
       ++it) {
    SearchBuilderResult* result = NULL;
    switch (it->type) {
      case AutocompleteMatch::EXTENSION_APP:
        result = new ExtensionAppResult();
        break;
#if defined(OS_CHROMEOS)
      case AutocompleteMatch::CONTACT:
        result = new ContactResult();
        break;
#endif
      default:
        result = new SearchBuilderResult();
    }
    result->Init(profile_, *it);
    results_->Add(result);
  }
}

void SearchBuilder::OnResultChanged(bool default_match_changed) {
  // TODO(xiyuan): Handle default match properly.
  const AutocompleteResult& ac_result = controller_->result();
  PopulateFromACResult(ac_result);
}
