// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/instant_service.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ntp_tiles/chrome_most_visited_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_io_context.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/search/local_ntp_source.h"
#include "chrome/browser/search/most_visited_iframe_source.h"
#include "chrome/browser/search/ntp_features.h"
#include "chrome/browser/search/ntp_icon_source.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search/thumbnail_source.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/thumbnails/thumbnail_list_source.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/search.mojom.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/theme_resources.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search/search.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/url_data_source.h"
#include "ui/gfx/color_utils.h"

namespace {

const char* kNtpCustomBackgroundURL = "background_url";
const char* kNtpCustomBackgroundAttributionLine1 = "attribution_line_1";
const char* kNtpCustomBackgroundAttributionLine2 = "attribution_line_2";
const char* kNtpCustomBackgroundAttributionActionURL = "attribution_action_url";

base::DictionaryValue GetBackgroundInfoAsDict(
    const GURL& background_url,
    const std::string& attribution_line_1,
    const std::string& attribution_line_2,
    const GURL& action_url) {
  base::DictionaryValue background_info;
  background_info.SetKey(kNtpCustomBackgroundURL,
                         base::Value(background_url.spec()));
  background_info.SetKey(kNtpCustomBackgroundAttributionLine1,
                         base::Value(attribution_line_1));
  background_info.SetKey(kNtpCustomBackgroundAttributionLine2,
                         base::Value(attribution_line_2));
  background_info.SetKey(kNtpCustomBackgroundAttributionActionURL,
                         base::Value(action_url.spec()));

  return background_info;
}

std::unique_ptr<base::DictionaryValue> NtpCustomBackgroundDefaults() {
  std::unique_ptr<base::DictionaryValue> defaults =
      std::make_unique<base::DictionaryValue>();
  defaults->SetString(kNtpCustomBackgroundURL, std::string());
  defaults->SetString(kNtpCustomBackgroundAttributionLine1, std::string());
  defaults->SetString(kNtpCustomBackgroundAttributionLine2, std::string());
  defaults->SetString(kNtpCustomBackgroundAttributionActionURL, std::string());
  return defaults;
}

void CopyFileToProfilePath(const base::FilePath& from_path) {
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  base::FilePath to_path(user_data_dir.AppendASCII(
      chrome::kChromeSearchLocalNtpBackgroundFilename));
  base::CopyFile(from_path, to_path);
}

void RemoveLocalBackgroundImageCopy() {
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  base::FilePath path(user_data_dir.AppendASCII(
      chrome::kChromeSearchLocalNtpBackgroundFilename));
  base::DeleteFile(path, false);
}

}  // namespace

InstantService::InstantService(Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {
  // The initialization below depends on a typical set of browser threads. Skip
  // it if we are running in a unit test without the full suite.
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI))
    return;

  // This depends on the existence of the typical browser threads. Therefore it
  // is only instantiated here (after the check for a UI thread above).
  instant_io_context_ = new InstantIOContext();

  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::NotificationService::AllSources());

  most_visited_sites_ = ChromeMostVisitedSitesFactory::NewForProfile(profile_);
  if (most_visited_sites_) {
    // 9 tiles are required for the custom links feature in order to balance the
    // Most Visited rows (this is due to an additional "Add" button). Otherwise,
    // Most Visited should return the regular 8 tiles.
    most_visited_sites_->SetMostVisitedURLsObserver(
        this, features::IsCustomLinksEnabled() ? 9 : 8);
  }

  if (profile_ && profile_->GetResourceContext()) {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(&InstantIOContext::SetUserDataOnIO,
                       profile->GetResourceContext(), instant_io_context_));
  }

  // Listen for theme installation.
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(
                     ThemeServiceFactory::GetForProfile(profile_)));

  // Set up the data sources that Instant uses on the NTP.
  content::URLDataSource::Add(profile_, new ThemeSource(profile_));
  content::URLDataSource::Add(profile_, new LocalNtpSource(profile_));
  content::URLDataSource::Add(profile_, new NtpIconSource(profile_));
  content::URLDataSource::Add(profile_, new ThumbnailSource(profile_, false));
  content::URLDataSource::Add(profile_, new ThumbnailSource(profile_, true));
  content::URLDataSource::Add(profile_, new ThumbnailListSource(profile_));
  content::URLDataSource::Add(profile_, new FaviconSource(profile_));
  content::URLDataSource::Add(profile_, new MostVisitedIframeSource());
}

