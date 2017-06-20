// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/local_ntp_source.h"

#include "base/base64.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_io_context.h"
#include "chrome/browser/search/local_files_ntp_source.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_data.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_service.h"
#include "chrome/browser/search/one_google_bar/one_google_bar_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/secure_hash.h"
#include "net/base/hash_value.h"
#include "net/url_request/url_request.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/ui_resources.h"
#include "url/gurl.h"

namespace {

// Signifies a locally constructed resource, i.e. not from grit/.
const int kLocalResource = -1;

const char kConfigDataFilename[] = "config.js";
const char kThemeCSSFilename[] = "theme.css";
const char kMainHtmlFilename[] = "local-ntp.html";
const char kOneGoogleBarScriptFilename[] = "one-google.js";
const char kOneGoogleBarInHeadStyleFilename[] = "one-google/in-head.css";
const char kOneGoogleBarInHeadScriptFilename[] = "one-google/in-head.js";
const char kOneGoogleBarAfterBarScriptFilename[] = "one-google/after-bar.js";
const char kOneGoogleBarEndOfBodyScriptFilename[] = "one-google/end-of-body.js";

const struct Resource{
  const char* filename;
  int identifier;
  const char* mime_type;
} kResources[] = {
    {kMainHtmlFilename, kLocalResource, "text/html"},
    {"local-ntp.js", IDR_LOCAL_NTP_JS, "application/javascript"},
    {kConfigDataFilename, kLocalResource, "application/javascript"},
    {kThemeCSSFilename, kLocalResource, "text/css"},
    {"local-ntp.css", IDR_LOCAL_NTP_CSS, "text/css"},
    {"images/close_3_mask.png", IDR_CLOSE_3_MASK, "image/png"},
    {"images/ntp_default_favicon.png", IDR_NTP_DEFAULT_FAVICON, "image/png"},
    {kOneGoogleBarScriptFilename, kLocalResource, "text/javascript"},
    {kOneGoogleBarInHeadStyleFilename, kLocalResource, "text/css"},
    {kOneGoogleBarInHeadScriptFilename, kLocalResource, "text/javascript"},
    {kOneGoogleBarAfterBarScriptFilename, kLocalResource, "text/javascript"},
    {kOneGoogleBarEndOfBodyScriptFilename, kLocalResource, "text/javascript"},
};

// Strips any query parameters from the specified path.
std::string StripParameters(const std::string& path) {
  return path.substr(0, path.find("?"));
}

// Adds a localized string keyed by resource id to the dictionary.
void AddString(base::DictionaryValue* dictionary,
               const std::string& key,
               int resource_id) {
  dictionary->SetString(key, l10n_util::GetStringUTF16(resource_id));
}

// Populates |translated_strings| dictionary for the local NTP. |is_google|
// indicates that this page is the Google local NTP.
std::unique_ptr<base::DictionaryValue> GetTranslatedStrings(bool is_google) {
  auto translated_strings = base::MakeUnique<base::DictionaryValue>();

  AddString(translated_strings.get(), "thumbnailRemovedNotification",
            IDS_NEW_TAB_THUMBNAIL_REMOVED_NOTIFICATION);
  AddString(translated_strings.get(), "removeThumbnailTooltip",
            IDS_NEW_TAB_REMOVE_THUMBNAIL_TOOLTIP);
  AddString(translated_strings.get(), "undoThumbnailRemove",
            IDS_NEW_TAB_UNDO_THUMBNAIL_REMOVE);
  AddString(translated_strings.get(), "restoreThumbnailsShort",
            IDS_NEW_TAB_RESTORE_THUMBNAILS_SHORT_LINK);
  AddString(translated_strings.get(), "attributionIntro",
            IDS_NEW_TAB_ATTRIBUTION_INTRO);
  AddString(translated_strings.get(), "title", IDS_NEW_TAB_TITLE);
  if (is_google) {
    AddString(translated_strings.get(), "searchboxPlaceholder",
              IDS_GOOGLE_SEARCH_BOX_EMPTY_HINT);
  }

  return translated_strings;
}

// Returns a JS dictionary of configuration data for the local NTP.
std::string GetConfigData(bool is_google) {
  base::DictionaryValue config_data;
  config_data.Set("translatedStrings", GetTranslatedStrings(is_google));
  config_data.SetBoolean("isGooglePage", is_google);

  // Serialize the dictionary.
  std::string js_text;
  JSONStringValueSerializer serializer(&js_text);
  serializer.Serialize(config_data);

  std::string config_data_js;
  config_data_js.append("var configData = ");
  config_data_js.append(js_text);
  config_data_js.append(";");
  return config_data_js;
}

std::string GetIntegritySha256Value(const std::string& data) {
  // Compute the sha256 hash.
  net::SHA256HashValue hash_value;
  std::unique_ptr<crypto::SecureHash> hash(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));
  hash->Update(data.data(), data.size());
  hash->Finish(&hash_value, sizeof(hash_value));

