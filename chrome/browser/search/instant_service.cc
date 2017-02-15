// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/instant_service.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/favicon/fallback_icon_service_factory.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/large_icon_service_factory.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/ntp_tiles/chrome_most_visited_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_io_context.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/search/most_visited_iframe_source.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search/suggestions/image_decoder_impl.h"
#include "chrome/browser/search/suggestions/suggestions_service_factory.h"
#include "chrome/browser/search/thumbnail_source.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/thumbnails/thumbnail_list_source.h"
#include "chrome/browser/ui/search/instant_search_prerenderer.h"
#include "chrome/browser/ui/webui/fallback_icon_source.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/large_icon_source.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/render_messages.h"
#include "chrome/grit/theme_resources.h"
#include "components/favicon/core/fallback_icon_service.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/history/core/browser/top_sites.h"
#include "components/image_fetcher/image_fetcher_impl.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/ntp_tiles/icon_cacher.h"
#include "components/search/search.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/url_data_source.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/search/local_ntp_source.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#endif

namespace {

const base::Feature kNtpTilesFeature{"NTPTilesInInstantService",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace

InstantService::InstantService(Profile* profile)
    : profile_(profile),
      weak_ptr_factory_(this) {
  // The initialization below depends on a typical set of browser threads. Skip
  // it if we are running in a unit test without the full suite.
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI))
    return;

  // This depends on the existence of the typical browser threads. Therefore it
  // is only instantiated here (after the check for a UI thread above).
  instant_io_context_ = new InstantIOContext();

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  // TemplateURLService can be null in tests.
  if (template_url_service) {
    search_engine_base_url_tracker_ =
        base::MakeUnique<SearchEngineBaseURLTracker>(
            template_url_service,
            base::MakeUnique<UIThreadSearchTermsData>(profile_),
            base::Bind(&InstantService::OnSearchEngineBaseURLChanged,
                       base::Unretained(this)));
  }

  ResetInstantSearchPrerendererIfNecessary();

  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::NotificationService::AllSources());

  if (base::FeatureList::IsEnabled(kNtpTilesFeature)) {
    most_visited_sites_ =
        ChromeMostVisitedSitesFactory::NewForProfile(profile_);
    if (most_visited_sites_)
      most_visited_sites_->SetMostVisitedURLsObserver(this, 8);
  } else {
    top_sites_ = TopSitesFactory::GetForProfile(profile_);
    if (top_sites_) {
      top_sites_->AddObserver(this);
      // Immediately query the TopSites state.
      TopSitesChanged(top_sites_.get(),
                      history::TopSitesObserver::ChangeReason::MOST_VISITED);
    }
  }

  if (profile_ && profile_->GetResourceContext()) {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&InstantIOContext::SetUserDataOnIO,
                   profile->GetResourceContext(), instant_io_context_));
  }

  // Set up the data sources that Instant uses on the NTP.
  // TODO(aurimas) remove this #if once instant_service.cc is no longer compiled
  // on Android.
#if !defined(OS_ANDROID)
  // Listen for theme installation.
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(
                     ThemeServiceFactory::GetForProfile(profile_)));

  content::URLDataSource::Add(profile_, new ThemeSource(profile_));
  content::URLDataSource::Add(profile_, new LocalNtpSource(profile_));
  content::URLDataSource::Add(profile_, new ThumbnailSource(profile_, false));
  content::URLDataSource::Add(profile_, new ThumbnailSource(profile_, true));
  content::URLDataSource::Add(profile_, new ThumbnailListSource(profile_));
#endif  // !defined(OS_ANDROID)

  favicon::FallbackIconService* fallback_icon_service =
      FallbackIconServiceFactory::GetForBrowserContext(profile_);
  favicon::LargeIconService* large_icon_service =
      LargeIconServiceFactory::GetForBrowserContext(profile_);
  content::URLDataSource::Add(
      profile_, new FallbackIconSource(fallback_icon_service));
  content::URLDataSource::Add(
      profile_, new FaviconSource(profile_, FaviconSource::FAVICON));
  content::URLDataSource::Add(
      profile_, new LargeIconSource(fallback_icon_service, large_icon_service));
  content::URLDataSource::Add(profile_, new MostVisitedIframeSource());
}

InstantService::~InstantService() = default;

void InstantService::AddInstantProcess(int process_id) {
  process_ids_.insert(process_id);

  if (instant_io_context_.get()) {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&InstantIOContext::AddInstantProcessOnIO,
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
  } else if (top_sites_) {
    top_sites_->SyncWithHistory();
  }
}

void InstantService::DeleteMostVisitedItem(const GURL& url) {
  if (most_visited_sites_) {
    most_visited_sites_->AddOrRemoveBlacklistedUrl(url, true);
  } else if (top_sites_) {
    top_sites_->AddBlacklistedURL(url);
  }
}

void InstantService::UndoMostVisitedDeletion(const GURL& url) {
  if (most_visited_sites_) {
    most_visited_sites_->AddOrRemoveBlacklistedUrl(url, false);
  } else if (top_sites_) {
    top_sites_->RemoveBlacklistedURL(url);
  }
}

void InstantService::UndoAllMostVisitedDeletions() {
  if (most_visited_sites_) {
    most_visited_sites_->ClearBlacklistedUrls();
  } else if (top_sites_) {
    top_sites_->ClearBlacklistedURLs();
  }
}

void InstantService::UpdateThemeInfo() {
#if !defined(OS_ANDROID)
  // Update theme background info.
  // Initialize |theme_info| if necessary.
  if (!theme_info_) {
    OnThemeChanged();
  } else {
    for (InstantServiceObserver& observer : observers_)
      observer.ThemeInfoChanged(*theme_info_);
  }
#endif  // !defined(OS_ANDROID)
}

