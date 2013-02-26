// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/search.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/string_split.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/instant/instant_service.h"
#include "chrome/browser/instant/instant_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

namespace {

// Configuration options for Embedded Search.
// InstantExtended field trials are named in such a way that we can parse out
// the experiment configuration from the trial's group name in order to give
// us maximum flexability in running experiments.
// Field trial groups should be named things like "Group7 espv:2 instant:1".
// The first token is always GroupN for some integer N, followed by a
// space-delimited list of key:value pairs which correspond to these flags:
const char kEmbeddedPageVersionFlagName[] = "espv";
const uint64 kEmbeddedPageVersionDisabled = 0;
const uint64 kEmbeddedPageVersionDefault = 2;

const char kInstantExtendedActivationName[] = "instant";
const chrome::search::InstantExtendedDefault kInstantExtendedActivationDefault =
    chrome::search::INSTANT_DEFAULT_ON;

// Constants for the field trial name and group prefix.
const char kInstantExtendedFieldTrialName[] = "InstantExtended";
const char kGroupNumberPrefix[] = "Group";

// If the field trial's group name ends with this string its configuration will
// be ignored and Instant Extended will not be enabled by default.
const char kDisablingSuffix[] = "DISABLED";

chrome::search::InstantExtendedDefault InstantExtendedDefaultFromInt64(
    int64 default_value) {
  switch (default_value) {
    case 0: return chrome::search::INSTANT_DEFAULT_ON;
    case 1: return chrome::search::INSTANT_USE_EXISTING;
    case 2: return chrome::search::INSTANT_DEFAULT_OFF;
    default: return chrome::search::INSTANT_USE_EXISTING;
  }
}

TemplateURL* GetDefaultSearchProviderTemplateURL(Profile* profile) {
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  if (template_url_service)
    return template_url_service->GetDefaultSearchProvider();
  return NULL;
}

GURL TemplateURLRefToGURL(const TemplateURLRef& ref) {
  return GURL(
      ref.ReplaceSearchTerms(TemplateURLRef::SearchTermsArgs(string16())));
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
  GURL instant_url(cl->GetSwitchValueASCII(switches::kInstantURL));
  return instant_url.is_valid() && MatchesOriginAndPath(url, instant_url);
}

bool MatchesAnySearchURL(const GURL& url, TemplateURL* template_url) {
  GURL search_url = TemplateURLRefToGURL(template_url->url_ref());
  if (search_url.is_valid() && MatchesOriginAndPath(url, search_url))
    return true;

  // "URLCount() - 1" because we already tested url_ref above.
  for (size_t i = 0; i < template_url->URLCount() - 1; ++i) {
    TemplateURLRef ref(template_url, i);
    search_url = TemplateURLRefToGURL(ref);
    if (search_url.is_valid() && MatchesOriginAndPath(url, search_url))
      return true;
  }

  return false;
}

}  // namespace

namespace chrome {
namespace search {

const char kInstantExtendedSearchTermsKey[] = "search_terms";

const char kLocalOmniboxPopupURL[] =
    "chrome://local-omnibox-popup/local-omnibox-popup.html";

InstantExtendedDefault GetInstantExtendedDefaultSetting() {
  // Check the command-line/about:flags setting first, which should have
  // precedence and allows the trial to not be reported (if it's never queried).
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableInstantExtendedAPI))
    return chrome::search::INSTANT_DEFAULT_OFF;
  if (command_line->HasSwitch(switches::kEnableInstantExtendedAPI))
    return chrome::search::INSTANT_DEFAULT_ON;

  FieldTrialFlags flags;
  if (GetFieldTrialInfo(
          base::FieldTrialList::FindFullName(kInstantExtendedFieldTrialName),
          &flags, NULL)) {
    uint64 trial_default = GetUInt64ValueForFlagWithDefault(
                               kInstantExtendedActivationName,
                               kInstantExtendedActivationDefault,
                               flags);
    return InstantExtendedDefaultFromInt64(trial_default);
  }

  return kInstantExtendedActivationDefault;
}

bool IsInstantExtendedAPIEnabled(const Profile* profile) {
  return EmbeddedSearchPageVersion(profile) != kEmbeddedPageVersionDisabled;
}