  // Base64-encode it.
  base::StringPiece hash_value_str(
      reinterpret_cast<const char*>(hash_value.data), sizeof(hash_value));
  std::string result;
  base::Base64Encode(hash_value_str, &result);
  return result;
}

std::string GetThemeCSS(Profile* profile) {
  SkColor background_color =
      ThemeService::GetThemeProviderForProfile(profile)
          .GetColor(ThemeProperties::COLOR_NTP_BACKGROUND);

  return base::StringPrintf("body { background-color: #%02X%02X%02X; }",
                            SkColorGetR(background_color),
                            SkColorGetG(background_color),
                            SkColorGetB(background_color));
}

std::string GetLocalNtpPath() {
  return std::string(chrome::kChromeSearchScheme) + "://" +
         std::string(chrome::kChromeSearchLocalNtpHost) + "/";
}

std::unique_ptr<base::DictionaryValue> ConvertOGBDataToDict(
    const OneGoogleBarData& og) {
  auto result = base::MakeUnique<base::DictionaryValue>();
  // Only provide the html parts here. The js and css are injected separately
  // via <script src=...> and <link rel="stylesheet" href=...>.
  result->SetString("html", og.bar_html);
  result->SetString("end_of_body_html", og.end_of_body_html);
  return result;
}

}  // namespace

class LocalNtpSource::GoogleSearchProviderTracker
    : public TemplateURLServiceObserver {
 public:
  using SearchProviderIsGoogleChangedCallback =
      base::Callback<void(bool is_google)>;

  GoogleSearchProviderTracker(
      TemplateURLService* service,
      const SearchProviderIsGoogleChangedCallback& callback)
      : service_(service), callback_(callback), is_google_(false) {
    DCHECK(service_);
    service_->AddObserver(this);
    is_google_ = search::DefaultSearchProviderIsGoogle(service_);
  }

  ~GoogleSearchProviderTracker() override {
    if (service_)
      service_->RemoveObserver(this);
  }

  bool DefaultSearchProviderIsGoogle() const { return is_google_; }

 private:
  void OnTemplateURLServiceChanged() override {
    bool old_is_google = is_google_;
    is_google_ = search::DefaultSearchProviderIsGoogle(service_);
    if (is_google_ != old_is_google)
      callback_.Run(is_google_);
  }

  void OnTemplateURLServiceShuttingDown() override {
    service_->RemoveObserver(this);
    service_ = nullptr;
  }

  TemplateURLService* service_;
  SearchProviderIsGoogleChangedCallback callback_;

  bool is_google_;
};

LocalNtpSource::LocalNtpSource(Profile* profile)
    : profile_(profile),
      one_google_bar_service_(nullptr),
      one_google_bar_service_observer_(this),
      default_search_provider_is_google_(false),
      default_search_provider_is_google_io_thread_(false),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (base::FeatureList::IsEnabled(features::kOneGoogleBarOnLocalNtp)) {
    one_google_bar_service_ =
        OneGoogleBarServiceFactory::GetForProfile(profile_);
  }

  // |one_google_bar_service_| is null in incognito, or when the feature is
  // disabled.
  if (one_google_bar_service_)
    one_google_bar_service_observer_.Add(one_google_bar_service_);

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (template_url_service) {
    google_tracker_ = base::MakeUnique<GoogleSearchProviderTracker>(
        template_url_service,
        base::Bind(&LocalNtpSource::DefaultSearchProviderIsGoogleChanged,
                   base::Unretained(this)));
    DefaultSearchProviderIsGoogleChanged(
        google_tracker_->DefaultSearchProviderIsGoogle());
  }
}

LocalNtpSource::~LocalNtpSource() = default;