InstantService::~InstantService() = default;

void InstantService::AddInstantProcess(int process_id) {
  process_ids_.insert(process_id);

  if (instant_io_context_.get()) {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(&InstantIOContext::AddInstantProcessOnIO,
                       instant_io_context_, process_id));
  }
}

bool InstantService::IsInstantProcess(int process_id) const {
  return process_ids_.find(process_id) != process_ids_.end();
}

void InstantService::AddObserver(InstantServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void InstantService::RemoveObserver(InstantServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void InstantService::OnNewTabPageOpened() {
  if (most_visited_sites_) {
    most_visited_sites_->Refresh();
  }
}

void InstantService::DeleteMostVisitedItem(const GURL& url) {
  if (most_visited_sites_) {
    most_visited_sites_->AddOrRemoveBlacklistedUrl(url, true);
  }
}

void InstantService::UndoMostVisitedDeletion(const GURL& url) {
  if (most_visited_sites_) {
    most_visited_sites_->AddOrRemoveBlacklistedUrl(url, false);
  }
}

void InstantService::UndoAllMostVisitedDeletions() {
  if (most_visited_sites_) {
    most_visited_sites_->ClearBlacklistedUrls();
  }
}

bool InstantService::AddCustomLink(const GURL& url, const std::string& title) {
  if (most_visited_sites_) {
    // Initializes custom links if they have not been initialized yet.
    most_visited_sites_->InitializeCustomLinks();
    return most_visited_sites_->AddCustomLink(url, base::UTF8ToUTF16(title));
  }
  return false;
}

bool InstantService::UpdateCustomLink(const GURL& url,
                                      const GURL& new_url,
                                      const std::string& new_title) {
  if (most_visited_sites_) {
    // Initializes custom links if they have not been initialized yet.
    most_visited_sites_->InitializeCustomLinks();
    return most_visited_sites_->UpdateCustomLink(url, new_url,
                                                 base::UTF8ToUTF16(new_title));
  }
  return false;
}

bool InstantService::DeleteCustomLink(const GURL& url) {
  if (most_visited_sites_) {
    // Initializes custom links if they have not been initialized yet.
    most_visited_sites_->InitializeCustomLinks();
    return most_visited_sites_->DeleteCustomLink(url);
  }
  return false;
}

bool InstantService::UndoCustomLinkAction() {
  // Non-Google search providers are not supported.
  if (most_visited_sites_ && search::DefaultSearchProviderIsGoogle(profile_)) {
    most_visited_sites_->UndoCustomLinkAction();
    return true;
  }
  return false;
}

bool InstantService::ResetCustomLinks() {
  // Non-Google search providers are not supported.
  if (most_visited_sites_ && search::DefaultSearchProviderIsGoogle(profile_)) {
    most_visited_sites_->UninitializeCustomLinks();
    return true;
  }
  return false;
}

void InstantService::UpdateThemeInfo() {
  // Initialize |theme_info_| if necessary.
  if (!theme_info_) {
    BuildThemeInfo();
  }

  ApplyOrResetCustomBackgroundThemeInfo();

  NotifyAboutThemeInfo();
}

void InstantService::UpdateMostVisitedItemsInfo() {
  NotifyAboutMostVisitedItems();
}

void InstantService::SendNewTabPageURLToRenderer(
    content::RenderProcessHost* rph) {
  if (auto* channel = rph->GetChannel()) {
    chrome::mojom::SearchBouncerAssociatedPtr client;
    channel->GetRemoteAssociatedInterface(&client);
    client->SetNewTabPageURL(search::GetNewTabPageURL(profile_));
  }
}

void InstantService::SetCustomBackgroundURL(const GURL& url) {
  SetCustomBackgroundURLWithAttributions(url, std::string(), std::string(),
                                         GURL());
}

void InstantService::SetCustomBackgroundURLWithAttributions(
    const GURL& background_url,
    const std::string& attribution_line_1,
    const std::string& attribution_line_2,
    const GURL& action_url) {
  PrefService* pref_service = profile_->GetPrefs();

  if (background_url.is_valid()) {
    base::DictionaryValue background_info = GetBackgroundInfoAsDict(
        background_url, attribution_line_1, attribution_line_2, action_url);
    pref_service->Set(prefs::kNtpCustomBackgroundDict, background_info);
  } else {
    pref_service->ClearPref(prefs::kNtpCustomBackgroundDict);
  }

  UpdateThemeInfo();
}

void InstantService::SetBackgroundToLocalResource() {
  // Add a timestamp to the url to prevent the browser from using a cached
  // version when "Upload an image" is used multiple times.
  std::string time_string = std::to_string(base::Time::Now().ToTimeT());
  std::string local_string(chrome::kChromeSearchLocalNtpBackgroundUrl);
  GURL timestamped_url(local_string + "?ts=" + time_string);

  base::DictionaryValue background_info = GetBackgroundInfoAsDict(
      timestamped_url, std::string(), std::string(), GURL());
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->Set(prefs::kNtpCustomBackgroundDict, background_info);
  UpdateThemeInfo();
}

void InstantService::SelectLocalBackgroundImage(const base::FilePath& path) {
  base::PostTaskWithTraitsAndReply(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&CopyFileToProfilePath, path),
      base::BindOnce(&InstantService::SetBackgroundToLocalResource,
                     weak_ptr_factory_.GetWeakPtr()));
}

void InstantService::Shutdown() {
  process_ids_.clear();

  if (instant_io_context_.get()) {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(&InstantIOContext::ClearInstantProcessesOnIO,
                       instant_io_context_));
  }

  if (most_visited_sites_) {
    most_visited_sites_.reset();
  }

  instant_io_context_ = NULL;
}

