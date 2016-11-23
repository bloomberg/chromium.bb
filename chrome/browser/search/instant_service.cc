// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/instant_service.h"

#include <stddef.h>

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/favicon/fallback_icon_service_factory.h"
#include "chrome/browser/favicon/large_icon_service_factory.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_io_context.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/search/most_visited_iframe_source.h"
#include "chrome/browser/search/search.h"
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
#include "components/keyed_service/core/service_access_type.h"
#include "components/search/search.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia.h"
#include "url/url_constants.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/search/local_ntp_source.h"
#endif

#if defined(ENABLE_THEMES)
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#endif  // defined(ENABLE_THEMES)

InstantService::InstantService(Profile* profile)
    : profile_(profile),
      template_url_service_(TemplateURLServiceFactory::GetForProfile(profile_)),
      weak_ptr_factory_(this) {
  // The initialization below depends on a typical set of browser threads. Skip
  // it if we are running in a unit test without the full suite.
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI))
    return;

  // This depends on the existence of the typical browser threads. Therefore it
  // is only instantiated here (after the check for a UI thread above).
  instant_io_context_ = new InstantIOContext();

  previous_google_base_url_ =
      GURL(UIThreadSearchTermsData(profile).GoogleBaseURLValue());

  // TemplateURLService is NULL by default in tests.
  if (template_url_service_) {
    template_url_service_->AddObserver(this);
    const TemplateURL* default_search_provider =
        template_url_service_->GetDefaultSearchProvider();
    if (default_search_provider) {
      previous_default_search_provider_.reset(
          new TemplateURLData(default_search_provider->data()));
    }
  }

  ResetInstantSearchPrerendererIfNecessary();

  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::NotificationService::AllSources());

  scoped_refptr<history::TopSites> top_sites =
      TopSitesFactory::GetForProfile(profile_);
  if (top_sites) {
    top_sites->AddObserver(this);
    // Immediately query the TopSites state.
    TopSitesChanged(top_sites.get(),
                    history::TopSitesObserver::ChangeReason::MOST_VISITED);
  }

  if (profile_ && profile_->GetResourceContext()) {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
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

  // TODO(aurimas) remove this #if once instant_service.cc is no longer compiled
  // on Android.
#if !defined(OS_ANDROID)
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

InstantService::~InstantService() {
  if (template_url_service_)
    template_url_service_->RemoveObserver(this);
}

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
  scoped_refptr<history::TopSites> top_sites =
      TopSitesFactory::GetForProfile(profile_);
  if (top_sites)
    top_sites->SyncWithHistory();
}

void InstantService::DeleteMostVisitedItem(const GURL& url) {
  scoped_refptr<history::TopSites> top_sites =
      TopSitesFactory::GetForProfile(profile_);
  if (top_sites)
    top_sites->AddBlacklistedURL(url);
}

void InstantService::UndoMostVisitedDeletion(const GURL& url) {
  scoped_refptr<history::TopSites> top_sites =
      TopSitesFactory::GetForProfile(profile_);
  if (top_sites)
    top_sites->RemoveBlacklistedURL(url);
}

void InstantService::UndoAllMostVisitedDeletions() {
  scoped_refptr<history::TopSites> top_sites =
      TopSitesFactory::GetForProfile(profile_);
  if (top_sites)
    top_sites->ClearBlacklistedURLs();
}

void InstantService::UpdateThemeInfo() {
#if defined(ENABLE_THEMES)
  // Update theme background info.
  // Initialize |theme_info| if necessary.
  if (!theme_info_) {
    OnThemeChanged();
  } else {
    for (InstantServiceObserver& observer : observers_)
      observer.ThemeInfoChanged(*theme_info_);
  }
#endif  // defined(ENABLE_THEMES)
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

  scoped_refptr<history::TopSites> top_sites =
      TopSitesFactory::GetForProfile(profile_);
  if (top_sites)
    top_sites->RemoveObserver(this);

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
#if defined(ENABLE_THEMES)
    case chrome::NOTIFICATION_BROWSER_THEME_CHANGED:
      OnThemeChanged();
      break;
#endif  // defined(ENABLE_THEMES)
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

void InstantService::OnMostVisitedItemsReceived(
    const history::MostVisitedURLList& data) {
  most_visited_items_.clear();
  for (const history::MostVisitedURL& mv_url : data) {
    InstantMostVisitedItem item;
    item.url = mv_url.url;
    item.title = mv_url.title;
    item.is_server_side_suggestion = false;
    most_visited_items_.push_back(item);
  }

  NotifyAboutMostVisitedItems();
}

void InstantService::NotifyAboutMostVisitedItems() {
  for (InstantServiceObserver& observer : observers_)
    observer.MostVisitedItemsChanged(most_visited_items_);
}

#if defined(ENABLE_THEMES)

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
#endif  // defined(ENABLE_THEMES)

void InstantService::OnTemplateURLServiceChanged() {
  // Check whether the default search provider was changed.
  const TemplateURL* template_url =
      template_url_service_->GetDefaultSearchProvider();
  bool default_search_provider_changed = !TemplateURL::MatchesData(
      template_url, previous_default_search_provider_.get(),
      UIThreadSearchTermsData(profile_));
  if (default_search_provider_changed) {
    previous_default_search_provider_.reset(
        template_url ? new TemplateURLData(template_url->data()) : NULL);
  }

  // Note that, even if the TemplateURL for the Default Search Provider has not
  // changed, the effective URLs might change if they reference the Google base
  // URL. The TemplateURLService will notify us when the effective URL changes
  // in this way but it's up to us to do the work to check both.
  bool google_base_url_domain_changed = false;
  GURL google_base_url(UIThreadSearchTermsData(profile_).GoogleBaseURLValue());
  if (google_base_url != previous_google_base_url_) {
    previous_google_base_url_ = google_base_url;
    if (template_url &&
        template_url->HasGoogleBaseURLs(UIThreadSearchTermsData(profile_))) {
      google_base_url_domain_changed = true;
    }
  }

  if (default_search_provider_changed || google_base_url_domain_changed) {
    ResetInstantSearchPrerendererIfNecessary();
    for (InstantServiceObserver& observer : observers_)
      observer.DefaultSearchProviderChanged(google_base_url_domain_changed);
  }
}

void InstantService::TopSitesLoaded(history::TopSites* top_sites) {
}

void InstantService::TopSitesChanged(history::TopSites* top_sites,
                                     ChangeReason change_reason) {
  // As forced urls already come from tiles, we can safely ignore those updates.
  if (change_reason == history::TopSitesObserver::ChangeReason::FORCED_URL)
    return;
  top_sites->GetMostVisitedURLs(
      base::Bind(&InstantService::OnMostVisitedItemsReceived,
                 weak_ptr_factory_.GetWeakPtr()),
      false);
}

void InstantService::ResetInstantSearchPrerendererIfNecessary() {
  if (!search::ShouldPrefetchSearchResults())
    return;

  GURL url(search::GetSearchResultPrefetchBaseURL(profile_));
  if (!instant_prerenderer_ || instant_prerenderer_->prerender_url() != url) {
    instant_prerenderer_.reset(
        url.is_valid() ? new InstantSearchPrerenderer(profile_, url) : nullptr);
  }
}
