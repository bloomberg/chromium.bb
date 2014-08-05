// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/instant_service.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_io_context.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/search/local_ntp_source.h"
#include "chrome/browser/search/most_visited_iframe_source.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search/suggestions/suggestions_source.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/thumbnails/thumbnail_list_source.h"
#include "chrome/browser/ui/search/instant_search_prerenderer.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/ntp/thumbnail_source.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/render_messages.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/url_data_source.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/sys_color_change_listener.h"


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
      template_url_service_(TemplateURLServiceFactory::GetForProfile(profile_)),
      omnibox_start_margin_(chrome::kDisableStartMargin),
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

  ResetInstantSearchPrerenderer();

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
  content::URLDataSource::Add(profile_, new ThumbnailSource(profile_, false));
  content::URLDataSource::Add(profile_, new ThumbnailSource(profile_, true));
  content::URLDataSource::Add(profile_, new ThumbnailListSource(profile_));
#endif  // !defined(OS_ANDROID)

  content::URLDataSource::Add(
      profile_, new FaviconSource(profile_, FaviconSource::FAVICON));
  content::URLDataSource::Add(profile_, new LocalNtpSource(profile_));
  content::URLDataSource::Add(profile_, new MostVisitedIframeSource());
  content::URLDataSource::Add(
      profile_, new suggestions::SuggestionsSource(profile_));
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
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&InstantIOContext::ClearInstantProcessesOnIO,
                   instant_io_context_));
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
    default:
      NOTREACHED() << "Unexpected notification type in InstantService.";
  }
}

void InstantService::SendSearchURLsToRenderer(content::RenderProcessHost* rph) {
  rph->Send(new ChromeViewMsg_SetSearchURLs(
      chrome::GetSearchURLs(profile_), chrome::GetNewTabPageURL(profile_)));
}

void InstantService::OnOmniboxStartMarginChanged(int start_margin) {
  omnibox_start_margin_ = start_margin;
  FOR_EACH_OBSERVER(InstantServiceObserver, observers_,
                    OmniboxStartMarginChanged(omnibox_start_margin_));
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
  history::MostVisitedURLList reordered_data(data);
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
  GURL google_base_url(UIThreadSearchTermsData(profile_).GoogleBaseURLValue());
  if (google_base_url != previous_google_base_url_) {
    previous_google_base_url_ = google_base_url;
    if (template_url && template_url->HasGoogleBaseURLs(
            UIThreadSearchTermsData(profile_)))
      default_search_provider_changed = true;
  }

  if (default_search_provider_changed) {
    ResetInstantSearchPrerenderer();
    FOR_EACH_OBSERVER(InstantServiceObserver, observers_,
                      DefaultSearchProviderChanged());
  }
}

void InstantService::ResetInstantSearchPrerenderer() {
  if (!chrome::ShouldPrefetchSearchResults())
    return;

  GURL url(chrome::GetSearchResultPrefetchBaseURL(profile_));
  instant_prerenderer_.reset(
      url.is_valid() ? new InstantSearchPrerenderer(profile_, url) : NULL);
}
