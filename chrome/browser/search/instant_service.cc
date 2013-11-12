// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/instant_service.h"

#include <vector>

#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/most_visited_tiles_experiment.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_io_context.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/search/local_ntp_source.h"
#include "chrome/browser/search/most_visited_iframe_source.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/ntp/thumbnail_list_source.h"
#include "chrome/browser/ui/webui/ntp/thumbnail_source.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/url_data_source.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/sys_color_change_listener.h"
#include "url/gurl.h"

using content::BrowserThread;

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

InstantService::InstantService(Profile* profile)
    : profile_(profile),
      ntp_prerenderer_(profile, this, profile->GetPrefs()),
      browser_instant_controller_object_count_(0),
      weak_ptr_factory_(this) {
  // Stub for unit tests.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI))
    return;

  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::NotificationService::AllSources());

  history::TopSites* top_sites = profile_->GetTopSites();
  if (top_sites) {
    registrar_.Add(this,
                   chrome::NOTIFICATION_TOP_SITES_CHANGED,
                   content::Source<history::TopSites>(top_sites));
  }
  instant_io_context_ = new InstantIOContext();

  if (profile_ && profile_->GetResourceContext()) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&InstantIOContext::SetUserDataOnIO,
                   profile->GetResourceContext(), instant_io_context_));
  }

  // Set up the data sources that Instant uses on the NTP.
#if defined(ENABLE_THEMES)
  // Listen for theme installation.
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(
                     ThemeServiceFactory::GetForProfile(profile_)));

  content::URLDataSource::Add(profile_, new ThemeSource(profile_));
#endif  // defined(ENABLE_THEMES)

  content::URLDataSource::Add(profile_, new ThumbnailSource(profile_, false));
  content::URLDataSource::Add(profile_, new ThumbnailSource(profile_, true));
  content::URLDataSource::Add(profile_, new ThumbnailListSource(profile_));
  content::URLDataSource::Add(
      profile_, new FaviconSource(profile_, FaviconSource::FAVICON));
  content::URLDataSource::Add(profile_, new LocalNtpSource(profile_));
  content::URLDataSource::Add(profile_, new MostVisitedIframeSource());

  profile_pref_registrar_.Init(profile_->GetPrefs());
  profile_pref_registrar_.Add(
      prefs::kDefaultSearchProviderID,
      base::Bind(&InstantService::OnDefaultSearchProviderChanged,
                 base::Unretained(this)));

  registrar_.Add(this, chrome::NOTIFICATION_GOOGLE_URL_UPDATED,
                 content::Source<Profile>(profile_->GetOriginalProfile()));
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::Source<Profile>(profile_));
}

InstantService::~InstantService() {
}

void InstantService::AddInstantProcess(int process_id) {
  process_ids_.insert(process_id);

  if (instant_io_context_.get()) {
    BrowserThread::PostTask(BrowserThread::IO,
                            FROM_HERE,
                            base::Bind(&InstantIOContext::AddInstantProcessOnIO,
                                       instant_io_context_,
                                       process_id));
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

void InstantService::DeleteMostVisitedItem(const GURL& url) {
  history::TopSites* top_sites = profile_->GetTopSites();
  if (!top_sites)
    return;

  top_sites->AddBlacklistedURL(url);
}

void InstantService::UndoMostVisitedDeletion(const GURL& url) {
  history::TopSites* top_sites = profile_->GetTopSites();
  if (!top_sites)
    return;

  top_sites->RemoveBlacklistedURL(url);
}

void InstantService::UndoAllMostVisitedDeletions() {
  history::TopSites* top_sites = profile_->GetTopSites();
  if (!top_sites)
    return;

  top_sites->ClearBlacklistedURLs();
}

void InstantService::UpdateThemeInfo() {
  // Update theme background info.
  // Initialize |theme_info| if necessary.
  if (!theme_info_)
    OnThemeChanged(ThemeServiceFactory::GetForProfile(profile_));
  else
    OnThemeChanged(NULL);
}

void InstantService::UpdateMostVisitedItemsInfo() {
  NotifyAboutMostVisitedItems();
}

void InstantService::Shutdown() {
  process_ids_.clear();

  if (instant_io_context_.get()) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&InstantIOContext::ClearInstantProcessesOnIO,
                   instant_io_context_));
  }
  instant_io_context_ = NULL;
}