void InstantService::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_CREATED:
      SendNewTabPageURLToRenderer(
          content::Source<content::RenderProcessHost>(source).ptr());
      break;
    case content::NOTIFICATION_RENDERER_PROCESS_TERMINATED:
      OnRendererProcessTerminated(
          content::Source<content::RenderProcessHost>(source)->GetID());
      break;
    case chrome::NOTIFICATION_BROWSER_THEME_CHANGED:
      BuildThemeInfo();
      NotifyAboutThemeInfo();
      break;
    default:
      NOTREACHED() << "Unexpected notification type in InstantService.";
  }
}

void InstantService::OnRendererProcessTerminated(int process_id) {
  process_ids_.erase(process_id);

  if (instant_io_context_.get()) {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::BindOnce(&InstantIOContext::RemoveInstantProcessOnIO,
                       instant_io_context_, process_id));
  }
}

void InstantService::OnURLsAvailable(
    const std::map<ntp_tiles::SectionType, ntp_tiles::NTPTilesVector>&
        sections) {
  DCHECK(most_visited_sites_);
  most_visited_items_.clear();
  // Use only personalized tiles for instant service.
  const ntp_tiles::NTPTilesVector& tiles =
      sections.at(ntp_tiles::SectionType::PERSONALIZED);
  for (const ntp_tiles::NTPTile& tile : tiles) {
    InstantMostVisitedItem item;
    item.url = tile.url;
    item.title = tile.title;
    item.thumbnail = tile.thumbnail_url;
    item.favicon = tile.favicon_url;
    item.source = tile.source;
    item.title_source = tile.title_source;
    item.data_generation_time = tile.data_generation_time;
    most_visited_items_.push_back(item);
  }

  NotifyAboutMostVisitedItems();
}

void InstantService::OnIconMadeAvailable(const GURL& site_url) {}

void InstantService::NotifyAboutMostVisitedItems() {
  for (InstantServiceObserver& observer : observers_)
    observer.MostVisitedItemsChanged(most_visited_items_);
}

void InstantService::NotifyAboutThemeInfo() {
  for (InstantServiceObserver& observer : observers_)
    observer.ThemeInfoChanged(*theme_info_);
}

