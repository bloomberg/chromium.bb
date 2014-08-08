// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/omnibox/autocomplete_controller_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/autocomplete/autocomplete_classifier.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/autocomplete_controller.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/autocomplete/search_provider.h"
#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/omnibox/omnibox_log.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/ui/search/instant_search_prerenderer.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "chrome/common/instant_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/omnibox/autocomplete_input.h"
#include "components/omnibox/autocomplete_match.h"
#include "components/omnibox/autocomplete_match_type.h"
#include "components/omnibox/omnibox_field_trial.h"
#include "components/search/search.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "jni/AutocompleteController_jni.h"
#include "net/base/escape.h"
#include "net/base/net_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/resource/resource_bundle.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using metrics::OmniboxEventProto;

namespace {

const int kAndroidAutocompleteProviders =
    AutocompleteClassifier::kDefaultOmniboxProviders;

/**
 * A prefetcher class responsible for triggering zero suggest prefetch.
 * The prefetch occurs as a side-effect of calling StartZeroSuggest() on
 * the AutocompleteController object.
 */
class ZeroSuggestPrefetcher : public AutocompleteControllerDelegate {
 public:
  explicit ZeroSuggestPrefetcher(Profile* profile);

 private:
  virtual ~ZeroSuggestPrefetcher();
  void SelfDestruct();

  // AutocompleteControllerDelegate:
  virtual void OnResultChanged(bool default_match_changed) OVERRIDE;

  scoped_ptr<AutocompleteController> controller_;
  base::OneShotTimer<ZeroSuggestPrefetcher> expire_timer_;
};

ZeroSuggestPrefetcher::ZeroSuggestPrefetcher(Profile* profile)
    : controller_(new AutocompleteController(
          profile, TemplateURLServiceFactory::GetForProfile(profile), this,
          AutocompleteProvider::TYPE_ZERO_SUGGEST)) {
  // Creating an arbitrary fake_request_source to avoid passing in an invalid
  // AutocompleteInput object.
  base::string16 fake_request_source(base::ASCIIToUTF16(
      "http://www.foobarbazblah.com"));
  controller_->StartZeroSuggest(AutocompleteInput(
      fake_request_source, base::string16::npos, base::string16(),
      GURL(fake_request_source), OmniboxEventProto::INVALID_SPEC, false, false,
      true, true, ChromeAutocompleteSchemeClassifier(profile)));
  // Delete ourselves after 10s. This is enough time to cache results or
  // give up if the results haven't been received.
  expire_timer_.Start(FROM_HERE,
                      base::TimeDelta::FromMilliseconds(10000),
                      this, &ZeroSuggestPrefetcher::SelfDestruct);
}

ZeroSuggestPrefetcher::~ZeroSuggestPrefetcher() {
}

void ZeroSuggestPrefetcher::SelfDestruct() {
  delete this;
}

void ZeroSuggestPrefetcher::OnResultChanged(bool default_match_changed) {
  // Nothing to do here, the results have been cached.
  // We don't want to trigger deletion here because this is being called by the
  // AutocompleteController object.
}

}  // namespace

AutocompleteControllerAndroid::AutocompleteControllerAndroid(Profile* profile)
    : autocomplete_controller_(new AutocompleteController(
          profile, TemplateURLServiceFactory::GetForProfile(profile), this,
          kAndroidAutocompleteProviders)),
      inside_synchronous_start_(false),
      profile_(profile) {
}