// Determine what embedded search page version to request from the user's
// default search provider. If 0, the embedded search UI should not be enabled.
uint64 EmbeddedSearchPageVersion(const Profile* profile) {
  if (!profile || profile->IsOffTheRecord())
    return kEmbeddedPageVersionDisabled;

  // Check the command-line/about:flags setting first, which should have
  // precedence and allows the trial to not be reported (if it's never queried).
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableInstantExtendedAPI))
    return kEmbeddedPageVersionDisabled;
  if (command_line->HasSwitch(switches::kEnableInstantExtendedAPI)) {
    // The user has set the about:flags switch to Enabled - give the default
    // UI version.
    return kEmbeddedPageVersionDefault;
  }

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

bool IsQueryExtractionEnabled(const Profile* profile) {
#if defined(OS_IOS)
  const CommandLine* cl = CommandLine::ForCurrentProcess();
  return cl->HasSwitch(switches::kEnableQueryExtraction);
#else
  // On desktop, query extraction is controlled by the instant-extended-api
  // flag.
  return IsInstantExtendedAPIEnabled(profile);
#endif
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

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  if (!IsQueryExtractionEnabled(profile))
    return string16();

  // For security reasons, don't extract search terms if the page is not being
  // rendered in the privileged Instant renderer process. This is to protect
  // against a malicious page somehow scripting the search results page and
  // faking search terms in the URL. Random pages can't get into the Instant
  // renderer and scripting doesn't work cross-process, so if the page is in
  // the Instant process, we know it isn't being exploited.
  const content::RenderProcessHost* process_host =
      contents->GetRenderProcessHost();
  if (!process_host)
    return string16();

  const InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile);
  if (!instant_service)
    return string16();

  if (!instant_service->IsInstantProcess(process_host->GetID()))
    return string16();

  // Check to see if search terms have already been extracted.
  const content::NavigationEntry* entry =
      contents->GetController().GetVisibleEntry();
  if (!entry)
    return string16();

  string16 search_terms = GetSearchTermsFromNavigationEntry(entry);
  if (!search_terms.empty())
    return search_terms;

  // Otherwise, extract from the URL.
  TemplateURL* template_url = GetDefaultSearchProviderTemplateURL(profile);
  if (!template_url)
    return string16();

  GURL url = entry->GetVirtualURL();

  if (IsCommandLineInstantURL(url))
    url = CoerceCommandLineURLToTemplateURL(url, template_url->url_ref());

  if (url.SchemeIsSecure() && template_url->HasSearchTermsReplacementKey(url))
    template_url->ExtractSearchTermsFromURL(url, &search_terms);

  return search_terms;
}

bool ShouldAssignURLToInstantRenderer(const GURL& url, Profile* profile) {
  TemplateURL* template_url = GetDefaultSearchProviderTemplateURL(profile);
  if (!template_url)
    return false;

  GURL effective_url = url;

  if (IsCommandLineInstantURL(url)) {
    const TemplateURLRef& instant_url_ref = template_url->instant_url_ref();
    effective_url = CoerceCommandLineURLToTemplateURL(url, instant_url_ref);
  }

  return ShouldAssignURLToInstantRendererImpl(
             effective_url,
             IsInstantExtendedAPIEnabled(profile),
             template_url);
}

void EnableInstantExtendedAPIForTesting() {
  CommandLine* cl = CommandLine::ForCurrentProcess();
  cl->AppendSwitch(switches::kEnableInstantExtendedAPI);
}

void EnableQueryExtractionForTesting() {
#if defined(OS_IOS)
  CommandLine* cl = CommandLine::ForCurrentProcess();
  cl->AppendSwitch(switches::kEnableQueryExtraction);
#else
  EnableInstantExtendedAPIForTesting();
#endif
}

bool ShouldAssignURLToInstantRendererImpl(const GURL& url,
                                          bool extended_api_enabled,
                                          TemplateURL* template_url) {
  if (!url.is_valid())
    return false;

  if (url.SchemeIs(chrome::kChromeSearchScheme))
    return true;

  if (extended_api_enabled && url == GURL(kLocalOmniboxPopupURL))
    return true;

  if (extended_api_enabled && !url.SchemeIsSecure())
    return false;

  if (extended_api_enabled && !template_url->HasSearchTermsReplacementKey(url))
    return false;

  GURL instant_url = TemplateURLRefToGURL(template_url->instant_url_ref());
  if (!instant_url.is_valid())
    return false;

  if (MatchesOriginAndPath(url, instant_url))
    return true;

  if (extended_api_enabled && MatchesAnySearchURL(url, template_url))
    return true;

  return false;
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
                                       const TemplateURLRef& ref) {
  GURL search_url = TemplateURLRefToGURL(ref);
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