namespace {

const int kSectionBorderAlphaTransparency = 80;

// Converts SkColor to RGBAColor
RGBAColor SkColorToRGBAColor(const SkColor& sKColor) {
  RGBAColor color;
  color.r = SkColorGetR(sKColor);
  color.g = SkColorGetG(sKColor);
  color.b = SkColorGetB(sKColor);
  color.a = SkColorGetA(sKColor);
  return color;
}

}  // namespace

void InstantService::BuildThemeInfo() {
  // Get theme information from theme service.
  theme_info_.reset(new ThemeBackgroundInfo());

  // Get if the current theme is the default theme (or GTK+ on linux).
  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(profile_);
  theme_info_->using_default_theme =
      theme_service->UsingDefaultTheme() || theme_service->UsingSystemTheme();

  // Get theme colors.
  const ui::ThemeProvider& theme_provider =
      ThemeService::GetThemeProviderForProfile(profile_);
  SkColor background_color =
      theme_provider.GetColor(ThemeProperties::COLOR_NTP_BACKGROUND);
  SkColor text_color = theme_provider.GetColor(ThemeProperties::COLOR_NTP_TEXT);
  SkColor link_color = theme_provider.GetColor(ThemeProperties::COLOR_NTP_LINK);
  SkColor text_color_light =
      theme_provider.GetColor(ThemeProperties::COLOR_NTP_TEXT_LIGHT);
  SkColor header_color =
      theme_provider.GetColor(ThemeProperties::COLOR_NTP_HEADER);
  // Generate section border color from the header color.
  SkColor section_border_color =
      SkColorSetARGB(kSectionBorderAlphaTransparency,
                     SkColorGetR(header_color),
                     SkColorGetG(header_color),
                     SkColorGetB(header_color));

  // Invert colors if needed.
  if (color_utils::IsInvertedColorScheme()) {
    background_color = color_utils::InvertColor(background_color);
    text_color = color_utils::InvertColor(text_color);
    link_color = color_utils::InvertColor(link_color);
    text_color_light = color_utils::InvertColor(text_color_light);
    header_color = color_utils::InvertColor(header_color);
    section_border_color = color_utils::InvertColor(section_border_color);
  }

  // Set colors.
  theme_info_->background_color = SkColorToRGBAColor(background_color);
  theme_info_->text_color = SkColorToRGBAColor(text_color);
  theme_info_->link_color = SkColorToRGBAColor(link_color);
  theme_info_->text_color_light = SkColorToRGBAColor(text_color_light);
  theme_info_->header_color = SkColorToRGBAColor(header_color);
  theme_info_->section_border_color = SkColorToRGBAColor(section_border_color);

  int logo_alternate =
      theme_provider.GetDisplayProperty(ThemeProperties::NTP_LOGO_ALTERNATE);
  theme_info_->logo_alternate = logo_alternate == 1;

  if (theme_provider.HasCustomImage(IDR_THEME_NTP_BACKGROUND)) {
    // Set theme id for theme background image url.
    theme_info_->theme_id = theme_service->GetThemeID();

    // Set theme background image horizontal alignment.
    int alignment = theme_provider.GetDisplayProperty(
        ThemeProperties::NTP_BACKGROUND_ALIGNMENT);
    if (alignment & ThemeProperties::ALIGN_LEFT)
      theme_info_->image_horizontal_alignment = THEME_BKGRND_IMAGE_ALIGN_LEFT;
    else if (alignment & ThemeProperties::ALIGN_RIGHT)
      theme_info_->image_horizontal_alignment = THEME_BKGRND_IMAGE_ALIGN_RIGHT;
    else
      theme_info_->image_horizontal_alignment = THEME_BKGRND_IMAGE_ALIGN_CENTER;

    // Set theme background image vertical alignment.
    if (alignment & ThemeProperties::ALIGN_TOP)
      theme_info_->image_vertical_alignment = THEME_BKGRND_IMAGE_ALIGN_TOP;
    else if (alignment & ThemeProperties::ALIGN_BOTTOM)
      theme_info_->image_vertical_alignment = THEME_BKGRND_IMAGE_ALIGN_BOTTOM;
    else
      theme_info_->image_vertical_alignment = THEME_BKGRND_IMAGE_ALIGN_CENTER;

    // Set theme background image tiling.
    int tiling = theme_provider.GetDisplayProperty(
        ThemeProperties::NTP_BACKGROUND_TILING);
    switch (tiling) {
      case ThemeProperties::NO_REPEAT:
        theme_info_->image_tiling = THEME_BKGRND_IMAGE_NO_REPEAT;
        break;
      case ThemeProperties::REPEAT_X:
        theme_info_->image_tiling = THEME_BKGRND_IMAGE_REPEAT_X;
        break;
      case ThemeProperties::REPEAT_Y:
        theme_info_->image_tiling = THEME_BKGRND_IMAGE_REPEAT_Y;
        break;
      case ThemeProperties::REPEAT:
        theme_info_->image_tiling = THEME_BKGRND_IMAGE_REPEAT;
        break;
    }

    theme_info_->has_attribution =
        theme_provider.HasCustomImage(IDR_THEME_NTP_ATTRIBUTION);
  }
}