void AutocompleteControllerAndroid::Start(JNIEnv* env,
                                          jobject obj,
                                          jstring j_text,
                                          jstring j_desired_tld,
                                          jstring j_current_url,
                                          bool prevent_inline_autocomplete,
                                          bool prefer_keyword,
                                          bool allow_exact_keyword_match,
                                          bool want_asynchronous_matches) {
  if (!autocomplete_controller_)
    return;

  base::string16 desired_tld;
  GURL current_url;
  if (j_current_url != NULL)
    current_url = GURL(ConvertJavaStringToUTF16(env, j_current_url));
  if (j_desired_tld != NULL)
    desired_tld = ConvertJavaStringToUTF16(env, j_desired_tld);
  base::string16 text = ConvertJavaStringToUTF16(env, j_text);
  OmniboxEventProto::PageClassification page_classification =
      OmniboxEventProto::OTHER;
  input_ = AutocompleteInput(
      text, base::string16::npos, desired_tld, current_url, page_classification,
      prevent_inline_autocomplete, prefer_keyword, allow_exact_keyword_match,
      want_asynchronous_matches, ChromeAutocompleteSchemeClassifier(profile_));
  autocomplete_controller_->Start(input_);
}

ScopedJavaLocalRef<jobject> AutocompleteControllerAndroid::Classify(
    JNIEnv* env,
    jobject obj,
    jstring j_text) {
  return GetTopSynchronousResult(env, obj, j_text, true);
}

void AutocompleteControllerAndroid::StartZeroSuggest(
    JNIEnv* env,
    jobject obj,
    jstring j_omnibox_text,
    jstring j_current_url,
    jboolean is_query_in_omnibox,
    jboolean focused_from_fakebox) {
  if (!autocomplete_controller_)
    return;

  base::string16 url = ConvertJavaStringToUTF16(env, j_current_url);
  const GURL current_url = GURL(url);
  base::string16 omnibox_text = ConvertJavaStringToUTF16(env, j_omnibox_text);

  // If omnibox text is empty, set it to the current URL for the purposes of
  // populating the verbatim match.
  if (omnibox_text.empty())
    omnibox_text = url;

  input_ = AutocompleteInput(
      omnibox_text, base::string16::npos, base::string16(), current_url,
      ClassifyPage(current_url, is_query_in_omnibox, focused_from_fakebox),
      false, false, true, true, ChromeAutocompleteSchemeClassifier(profile_));
  autocomplete_controller_->StartZeroSuggest(input_);
}

void AutocompleteControllerAndroid::Stop(JNIEnv* env,
                                         jobject obj,
                                         bool clear_results) {
  if (autocomplete_controller_ != NULL)
    autocomplete_controller_->Stop(clear_results);
}

void AutocompleteControllerAndroid::ResetSession(JNIEnv* env, jobject obj) {
  if (autocomplete_controller_ != NULL)
    autocomplete_controller_->ResetSession();
}

void AutocompleteControllerAndroid::OnSuggestionSelected(
    JNIEnv* env,
    jobject obj,
    jint selected_index,
    jstring j_current_url,
    jboolean is_query_in_omnibox,
    jboolean focused_from_fakebox,
    jlong elapsed_time_since_first_modified,
    jobject j_web_contents) {
  base::string16 url = ConvertJavaStringToUTF16(env, j_current_url);
  const GURL current_url = GURL(url);
  OmniboxEventProto::PageClassification current_page_classification =
      ClassifyPage(current_url, is_query_in_omnibox, focused_from_fakebox);
  const base::TimeTicks& now(base::TimeTicks::Now());
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);

  OmniboxLog log(
      input_.text(),
      false, /* don't know */
      input_.type(),
      true,
      selected_index,
      false,
      SessionID::IdForTab(web_contents),
      current_page_classification,
      base::TimeDelta::FromMilliseconds(elapsed_time_since_first_modified),
      base::string16::npos,
      now - autocomplete_controller_->last_time_default_match_changed(),
      autocomplete_controller_->result());
  autocomplete_controller_->AddProvidersInfo(&log.providers_info);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_OMNIBOX_OPENED_URL,
      content::Source<Profile>(profile_),
      content::Details<OmniboxLog>(&log));
}

void AutocompleteControllerAndroid::DeleteSuggestion(JNIEnv* env,
                                                     jobject obj,
                                                     int selected_index) {
  const AutocompleteResult& result = autocomplete_controller_->result();
  const AutocompleteMatch& match = result.match_at(selected_index);
  if (match.SupportsDeletion())
    autocomplete_controller_->DeleteMatch(match);
}

