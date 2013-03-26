// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

namespace chrome {
namespace search {

namespace {

// The default value we should assign to the instant_extended.enabled pref. As
// with other prefs, the default is used only when the user hasn't toggled the
// pref explicitly.
enum InstantExtendedDefault {
  INSTANT_DEFAULT_ON,    // Default the pref to be enabled.
  INSTANT_USE_EXISTING,  // Use the current value of the instant.enabled pref.
  INSTANT_DEFAULT_OFF,   // Default the pref to be disabled.
};

// Configuration options for Embedded Search.
// InstantExtended field trials are named in such a way that we can parse out
// the experiment configuration from the trial's group name in order to give
// us maximum flexability in running experiments.
// Field trial groups should be named things like "Group7 espv:2 instant:1".
// The first token is always GroupN for some integer N, followed by a
// space-delimited list of key:value pairs which correspond to these flags:
const char kEmbeddedPageVersionFlagName[] = "espv";
const uint64 kEmbeddedPageVersionDisabled = 0;
#if defined(OS_IOS) || defined(OS_ANDROID)
const uint64 kEmbeddedPageVersionDefault = 1;
#else
const uint64 kEmbeddedPageVersionDefault = 2;
#endif

const char kInstantExtendedActivationName[] = "instant";
const InstantExtendedDefault kInstantExtendedActivationDefault =
    INSTANT_DEFAULT_ON;

// Constants for the field trial name and group prefix.
const char kInstantExtendedFieldTrialName[] = "InstantExtended";
const char kGroupNumberPrefix[] = "Group";

// If the field trial's group name ends with this string its configuration will
// be ignored and Instant Extended will not be enabled by default.
const char kDisablingSuffix[] = "DISABLED";

TemplateURL* GetDefaultSearchProviderTemplateURL(Profile* profile) {
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  if (template_url_service)
    return template_url_service->GetDefaultSearchProvider();
  return NULL;
}

GURL TemplateURLRefToGURL(const TemplateURLRef& ref, int start_margin) {
  TemplateURLRef::SearchTermsArgs search_terms_args =
      TemplateURLRef::SearchTermsArgs(string16());
  search_terms_args.omnibox_start_margin = start_margin;
  return GURL(ref.ReplaceSearchTerms(search_terms_args));
}

bool MatchesOriginAndPath(const GURL& my_url, const GURL& other_url) {
  return my_url.host() == other_url.host() &&
         my_url.port() == other_url.port() &&
         my_url.path() == other_url.path() &&
         (my_url.scheme() == other_url.scheme() ||
          (my_url.SchemeIs(chrome::kHttpsScheme) &&
           other_url.SchemeIs(chrome::kHttpScheme)));
}

bool IsCommandLineInstantURL(const GURL& url) {
  const CommandLine* cl = CommandLine::ForCurrentProcess();
  const GURL instant_url(cl->GetSwitchValueASCII(switches::kInstantURL));
  return instant_url.is_valid() && MatchesOriginAndPath(url, instant_url);
}

bool MatchesAnySearchURL(const GURL& url, TemplateURL* template_url) {
  GURL search_url =
      TemplateURLRefToGURL(template_url->url_ref(), kDisableStartMargin);
  if (search_url.is_valid() && MatchesOriginAndPath(url, search_url))
    return true;

  // "URLCount() - 1" because we already tested url_ref above.
  for (size_t i = 0; i < template_url->URLCount() - 1; ++i) {
    TemplateURLRef ref(template_url, i);
    search_url = TemplateURLRefToGURL(ref, kDisableStartMargin);
    if (search_url.is_valid() && MatchesOriginAndPath(url, search_url))
      return true;
  }

  return false;
}

enum OptInState {
  NOT_SET,  // The user has not manually opted into or out of InstantExtended.
  OPT_IN,   // The user has opted-in to InstantExtended.
  OPT_OUT,  // The user has opted-out of InstantExtended.
  OPT_IN_STATE_ENUM_COUNT,
};

void RecordInstantExtendedOptInState(OptInState state) {
  static bool recorded = false;
  if (!recorded) {
    recorded = true;
    UMA_HISTOGRAM_ENUMERATION("InstantExtended.OptInState", state,
                              OPT_IN_STATE_ENUM_COUNT);
  }
}

// Returns true if |contents| is rendered inside the Instant process for
// |profile|.
bool IsRenderedInInstantProcess(const content::WebContents* contents,
                                Profile* profile) {
  const content::RenderProcessHost* process_host =
      contents->GetRenderProcessHost();
  if (!process_host)
    return false;

  const InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile);
  if (!instant_service)
    return false;