scoped_ptr<content::WebContents> InstantService::ReleaseNTPContents() {
  return ntp_prerenderer_.ReleaseNTPContents();
}

content::WebContents* InstantService::GetNTPContents() const {
  return ntp_prerenderer_.GetNTPContents();
}

void InstantService::OnBrowserInstantControllerCreated() {
  if (profile_->IsOffTheRecord())
    return;

  ++browser_instant_controller_object_count_;

  if (browser_instant_controller_object_count_ == 1)
    ntp_prerenderer_.ReloadInstantNTP();
}

void InstantService::OnBrowserInstantControllerDestroyed() {
  if (profile_->IsOffTheRecord())
    return;

  DCHECK_GT(browser_instant_controller_object_count_, 0U);
  --browser_instant_controller_object_count_;

  // All browser windows have closed, so release the InstantNTP resources to
  // work around http://crbug.com/180810.
  if (browser_instant_controller_object_count_ == 0)
    ntp_prerenderer_.DeleteNTPContents();
}

void InstantService::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_CREATED:
      SendSearchURLsToRenderer(
          content::Source<content::RenderProcessHost>(source).ptr());
      break;
    case content::NOTIFICATION_RENDERER_PROCESS_TERMINATED:
      OnRendererProcessTerminated(
          content::Source<content::RenderProcessHost>(source)->GetID());
      break;
    case chrome::NOTIFICATION_TOP_SITES_CHANGED: {
      history::TopSites* top_sites = profile_->GetTopSites();
      if (top_sites) {
        top_sites->GetMostVisitedURLs(
            base::Bind(&InstantService::OnMostVisitedItemsReceived,
                       weak_ptr_factory_.GetWeakPtr()), false);
      }
      break;
    }
#if defined(ENABLE_THEMES)
    case chrome::NOTIFICATION_BROWSER_THEME_CHANGED: {
      OnThemeChanged(content::Source<ThemeService>(source).ptr());
      break;
    }
#endif  // defined(ENABLE_THEMES)
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      // Last chance to delete InstantNTP contents. We generally delete
      // preloaded InstantNTP when the last BrowserInstantController object is
      // destroyed. When the browser shutdown happens without closing browsers,
      // there is a race condition between BrowserInstantController destruction
      // and Profile destruction.
      if (GetNTPContents())
        ntp_prerenderer_.DeleteNTPContents();
      break;
    }
    case chrome::NOTIFICATION_GOOGLE_URL_UPDATED: {
      OnGoogleURLUpdated(
          content::Source<Profile>(source).ptr(),
          content::Details<GoogleURLTracker::UpdatedDetails>(details).ptr());
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification type in InstantService.";
  }
}

void InstantService::SendSearchURLsToRenderer(content::RenderProcessHost* rph) {
  rph->Send(new ChromeViewMsg_SetSearchURLs(
      chrome::GetSearchURLs(profile_), chrome::GetNewTabPageURL(profile_)));
}

void InstantService::OnRendererProcessTerminated(int process_id) {
  process_ids_.erase(process_id);

  if (instant_io_context_.get()) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&InstantIOContext::RemoveInstantProcessOnIO,
                   instant_io_context_,
                   process_id));
  }
}

void InstantService::OnMostVisitedItemsReceived(
    const history::MostVisitedURLList& data) {
  history::MostVisitedURLList reordered_data(data);
  history::MostVisitedTilesExperiment::MaybeShuffle(&reordered_data);

  std::vector<InstantMostVisitedItem> new_most_visited_items;
  for (size_t i = 0; i < reordered_data.size(); i++) {
    const history::MostVisitedURL& url = reordered_data[i];
    InstantMostVisitedItem item;
    item.url = url.url;
    item.title = url.title;
    new_most_visited_items.push_back(item);
  }

  most_visited_items_ = new_most_visited_items;
  NotifyAboutMostVisitedItems();
}

void InstantService::NotifyAboutMostVisitedItems() {
  FOR_EACH_OBSERVER(InstantServiceObserver, observers_,
                    MostVisitedItemsChanged(most_visited_items_));
}