ScopedJavaLocalRef<jstring> AutocompleteControllerAndroid::
    UpdateMatchDestinationURLWithQueryFormulationTime(
        JNIEnv* env,
        jobject obj,
        jint selected_index,
        jlong elapsed_time_since_input_change) {
  // In rare cases, we navigate to cached matches and the underlying result
  // has already been cleared, in that case ignore the URL update.
  if (autocomplete_controller_->result().empty())
    return ScopedJavaLocalRef<jstring>();

  AutocompleteMatch match(
      autocomplete_controller_->result().match_at(selected_index));
  autocomplete_controller_->UpdateMatchDestinationURLWithQueryFormulationTime(
      base::TimeDelta::FromMilliseconds(elapsed_time_since_input_change),
      &match);
  return ConvertUTF8ToJavaString(env, match.destination_url.spec());
}

ScopedJavaLocalRef<jobject>
AutocompleteControllerAndroid::GetTopSynchronousMatch(JNIEnv* env,
                                                      jobject obj,
                                                      jstring query) {
  return GetTopSynchronousResult(env, obj, query, false);
}

void AutocompleteControllerAndroid::Shutdown() {
  autocomplete_controller_.reset();

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_bridge =
      weak_java_autocomplete_controller_android_.get(env);
  if (java_bridge.obj())
    Java_AutocompleteController_notifyNativeDestroyed(env, java_bridge.obj());

  weak_java_autocomplete_controller_android_.reset();
}

// static
AutocompleteControllerAndroid*
AutocompleteControllerAndroid::Factory::GetForProfile(
    Profile* profile, JNIEnv* env, jobject obj) {
  AutocompleteControllerAndroid* bridge =
      static_cast<AutocompleteControllerAndroid*>(
          GetInstance()->GetServiceForBrowserContext(profile, true));
  bridge->InitJNI(env, obj);
  return bridge;
}

AutocompleteControllerAndroid::Factory*
AutocompleteControllerAndroid::Factory::GetInstance() {
  return Singleton<AutocompleteControllerAndroid::Factory>::get();
}

content::BrowserContext*
AutocompleteControllerAndroid::Factory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

AutocompleteControllerAndroid::Factory::Factory()
    : BrowserContextKeyedServiceFactory(
          "AutocompleteControllerAndroid",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ShortcutsBackendFactory::GetInstance());
}

AutocompleteControllerAndroid::Factory::~Factory() {
}

KeyedService* AutocompleteControllerAndroid::Factory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new AutocompleteControllerAndroid(static_cast<Profile*>(profile));
}

AutocompleteControllerAndroid::~AutocompleteControllerAndroid() {
}

void AutocompleteControllerAndroid::InitJNI(JNIEnv* env, jobject obj) {
  weak_java_autocomplete_controller_android_ =
      JavaObjectWeakGlobalRef(env, obj);
}

void AutocompleteControllerAndroid::OnResultChanged(
    bool default_match_changed) {
  if (!autocomplete_controller_)
    return;

  const AutocompleteResult& result = autocomplete_controller_->result();
  const AutocompleteResult::const_iterator default_match(
      result.default_match());
  if ((default_match != result.end()) && default_match_changed &&
      chrome::IsInstantExtendedAPIEnabled() &&
      chrome::ShouldPrefetchSearchResults()) {
    InstantSuggestion prefetch_suggestion;
    // If the default match should be prefetched, do that.
    if (SearchProvider::ShouldPrefetch(*default_match)) {
      prefetch_suggestion.text = default_match->contents;
      prefetch_suggestion.metadata =
          SearchProvider::GetSuggestMetadata(*default_match);
    }
    // Send the prefetch suggestion unconditionally to the Instant search base
    // page. If there is no suggestion to prefetch, we need to send a blank
    // query to clear the prefetched results.
    InstantSearchPrerenderer* prerenderer =
        InstantSearchPrerenderer::GetForProfile(profile_);
    if (prerenderer)
      prerenderer->Prerender(prefetch_suggestion);
  }
  if (!inside_synchronous_start_)
    NotifySuggestionsReceived(autocomplete_controller_->result());
}