  return instant_service->IsInstantProcess(process_host->GetID());
}

// Returns true if |url| can be used as an Instant URL for |profile|.
bool IsInstantURL(const GURL& url, Profile* profile) {
  TemplateURL* template_url = GetDefaultSearchProviderTemplateURL(profile);
  if (!template_url)
    return false;

  const TemplateURLRef& instant_url_ref = template_url->instant_url_ref();
  const bool extended_api_enabled = IsInstantExtendedAPIEnabled();
  GURL effective_url = url;

  if (IsCommandLineInstantURL(url))
    effective_url = CoerceCommandLineURLToTemplateURL(url, instant_url_ref,
                                                      kDisableStartMargin);

  if (!effective_url.is_valid())
    return false;

  if (extended_api_enabled && !effective_url.SchemeIsSecure())
    return false;

  if (extended_api_enabled &&
      !template_url->HasSearchTermsReplacementKey(effective_url))
    return false;

  const GURL instant_url =
      TemplateURLRefToGURL(instant_url_ref, kDisableStartMargin);
  if (!instant_url.is_valid())
    return false;

  if (MatchesOriginAndPath(effective_url, instant_url))
    return true;

  if (extended_api_enabled && MatchesAnySearchURL(effective_url, template_url))
    return true;

  return false;
}

string16 GetSearchTermsImpl(const content::WebContents* contents,
                            const content::NavigationEntry* entry) {
  if (!IsQueryExtractionEnabled())
    return string16();

  // For security reasons, don't extract search terms if the page is not being
  // rendered in the privileged Instant renderer process. This is to protect
  // against a malicious page somehow scripting the search results page and
  // faking search terms in the URL. Random pages can't get into the Instant
  // renderer and scripting doesn't work cross-process, so if the page is in
  // the Instant process, we know it isn't being exploited.
  // Since iOS and Android doesn't use the instant framework, these checks are
  // disabled for the two platforms.
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
#if !defined(OS_IOS) && !defined(OS_ANDROID)
  if (!IsRenderedInInstantProcess(contents, profile) &&
      (contents->GetController().GetLastCommittedEntry() == entry ||
       !ShouldAssignURLToInstantRenderer(entry->GetURL(), profile)))
    return string16();
#endif  // !defined(OS_IOS) && !defined(OS_ANDROID)
  // Check to see if search terms have already been extracted.
  string16 search_terms = GetSearchTermsFromNavigationEntry(entry);
  if (!search_terms.empty())
    return search_terms;

  // Otherwise, extract from the URL.
  TemplateURL* template_url = GetDefaultSearchProviderTemplateURL(profile);
  if (!template_url)
    return string16();

  GURL url = entry->GetVirtualURL();

  if (IsCommandLineInstantURL(url))
    url = CoerceCommandLineURLToTemplateURL(url, template_url->url_ref(),
                                            kDisableStartMargin);

  if (url.SchemeIsSecure() && template_url->HasSearchTermsReplacementKey(url))
    template_url->ExtractSearchTermsFromURL(url, &search_terms);

  return search_terms;
}

}  // namespace

const char kInstantExtendedSearchTermsKey[] = "search_terms";

// Negative start-margin values prevent the "es_sm" parameter from being used.
const int kDisableStartMargin = -1;

bool IsInstantExtendedAPIEnabled() {
#if defined(OS_IOS) || defined(OS_ANDROID)
  return false;
#else
  // On desktop, query extraction is part of Instant extended, so if one is
  // enabled, the other is too.
  return IsQueryExtractionEnabled();
#endif  // defined(OS_IOS) || defined(OS_ANDROID)
}

// Determine what embedded search page version to request from the user's
// default search provider. If 0, the embedded search UI should not be enabled.
uint64 EmbeddedSearchPageVersion() {
  // Check the command-line/about:flags setting first, which should have
  // precedence and allows the trial to not be reported (if it's never queried).
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableInstantExtendedAPI)) {
    RecordInstantExtendedOptInState(OPT_OUT);
    return kEmbeddedPageVersionDisabled;
  }
  if (command_line->HasSwitch(switches::kEnableInstantExtendedAPI)) {
    // The user has set the about:flags switch to Enabled - give the default
    // UI version.
    RecordInstantExtendedOptInState(OPT_IN);
    return kEmbeddedPageVersionDefault;
  }

  RecordInstantExtendedOptInState(NOT_SET);
  FieldTrialFlags flags;
  if (GetFieldTrialInfo(
          base::FieldTrialList::FindFullName(kInstantExtendedFieldTrialName),
          &flags, NULL)) {
    return GetUInt64ValueForFlagWithDefault(kEmbeddedPageVersionFlagName,
                                            kEmbeddedPageVersionDefault,
                                            flags);
  }
  return kEmbeddedPageVersionDisabled;
}

bool IsQueryExtractionEnabled() {
  return EmbeddedSearchPageVersion() != kEmbeddedPageVersionDisabled;
}