void InstantService::UpdateMostVisitedItemsInfo() {
  NotifyAboutMostVisitedItems();
}

void InstantService::SendSearchURLsToRenderer(content::RenderProcessHost* rph) {
  rph->Send(new ChromeViewMsg_SetSearchURLs(
      search::GetSearchURLs(profile_), search::GetNewTabPageURL(profile_)));
}

InstantSearchPrerenderer* InstantService::GetInstantSearchPrerenderer() {
  // The Instant prefetch base URL may have changed (see e.g. crbug.com/660923).
  // If so, recreate the prerenderer.
  ResetInstantSearchPrerendererIfNecessary();
  return instant_prerenderer_.get();
}

void InstantService::Shutdown() {
  process_ids_.clear();

  if (instant_io_context_.get()) {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&InstantIOContext::ClearInstantProcessesOnIO,
                   instant_io_context_));
  }

  if (most_visited_sites_) {
    most_visited_sites_.reset();
  } else if (top_sites_) {
    top_sites_->RemoveObserver(this);
    top_sites_ = nullptr;
  }

  instant_io_context_ = NULL;
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
#if !defined(OS_ANDROID)
    case chrome::NOTIFICATION_BROWSER_THEME_CHANGED:
      OnThemeChanged();
      break;
#endif  // !defined(OS_ANDROID)
    default:
      NOTREACHED() << "Unexpected notification type in InstantService.";
  }
}

void InstantService::OnRendererProcessTerminated(int process_id) {
  process_ids_.erase(process_id);

  if (instant_io_context_.get()) {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&InstantIOContext::RemoveInstantProcessOnIO,
                   instant_io_context_, process_id));
  }
}

void InstantService::OnTopSitesReceived(
    const history::MostVisitedURLList& data) {
  most_visited_items_.clear();
  for (const history::MostVisitedURL& mv_url : data) {
    InstantMostVisitedItem item;
    item.url = mv_url.url;
    item.title = mv_url.title;
    item.source = ntp_tiles::NTPTileSource::TOP_SITES;
    most_visited_items_.push_back(item);
  }

  NotifyAboutMostVisitedItems();
}

void InstantService::OnMostVisitedURLsAvailable(
    const ntp_tiles::NTPTilesVector& tiles) {
  DCHECK(most_visited_sites_);
  most_visited_items_.clear();
  for (const ntp_tiles::NTPTile& tile : tiles) {
    InstantMostVisitedItem item;
    item.url = tile.url;
    item.title = tile.title;
    item.thumbnail = tile.thumbnail_url;
    item.favicon = tile.favicon_url;
    item.source = tile.source;
    most_visited_items_.push_back(item);
  }

  NotifyAboutMostVisitedItems();
}

void InstantService::OnIconMadeAvailable(const GURL& site_url) {}

void InstantService::NotifyAboutMostVisitedItems() {
  for (InstantServiceObserver& observer : observers_)
    observer.MostVisitedItemsChanged(most_visited_items_);
}

#if !defined(OS_ANDROID)

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

void InstantService::OnThemeChanged() {
  // Get theme information from theme service.
  theme_info_.reset(new ThemeBackgroundInfo());

  // Get if the current theme is the default theme.
  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(profile_);
  theme_info_->using_default_theme = theme_service->UsingDefaultTheme();

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

    // Set theme background image height.
    gfx::ImageSkia* image =
        theme_provider.GetImageSkiaNamed(IDR_THEME_NTP_BACKGROUND);
    DCHECK(image);
    theme_info_->image_height = image->height();

    theme_info_->has_attribution =
        theme_provider.HasCustomImage(IDR_THEME_NTP_ATTRIBUTION);
  }

  for (InstantServiceObserver& observer : observers_)
    observer.ThemeInfoChanged(*theme_info_);
}
#endif  // !defined(OS_ANDROID)

void InstantService::TopSitesLoaded(history::TopSites* top_sites) {
  DCHECK(!most_visited_sites_);
  DCHECK_EQ(top_sites_.get(), top_sites);
}

void InstantService::TopSitesChanged(history::TopSites* top_sites,
                                     ChangeReason change_reason) {
  DCHECK(!most_visited_sites_);
  DCHECK_EQ(top_sites_.get(), top_sites);
  // As forced urls already come from tiles, we can safely ignore those updates.
  if (change_reason == history::TopSitesObserver::ChangeReason::FORCED_URL)
    return;
  top_sites_->GetMostVisitedURLs(base::Bind(&InstantService::OnTopSitesReceived,
                                            weak_ptr_factory_.GetWeakPtr()),
                                 false);
}

void InstantService::OnSearchEngineBaseURLChanged(
    SearchEngineBaseURLTracker::ChangeReason change_reason) {
  ResetInstantSearchPrerendererIfNecessary();
  bool google_base_url_changed =
      change_reason ==
      SearchEngineBaseURLTracker::ChangeReason::GOOGLE_BASE_URL;
  for (InstantServiceObserver& observer : observers_)
    observer.DefaultSearchProviderChanged(google_base_url_changed);
}

void InstantService::ResetInstantSearchPrerendererIfNecessary() {
  if (!search::IsInstantExtendedAPIEnabled())
    return;

  GURL url(search::GetSearchResultPrefetchBaseURL(profile_));
  if (!instant_prerenderer_ || instant_prerenderer_->prerender_url() != url) {
    instant_prerenderer_.reset(
        url.is_valid() ? new InstantSearchPrerenderer(profile_, url) : nullptr);
  }
}