void AutocompleteControllerAndroid::NotifySuggestionsReceived(
    const AutocompleteResult& autocomplete_result) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> java_bridge =
      weak_java_autocomplete_controller_android_.get(env);
  if (!java_bridge.obj())
    return;

  ScopedJavaLocalRef<jobject> suggestion_list_obj =
      Java_AutocompleteController_createOmniboxSuggestionList(
          env, autocomplete_result.size());
  for (size_t i = 0; i < autocomplete_result.size(); ++i) {
    ScopedJavaLocalRef<jobject> j_omnibox_suggestion =
        BuildOmniboxSuggestion(env, autocomplete_result.match_at(i));
    Java_AutocompleteController_addOmniboxSuggestionToList(
        env, suggestion_list_obj.obj(), j_omnibox_suggestion.obj());
  }

  // Get the inline-autocomplete text.
  const AutocompleteResult::const_iterator default_match(
      autocomplete_result.default_match());
  base::string16 inline_autocomplete_text;
  if (default_match != autocomplete_result.end()) {
    inline_autocomplete_text = default_match->inline_autocompletion;
  }
  ScopedJavaLocalRef<jstring> inline_text =
      ConvertUTF16ToJavaString(env, inline_autocomplete_text);
  jlong j_autocomplete_result =
      reinterpret_cast<intptr_t>(&(autocomplete_result));
  Java_AutocompleteController_onSuggestionsReceived(env,
                                                    java_bridge.obj(),
                                                    suggestion_list_obj.obj(),
                                                    inline_text.obj(),
                                                    j_autocomplete_result);
}

OmniboxEventProto::PageClassification
AutocompleteControllerAndroid::ClassifyPage(const GURL& gurl,
                                            bool is_query_in_omnibox,
                                            bool focused_from_fakebox) const {
  if (!gurl.is_valid())
    return OmniboxEventProto::INVALID_SPEC;

  const std::string& url = gurl.spec();

  if (gurl.SchemeIs(content::kChromeUIScheme) &&
      gurl.host() == chrome::kChromeUINewTabHost) {
    return OmniboxEventProto::NTP;
  }

  if (url == chrome::kChromeUINativeNewTabURL) {
    return focused_from_fakebox ?
        OmniboxEventProto::INSTANT_NTP_WITH_FAKEBOX_AS_STARTING_FOCUS :
        OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS;
  }

  if (url == url::kAboutBlankURL)
    return OmniboxEventProto::BLANK;

  if (url == profile_->GetPrefs()->GetString(prefs::kHomePage))
    return OmniboxEventProto::HOME_PAGE;

  if (is_query_in_omnibox)
    return OmniboxEventProto::SEARCH_RESULT_PAGE_DOING_SEARCH_TERM_REPLACEMENT;

  bool is_search_url = TemplateURLServiceFactory::GetForProfile(profile_)->
      IsSearchResultsPageFromDefaultSearchProvider(gurl);
  if (is_search_url)
    return OmniboxEventProto::SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT;

  return OmniboxEventProto::OTHER;
}