string16 GetSearchTermsFromNavigationEntry(
    const content::NavigationEntry* entry) {
  string16 search_terms;
  if (entry)
    entry->GetExtraData(kInstantExtendedSearchTermsKey, &search_terms);
  return search_terms;
}

string16 GetSearchTerms(const content::WebContents* contents) {
  if (!contents)
    return string16();

  const content::NavigationEntry* entry =
      contents->GetController().GetVisibleEntry();
  if (!entry)
    return string16();

  return GetSearchTermsImpl(contents, entry);
}

bool IsInstantNTP(const content::WebContents* contents) {
  if (!contents)
    return false;

  return NavEntryIsInstantNTP(contents,
                              contents->GetController().GetVisibleEntry());
}

bool NavEntryIsInstantNTP(const content::WebContents* contents,
                          const content::NavigationEntry* entry) {
  if (!contents || !entry)
    return false;

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  return IsInstantExtendedAPIEnabled() &&
         IsRenderedInInstantProcess(contents, profile) &&
         (IsInstantURL(entry->GetVirtualURL(), profile) ||
          entry->GetVirtualURL() == GURL(chrome::kChromeSearchLocalNtpUrl)) &&
         GetSearchTermsImpl(contents, entry).empty();
}

bool ShouldAssignURLToInstantRenderer(const GURL& url, Profile* profile) {
  return url.is_valid() &&
         profile &&
         (url.SchemeIs(chrome::kChromeSearchScheme) ||
          IsInstantURL(url, profile));
}

void RegisterUserPrefs(PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kInstantConfirmDialogShown, false,
                                PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kInstantEnabled, false,
                                PrefRegistrySyncable::SYNCABLE_PREF);
  // This default is overridden by SetInstantExtendedPrefDefault().
  registry->RegisterBooleanPref(prefs::kInstantExtendedEnabled, false,
                                PrefRegistrySyncable::SYNCABLE_PREF);
}

const char* GetInstantPrefName() {
  return IsInstantExtendedAPIEnabled() ? prefs::kInstantExtendedEnabled :
                                         prefs::kInstantEnabled;
}

bool IsInstantPrefEnabled(Profile* profile) {
  if (!profile || profile->IsOffTheRecord())
    return false;

  const PrefService* prefs = profile->GetPrefs();
  if (!prefs)
    return false;

  const char* pref_name = GetInstantPrefName();
  const bool pref_value = prefs->GetBoolean(pref_name);

  if (pref_name == prefs::kInstantExtendedEnabled) {
    // Note that this is only recorded for the first profile that calls this
    // code (which happens on startup).
    static bool recorded = false;
    if (!recorded) {
      UMA_HISTOGRAM_BOOLEAN("InstantExtended.PrefValue", pref_value);
      recorded = true;
    }
  }

  return pref_value;
}

void SetInstantExtendedPrefDefault(Profile* profile) {
  PrefService* prefs = profile ? profile->GetPrefs() : NULL;
  if (!prefs)
    return;

  bool pref_default = false;

  // Check the command-line/about:flags setting first, which should have
  // precedence and allows the trial to not be reported (if it's never queried).
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableInstantExtendedAPI)) {
    pref_default = true;
  } else if (!command_line->HasSwitch(switches::kDisableInstantExtendedAPI)) {
    uint64 trial_default = kInstantExtendedActivationDefault;

    FieldTrialFlags flags;
    if (GetFieldTrialInfo(
            base::FieldTrialList::FindFullName(kInstantExtendedFieldTrialName),
            &flags, NULL)) {
      trial_default = GetUInt64ValueForFlagWithDefault(
                          kInstantExtendedActivationName,
                          kInstantExtendedActivationDefault,
                          flags);
    }

    if (trial_default == INSTANT_DEFAULT_ON) {
      pref_default = true;
    } else if (trial_default != INSTANT_DEFAULT_OFF) {
      pref_default = prefs->GetBoolean(prefs::kInstantEnabled);
    }
  }

  prefs->SetDefaultPrefValue(prefs::kInstantExtendedEnabled,
                             Value::CreateBooleanValue(pref_default));
}