std::string LocalNtpSource::GetSource() const {
  return chrome::kChromeSearchLocalNtpHost;
}

void LocalNtpSource::StartDataRequest(
    const std::string& path,
    const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
    const content::URLDataSource::GotDataCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::string stripped_path = StripParameters(path);
  if (stripped_path == kConfigDataFilename) {
    std::string config_data_js =
        GetConfigData(default_search_provider_is_google_);
    callback.Run(base::RefCountedString::TakeString(&config_data_js));
    return;
  }
  if (stripped_path == kThemeCSSFilename) {
    std::string theme_css = GetThemeCSS(profile_);
    callback.Run(base::RefCountedString::TakeString(&theme_css));
    return;
  }

  if (base::StartsWith(stripped_path, "one-google",
                       base::CompareCase::SENSITIVE)) {
    if (!one_google_bar_service_) {
      callback.Run(nullptr);
      return;
    }

    const base::Optional<OneGoogleBarData>& data =
        one_google_bar_service_->one_google_bar_data();

    // The OneGoogleBar injector helper.
    if (stripped_path == kOneGoogleBarScriptFilename) {
      one_google_bar_requests_.emplace_back(base::TimeTicks::Now(), callback);

      // If there already is (cached) OGB data, serve it immediately.
      if (data.has_value())
        ServeOneGoogleBar(data);

      // In any case, request a refresh.
      one_google_bar_service_->Refresh();
    } else {
      // The actual OneGoogleBar sources.
      std::string result;
      if (data.has_value()) {
        if (stripped_path == kOneGoogleBarInHeadStyleFilename) {
          result = data->in_head_style;
        } else if (stripped_path == kOneGoogleBarInHeadScriptFilename) {
          result = data->in_head_script;
        } else if (stripped_path == kOneGoogleBarAfterBarScriptFilename) {
          result = data->after_bar_script;
        } else if (stripped_path == kOneGoogleBarEndOfBodyScriptFilename) {
          result = data->end_of_body_script;
        }
      }
      callback.Run(base::RefCountedString::TakeString(&result));
    }

    return;
  }

#if !defined(GOOGLE_CHROME_BUILD)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kLocalNtpReload)) {
    if (stripped_path == "local-ntp.html" || stripped_path == "local-ntp.js" ||
        stripped_path == "local-ntp.css") {
      base::ReplaceChars(stripped_path, "-", "_", &stripped_path);
      local_ntp::SendLocalFileResource(stripped_path, callback);
      return;
    }
  }
#endif  // !defined(GOOGLE_CHROME_BUILD)

  if (stripped_path == kMainHtmlFilename) {
    std::string html = ResourceBundle::GetSharedInstance()
                           .GetRawDataResource(IDR_LOCAL_NTP_HTML)
                           .as_string();
    std::string config_sha256 =
        "sha256-" + GetIntegritySha256Value(
                        GetConfigData(default_search_provider_is_google_));
    base::ReplaceFirstSubstringAfterOffset(&html, 0, "{{CONFIG_INTEGRITY}}",
                                           config_sha256);
    callback.Run(base::RefCountedString::TakeString(&html));
    return;
  }

  float scale = 1.0f;
  std::string filename;
  webui::ParsePathAndScale(
      GURL(GetLocalNtpPath() + stripped_path), &filename, &scale);
  ui::ScaleFactor scale_factor = ui::GetSupportedScaleFactor(scale);

  for (size_t i = 0; i < arraysize(kResources); ++i) {
    if (filename == kResources[i].filename) {
      scoped_refptr<base::RefCountedMemory> response(
          ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
              kResources[i].identifier, scale_factor));
      callback.Run(response.get());
      return;
    }
  }
  callback.Run(nullptr);
}

std::string LocalNtpSource::GetMimeType(
    const std::string& path) const {
  const std::string stripped_path = StripParameters(path);
  for (size_t i = 0; i < arraysize(kResources); ++i) {
    if (stripped_path == kResources[i].filename)
      return kResources[i].mime_type;
  }
  return std::string();
}

bool LocalNtpSource::AllowCaching() const {
  // Some resources served by LocalNtpSource, i.e. config.js, are dynamically
  // generated and could differ on each access. To avoid using old cached
  // content on reload, disallow caching here. Otherwise, it fails to reflect
  // newly revised user configurations in the page.
  return false;
}