ScopedJavaLocalRef<jobject>
AutocompleteControllerAndroid::BuildOmniboxSuggestion(
    JNIEnv* env,
    const AutocompleteMatch& match) {
  ScopedJavaLocalRef<jstring> contents =
      ConvertUTF16ToJavaString(env, match.contents);
  ScopedJavaLocalRef<jstring> description =
      ConvertUTF16ToJavaString(env, match.description);
  ScopedJavaLocalRef<jstring> answer_contents =
      ConvertUTF16ToJavaString(env, match.answer_contents);
  ScopedJavaLocalRef<jstring> answer_type =
      ConvertUTF16ToJavaString(env, match.answer_type);
  ScopedJavaLocalRef<jstring> fill_into_edit =
      ConvertUTF16ToJavaString(env, match.fill_into_edit);
  ScopedJavaLocalRef<jstring> destination_url =
      ConvertUTF8ToJavaString(env, match.destination_url.spec());
  // Note that we are also removing 'www' host from formatted url.
  ScopedJavaLocalRef<jstring> formatted_url = ConvertUTF16ToJavaString(env,
      FormatURLUsingAcceptLanguages(match.stripped_destination_url));
  BookmarkModel* bookmark_model = BookmarkModelFactory::GetForProfile(profile_);
  return Java_AutocompleteController_buildOmniboxSuggestion(
      env,
      match.type,
      match.relevance,
      match.transition,
      contents.obj(),
      description.obj(),
      answer_contents.obj(),
      answer_type.obj(),
      fill_into_edit.obj(),
      destination_url.obj(),
      formatted_url.obj(),
      bookmark_model && bookmark_model->IsBookmarked(match.destination_url),
      match.SupportsDeletion());
}

base::string16 AutocompleteControllerAndroid::FormatURLUsingAcceptLanguages(
    GURL url) {
  if (profile_ == NULL)
    return base::string16();

  std::string languages(
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages));

  return net::FormatUrl(url, languages, net::kFormatUrlOmitAll,
      net::UnescapeRule::SPACES, NULL, NULL, NULL);
}

ScopedJavaLocalRef<jobject>
AutocompleteControllerAndroid::GetTopSynchronousResult(
    JNIEnv* env,
    jobject obj,
    jstring j_text,
    bool prevent_inline_autocomplete) {
  if (!autocomplete_controller_)
    return ScopedJavaLocalRef<jobject>();

  inside_synchronous_start_ = true;
  Start(env,
        obj,
        j_text,
        NULL,
        NULL,
        prevent_inline_autocomplete,
        false,
        false,
        false);
  inside_synchronous_start_ = false;
  DCHECK(autocomplete_controller_->done());
  const AutocompleteResult& result = autocomplete_controller_->result();
  if (result.empty())
    return ScopedJavaLocalRef<jobject>();

  return BuildOmniboxSuggestion(env, *result.begin());
}

static jlong Init(JNIEnv* env, jobject obj, jobject jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  if (!profile)
    return 0;

  AutocompleteControllerAndroid* native_bridge =
      AutocompleteControllerAndroid::Factory::GetForProfile(profile, env, obj);
  return reinterpret_cast<intptr_t>(native_bridge);
}

static jstring QualifyPartialURLQuery(
    JNIEnv* env, jclass clazz, jstring jquery) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile)
    return NULL;
  AutocompleteMatch match;
  base::string16 query_string(ConvertJavaStringToUTF16(env, jquery));
  AutocompleteClassifierFactory::GetForProfile(profile)->Classify(
      query_string,
      false,
      false,
      OmniboxEventProto::INVALID_SPEC,
      &match,
      NULL);
  if (!match.destination_url.is_valid())
    return NULL;

  // Only return a URL if the match is a URL type.
  if (match.type != AutocompleteMatchType::URL_WHAT_YOU_TYPED &&
      match.type != AutocompleteMatchType::HISTORY_URL &&
      match.type != AutocompleteMatchType::NAVSUGGEST)
    return NULL;

  // As we are returning to Java, it is fine to call Release().
  return ConvertUTF8ToJavaString(env, match.destination_url.spec()).Release();
}

static void PrefetchZeroSuggestResults(JNIEnv* env, jclass clazz) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile)
    return;

  if (!OmniboxFieldTrial::InZeroSuggestPersonalizedFieldTrial())
    return;

  // ZeroSuggestPrefetcher deletes itself after it's done prefetching.
  new ZeroSuggestPrefetcher(profile);
}

// Register native methods
bool RegisterAutocompleteControllerAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