GURL GetInstantURL(Profile* profile, int start_margin) {
  const bool extended_api_enabled = IsInstantExtendedAPIEnabled();

  const PrefService* prefs = profile && !profile->IsOffTheRecord() ?
      profile->GetPrefs() : NULL;
  if (!IsInstantPrefEnabled(profile) &&
      !(extended_api_enabled && prefs &&
        prefs->GetBoolean(prefs::kSearchSuggestEnabled)))
    return GURL();

  TemplateURL* template_url = GetDefaultSearchProviderTemplateURL(profile);
  if (!template_url)
    return GURL();

  CommandLine* cl = CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(switches::kInstantURL)) {
    GURL instant_url(cl->GetSwitchValueASCII(switches::kInstantURL));
    if (extended_api_enabled) {
      // Extended mode won't work if the search terms replacement key is absent.
      GURL coerced_url = CoerceCommandLineURLToTemplateURL(
          instant_url, template_url->instant_url_ref(), start_margin);
      if (!template_url->HasSearchTermsReplacementKey(coerced_url))
        return GURL();
    }
    return instant_url;
  }

  GURL instant_url =
      TemplateURLRefToGURL(template_url->instant_url_ref(), start_margin);

  if (extended_api_enabled) {
    // Extended mode won't work if the search terms replacement key is absent.
    if (!template_url->HasSearchTermsReplacementKey(instant_url))
      return GURL();

    // Extended mode requires HTTPS. Force it if necessary.
    if (!instant_url.SchemeIsSecure()) {
      const std::string secure_scheme = chrome::kHttpsScheme;
      GURL::Replacements replacements;
      replacements.SetSchemeStr(secure_scheme);
      instant_url = instant_url.ReplaceComponents(replacements);
    }
  }

  return instant_url;
}

bool IsInstantEnabled(Profile* profile) {
  return GetInstantURL(profile, kDisableStartMargin).is_valid();
}

void EnableInstantExtendedAPIForTesting() {
  CommandLine* cl = CommandLine::ForCurrentProcess();
  cl->AppendSwitch(switches::kEnableInstantExtendedAPI);
}

bool GetFieldTrialInfo(const std::string& group_name,
                       FieldTrialFlags* flags,
                       uint64* group_number) {
  if (EndsWith(group_name, kDisablingSuffix, true) ||
      !StartsWithASCII(group_name, kGroupNumberPrefix, true))
    return false;

  // We have a valid trial that starts with "Group" and isn't disabled.
  // First extract the flags.
  std::string group_prefix(group_name);

  size_t first_space = group_name.find(" ");
  if (first_space != std::string::npos) {
    // There is a flags section of the group name. Split that out and parse it.
    group_prefix = group_name.substr(0, first_space);
    if (!base::SplitStringIntoKeyValuePairs(group_name.substr(first_space),
                                            ':', ' ', flags)) {
      // Failed to parse the flags section. Assume the whole group name is
      // invalid.
      return false;
    }
  }

  // Now extract the group number, making sure we get a non-zero value.
  uint64 temp_group_number = 0;
  std::string group_suffix = group_prefix.substr(strlen(kGroupNumberPrefix));
  if (!base::StringToUint64(group_suffix, &temp_group_number) ||
      temp_group_number == 0)
    return false;

  if (group_number)
    *group_number = temp_group_number;

  return true;
}

// Given a FieldTrialFlags object, returns the string value of the provided
// flag.
std::string GetStringValueForFlagWithDefault(const std::string& flag,
                                             const std::string& default_value,
                                             const FieldTrialFlags& flags) {
  FieldTrialFlags::const_iterator i;
  for (i = flags.begin(); i != flags.end(); i++) {
    if (i->first == flag)
      return i->second;
  }
  return default_value;
}

// Given a FieldTrialFlags object, returns the uint64 value of the provided
// flag.
uint64 GetUInt64ValueForFlagWithDefault(const std::string& flag,
                                        uint64 default_value,
                                        const FieldTrialFlags& flags) {
  uint64 value;
  std::string str_value = GetStringValueForFlagWithDefault(flag, "", flags);
  if (base::StringToUint64(str_value, &value))
    return value;
  return default_value;
}

// Given a FieldTrialFlags object, returns the boolean value of the provided
// flag.
bool GetBoolValueForFlagWithDefault(const std::string& flag,
                                    bool default_value,
                                    const FieldTrialFlags& flags) {
  return !!GetUInt64ValueForFlagWithDefault(flag, default_value ? 1 : 0, flags);
}

// Coerces the commandline Instant URL to look like a template URL, so that we
// can extract search terms from it.
GURL CoerceCommandLineURLToTemplateURL(const GURL& instant_url,
                                       const TemplateURLRef& ref,
                                       int start_margin) {
  GURL search_url = TemplateURLRefToGURL(ref, start_margin);

  // NOTE(samarth): GURL returns temporaries which we must save because
  // GURL::Replacements expects the replacements to live until
  // ReplaceComponents is called.
  const std::string search_scheme = chrome::kHttpsScheme;
  const std::string search_host = search_url.host();
  const std::string search_port = search_url.port();
  const std::string search_path = search_url.path();

  GURL::Replacements replacements;
  replacements.SetSchemeStr(search_scheme);
  replacements.SetHostStr(search_host);
  replacements.SetPortStr(search_port);
  replacements.SetPathStr(search_path);
  return instant_url.ReplaceComponents(replacements);
}

}  // namespace search
}  // namespace chrome