bool LocalNtpSource::ShouldServiceRequest(
    const GURL& url,
    content::ResourceContext* resource_context,
    int render_process_id) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  DCHECK(url.host_piece() == chrome::kChromeSearchLocalNtpHost);
  if (!InstantIOContext::ShouldServiceRequest(url, resource_context,
                                              render_process_id)) {
    return false;
  }

  if (url.SchemeIs(chrome::kChromeSearchScheme)) {
    std::string filename;
    webui::ParsePathAndScale(url, &filename, nullptr);
    for (size_t i = 0; i < arraysize(kResources); ++i) {
      if (filename == kResources[i].filename)
        return true;
    }
  }
  return false;
}

std::string LocalNtpSource::GetContentSecurityPolicyScriptSrc() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

#if !defined(GOOGLE_CHROME_BUILD)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kLocalNtpReload)) {
    // While live-editing the local NTP files, turn off CSP.
    return "script-src *;";
  }
#endif  // !defined(GOOGLE_CHROME_BUILD)

  return "script-src 'strict-dynamic' "
         "'sha256-" +
         GetIntegritySha256Value(
             GetConfigData(default_search_provider_is_google_io_thread_)) +
         "' "
         "'sha256-lulnU8hGXcddrvssXT2LbFuVh5e/8iE6ENokfXF7NRo=';";
}

std::string LocalNtpSource::GetContentSecurityPolicyChildSrc() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (one_google_bar_service_) {
    // Allow embedding of the most visited iframe, as well as the account
    // switcher and the notifications dropdown from the One Google Bar.
    return base::StringPrintf("child-src %s https://*.google.com/;",
                              chrome::kChromeSearchMostVisitedUrl);
  }
  // Allow embedding of the most visited iframe.
  return base::StringPrintf("child-src %s;",
                            chrome::kChromeSearchMostVisitedUrl);
}

void LocalNtpSource::OnOneGoogleBarDataChanged() {
  const base::Optional<OneGoogleBarData>& data =
      one_google_bar_service_->one_google_bar_data();
  ServeOneGoogleBar(data);
}

void LocalNtpSource::OnOneGoogleBarFetchFailed() {
  ServeOneGoogleBar(base::nullopt);
}

void LocalNtpSource::OnOneGoogleBarServiceShuttingDown() {
  one_google_bar_service_observer_.RemoveAll();
  one_google_bar_service_ = nullptr;
}

void LocalNtpSource::ServeOneGoogleBar(
    const base::Optional<OneGoogleBarData>& data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (one_google_bar_requests_.empty())
    return;

  scoped_refptr<base::RefCountedString> result;
  if (data.has_value()) {
    std::string js;
    base::JSONWriter::Write(*ConvertOGBDataToDict(*data), &js);
    js = "var og = " + js + ";";
    result = base::RefCountedString::TakeString(&js);
  }

  base::TimeTicks now = base::TimeTicks::Now();
  for (const auto& request : one_google_bar_requests_) {
    request.callback.Run(result);
    base::TimeDelta delta = now - request.start_time;
    UMA_HISTOGRAM_MEDIUM_TIMES("NewTabPage.OneGoogleBar.RequestLatency", delta);
    if (result) {
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "NewTabPage.OneGoogleBar.RequestLatency.Success", delta);
    } else {
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "NewTabPage.OneGoogleBar.RequestLatency.Failure", delta);
    }
  }
  one_google_bar_requests_.clear();
}

void LocalNtpSource::DefaultSearchProviderIsGoogleChanged(bool is_google) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  default_search_provider_is_google_ = is_google;
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&LocalNtpSource::SetDefaultSearchProviderIsGoogleOnIOThread,
                 weak_ptr_factory_.GetWeakPtr(), is_google));
}

void LocalNtpSource::SetDefaultSearchProviderIsGoogleOnIOThread(
    bool is_google) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  default_search_provider_is_google_io_thread_ = is_google;
}

LocalNtpSource::OneGoogleBarRequest::OneGoogleBarRequest(
    base::TimeTicks start_time,
    const content::URLDataSource::GotDataCallback& callback)
    : start_time(start_time), callback(callback) {}

LocalNtpSource::OneGoogleBarRequest::OneGoogleBarRequest(
    const OneGoogleBarRequest&) = default;

LocalNtpSource::OneGoogleBarRequest::~OneGoogleBarRequest() = default;