void InstantService::OnThemeChanged(ThemeService* theme_service) {
  if (!theme_service) {
    DCHECK(theme_info_.get());
    FOR_EACH_OBSERVER(InstantServiceObserver, observers_,
                      ThemeInfoChanged(*theme_info_));
    return;
  }

  // Get theme information from theme service.
  theme_info_.reset(new ThemeBackgroundInfo());

  // Get if the current theme is the default theme.
  theme_info_->using_default_theme = theme_service->UsingDefaultTheme();

  // Get theme colors.
  SkColor background_color =
      theme_service->GetColor(ThemeProperties::COLOR_NTP_BACKGROUND);
  SkColor text_color =
      theme_service->GetColor(ThemeProperties::COLOR_NTP_TEXT);
  SkColor link_color =
      theme_service->GetColor(ThemeProperties::COLOR_NTP_LINK);
  SkColor text_color_light =
      theme_service->GetColor(ThemeProperties::COLOR_NTP_TEXT_LIGHT);
  SkColor header_color =
      theme_service->GetColor(ThemeProperties::COLOR_NTP_HEADER);
  // Generate section border color from the header color.
  SkColor section_border_color =
      SkColorSetARGB(kSectionBorderAlphaTransparency,
                     SkColorGetR(header_color),
                     SkColorGetG(header_color),
                     SkColorGetB(header_color));

  // Invert colors if needed.
  if (gfx::IsInvertedColorScheme()) {
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

  int logo_alternate = theme_service->GetDisplayProperty(
      ThemeProperties::NTP_LOGO_ALTERNATE);
  theme_info_->logo_alternate = logo_alternate == 1;

  if (theme_service->HasCustomImage(IDR_THEME_NTP_BACKGROUND)) {
    // Set theme id for theme background image url.
    theme_info_->theme_id = theme_service->GetThemeID();

    // Set theme background image horizontal alignment.
    int alignment = theme_service->GetDisplayProperty(
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

    // Set theme backgorund image tiling.
    int tiling = theme_service->GetDisplayProperty(
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

    // Set theme background image height.
    gfx::ImageSkia* image = theme_service->GetImageSkiaNamed(
        IDR_THEME_NTP_BACKGROUND);
    DCHECK(image);
    theme_info_->image_height = image->height();

    theme_info_->has_attribution =
       theme_service->HasCustomImage(IDR_THEME_NTP_ATTRIBUTION);
  }

  FOR_EACH_OBSERVER(InstantServiceObserver, observers_,
                    ThemeInfoChanged(*theme_info_));
}

void InstantService::OnGoogleURLUpdated(
    Profile* profile,
    GoogleURLTracker::UpdatedDetails* details) {
  GURL last_prompted_url(
      profile->GetPrefs()->GetString(prefs::kLastPromptedGoogleURL));

  // See GoogleURLTracker::OnURLFetchComplete().
  // last_prompted_url.is_empty() indicates very first run of Chrome. So there
  // is no need to notify, as there won't be any old state.
  if (last_prompted_url.is_empty())
    return;

  // Only the scheme changed. Ignore it since we do not prompt the user in this
  // case.
  if (net::StripWWWFromHost(details->first) ==
      net::StripWWWFromHost(details->second))
    return;

  FOR_EACH_OBSERVER(InstantServiceObserver, observers_, GoogleURLUpdated());
}

void InstantService::OnDefaultSearchProviderChanged(
    const std::string& pref_name) {
  DCHECK_EQ(pref_name, std::string(prefs::kDefaultSearchProviderID));
  const TemplateURL* template_url = TemplateURLServiceFactory::GetForProfile(
      profile_)->GetDefaultSearchProvider();
  if (!template_url) {
    // A NULL |template_url| could mean either this notification is sent during
    // the browser start up operation or the user now has no default search
    // provider. There is no way for the user to reach this state using the
    // Chrome settings. Only explicitly poking at the DB or bugs in the Sync
    // could cause that, neither of which we support.
    return;
  }
  FOR_EACH_OBSERVER(
      InstantServiceObserver, observers_, DefaultSearchProviderChanged());
}

InstantNTPPrerenderer* InstantService::ntp_prerenderer() {
  return &ntp_prerenderer_;
}