// TODO(crbug.com/863942): Should switching default search provider retain the
// copy of user uploaded photos?
void InstantService::ApplyOrResetCustomBackgroundThemeInfo() {
  // Reset the pref if the feature is disabled.
  if (!features::IsCustomBackgroundsEnabled()) {
    ResetCustomBackgroundThemeInfo();
    return;
  }

  // Custom backgrounds for non-Google search providers are not supported.
  if (!search::DefaultSearchProviderIsGoogle(profile_)) {
    ResetCustomBackgroundThemeInfo();
    return;
  }

  // Attempt to get custom background URL from preferences.
  const base::DictionaryValue* background_info =
      profile_->GetPrefs()->GetDictionary(prefs::kNtpCustomBackgroundDict);
  const base::Value* background_url =
      background_info->FindKey(kNtpCustomBackgroundURL);
  if (!background_url) {
    ResetCustomBackgroundThemeInfo();
    return;
  }

  // Verify that the custom background URL is valid.
  GURL custom_background_url(
      background_info->FindKey(kNtpCustomBackgroundURL)->GetString());
  if (!custom_background_url.is_valid()) {
    ResetCustomBackgroundThemeInfo();
    return;
  }

  // Set custom background information in theme info (attributions are
  // optional).
  const base::Value* attribution_line_1 =
      background_info->FindKey(kNtpCustomBackgroundAttributionLine1);
  const base::Value* attribution_line_2 =
      background_info->FindKey(kNtpCustomBackgroundAttributionLine2);
  const base::Value* attribution_action_url =
      background_info->FindKey(kNtpCustomBackgroundAttributionActionURL);

  theme_info_->custom_background_url = custom_background_url;

  if (attribution_line_1) {
    theme_info_->custom_background_attribution_line_1 =
        background_info->FindKey(kNtpCustomBackgroundAttributionLine1)
            ->GetString();
  }
  if (attribution_line_2) {
    theme_info_->custom_background_attribution_line_2 =
        background_info->FindKey(kNtpCustomBackgroundAttributionLine2)
            ->GetString();
  }
  if (attribution_action_url) {
    GURL action_url(
        background_info->FindKey(kNtpCustomBackgroundAttributionActionURL)
            ->GetString());

    if (!action_url.SchemeIsCryptographic()) {
      theme_info_->custom_background_attribution_action_url = GURL();
    } else {
      theme_info_->custom_background_attribution_action_url = action_url;
    }
  }
}

void InstantService::ResetCustomBackgroundThemeInfo() {
  profile_->GetPrefs()->ClearPref(prefs::kNtpCustomBackgroundDict);
  base::PostTaskWithTraits(FROM_HERE,
                           {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
                           base::BindOnce(&RemoveLocalBackgroundImageCopy));

  theme_info_->custom_background_url = GURL();
  theme_info_->custom_background_attribution_line_1 = std::string();
  theme_info_->custom_background_attribution_line_2 = std::string();
  theme_info_->custom_background_attribution_action_url = GURL();
}

// static
void InstantService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kNtpCustomBackgroundDict,
                                   NtpCustomBackgroundDefaults());
}
