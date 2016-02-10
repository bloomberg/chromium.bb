// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/error_page/common/localized_error.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/error_page/common/error_page_params.h"
#include "components/error_page/common/error_page_switches.h"
#include "components/error_page/common/net_error_info.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_google_chrome_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

#if defined(OS_ANDROID)
#include "components/offline_pages/offline_page_feature.h"
#endif

namespace error_page {

namespace {

// Some error pages have no details.
const unsigned int kErrorPagesNoDetails = 0;

static const char kRedirectLoopLearnMoreUrl[] =
    "https://support.google.com/chrome/answer/95626";
static const char kWeakDHKeyLearnMoreUrl[] =
    "https://support.google.com/chrome?p=dh_error";
static const int kGoogleCachedCopySuggestionType = 0;

enum NAV_SUGGESTIONS {
  SUGGEST_NONE                  = 0,
  SUGGEST_RELOAD                = 1 << 0,
  SUGGEST_CHECK_CONNECTION      = 1 << 1,
  SUGGEST_DNS_CONFIG            = 1 << 2,
  SUGGEST_FIREWALL_CONFIG       = 1 << 3,
  SUGGEST_PROXY_CONFIG          = 1 << 4,
  SUGGEST_DISABLE_EXTENSION     = 1 << 5,
  SUGGEST_LEARNMORE             = 1 << 6,
  SUGGEST_VIEW_POLICIES         = 1 << 7,
  SUGGEST_CONTACT_ADMINISTRATOR = 1 << 8,
  SUGGEST_UNSUPPORTED_CIPHER    = 1 << 9,
};

struct LocalizedErrorMap {
  int error_code;
  unsigned int title_resource_id;
  unsigned int heading_resource_id;
  // Detailed summary used when the error is in the main frame.
  unsigned int summary_resource_id;
  // Short one sentence description shown on mouse over when the error is in
  // a frame.
  unsigned int details_resource_id;
  int suggestions;  // Bitmap of SUGGEST_* values.
  unsigned int error_explanation_id;
};

const LocalizedErrorMap net_error_options[] = {
  {net::ERR_TIMED_OUT,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_TIMED_OUT,
   IDS_ERRORPAGES_DETAILS_TIMED_OUT,
   SUGGEST_RELOAD | SUGGEST_CHECK_CONNECTION | SUGGEST_FIREWALL_CONFIG |
       SUGGEST_PROXY_CONFIG,
  },
  {net::ERR_CONNECTION_TIMED_OUT,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_TIMED_OUT,
   IDS_ERRORPAGES_DETAILS_TIMED_OUT,
   SUGGEST_RELOAD | SUGGEST_CHECK_CONNECTION | SUGGEST_FIREWALL_CONFIG |
       SUGGEST_PROXY_CONFIG,
  },
  {net::ERR_CONNECTION_CLOSED,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_CONNECTION_CLOSED,
   IDS_ERRORPAGES_DETAILS_CONNECTION_CLOSED,
   SUGGEST_RELOAD,
  },
  {net::ERR_CONNECTION_RESET,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_CONNECTION_RESET,
   IDS_ERRORPAGES_DETAILS_CONNECTION_RESET,
   SUGGEST_RELOAD | SUGGEST_CHECK_CONNECTION | SUGGEST_FIREWALL_CONFIG |
       SUGGEST_PROXY_CONFIG,
  },
  {net::ERR_CONNECTION_REFUSED,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_CONNECTION_REFUSED,
   IDS_ERRORPAGES_DETAILS_CONNECTION_REFUSED,
   SUGGEST_RELOAD | SUGGEST_CHECK_CONNECTION | SUGGEST_FIREWALL_CONFIG |
       SUGGEST_PROXY_CONFIG,
  },
  {net::ERR_CONNECTION_FAILED,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_CONNECTION_FAILED,
   IDS_ERRORPAGES_DETAILS_CONNECTION_FAILED,
   SUGGEST_RELOAD,
  },
  {net::ERR_NAME_NOT_RESOLVED,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_NAME_NOT_RESOLVED,
   IDS_ERRORPAGES_DETAILS_NAME_NOT_RESOLVED,
   SUGGEST_RELOAD | SUGGEST_CHECK_CONNECTION | SUGGEST_DNS_CONFIG |
       SUGGEST_FIREWALL_CONFIG | SUGGEST_PROXY_CONFIG,
  },
  {net::ERR_ICANN_NAME_COLLISION,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_ICANN_NAME_COLLISION,
   IDS_ERRORPAGES_DETAILS_ICANN_NAME_COLLISION,
   SUGGEST_NONE,
  },
  {net::ERR_ADDRESS_UNREACHABLE,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_ADDRESS_UNREACHABLE,
   IDS_ERRORPAGES_DETAILS_ADDRESS_UNREACHABLE,
   SUGGEST_RELOAD | SUGGEST_FIREWALL_CONFIG | SUGGEST_PROXY_CONFIG,
  },
  {net::ERR_NETWORK_ACCESS_DENIED,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_NETWORK_ACCESS_DENIED,
   IDS_ERRORPAGES_SUMMARY_NETWORK_ACCESS_DENIED,
   IDS_ERRORPAGES_DETAILS_NETWORK_ACCESS_DENIED,
   SUGGEST_FIREWALL_CONFIG,
  },
  {net::ERR_PROXY_CONNECTION_FAILED,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_INTERNET_DISCONNECTED,
   IDS_ERRORPAGES_SUMMARY_PROXY_CONNECTION_FAILED,
   IDS_ERRORPAGES_DETAILS_PROXY_CONNECTION_FAILED,
   SUGGEST_PROXY_CONFIG,
  },
  {net::ERR_INTERNET_DISCONNECTED,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_INTERNET_DISCONNECTED,
   IDS_ERRORPAGES_SUMMARY_INTERNET_DISCONNECTED,
   IDS_ERRORPAGES_DETAILS_INTERNET_DISCONNECTED,
   SUGGEST_CHECK_CONNECTION | SUGGEST_FIREWALL_CONFIG,
  },
  {net::ERR_FILE_NOT_FOUND,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_FILE_NOT_FOUND,
   IDS_ERRORPAGES_SUMMARY_FILE_NOT_FOUND,
   IDS_ERRORPAGES_DETAILS_FILE_NOT_FOUND,
   SUGGEST_RELOAD,
  },
  {net::ERR_CACHE_MISS,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_CACHE_READ_FAILURE,
   IDS_ERRORPAGES_SUMMARY_CACHE_READ_FAILURE,
   IDS_ERRORPAGES_DETAILS_CACHE_READ_FAILURE,
   SUGGEST_RELOAD,
  },
  {net::ERR_CACHE_READ_FAILURE,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_CACHE_READ_FAILURE,
   IDS_ERRORPAGES_SUMMARY_CACHE_READ_FAILURE,
   IDS_ERRORPAGES_DETAILS_CACHE_READ_FAILURE,
   SUGGEST_RELOAD,
  },
  {net::ERR_NETWORK_IO_SUSPENDED,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_CONNECTION_INTERRUPTED,
   IDS_ERRORPAGES_SUMMARY_NETWORK_IO_SUSPENDED,
   IDS_ERRORPAGES_DETAILS_NETWORK_IO_SUSPENDED,
   SUGGEST_RELOAD,
  },
  {net::ERR_TOO_MANY_REDIRECTS,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_PAGE_NOT_WORKING,
   IDS_ERRORPAGES_SUMMARY_TOO_MANY_REDIRECTS,
   IDS_ERRORPAGES_DETAILS_TOO_MANY_REDIRECTS,
   SUGGEST_RELOAD | SUGGEST_LEARNMORE,
  },
  {net::ERR_EMPTY_RESPONSE,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_PAGE_NOT_WORKING,
   IDS_ERRORPAGES_SUMMARY_EMPTY_RESPONSE,
   IDS_ERRORPAGES_DETAILS_EMPTY_RESPONSE,
   SUGGEST_RELOAD,
  },
  {net::ERR_RESPONSE_HEADERS_MULTIPLE_CONTENT_LENGTH,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_PAGE_NOT_WORKING,
   IDS_ERRORPAGES_SUMMARY_INVALID_RESPONSE,
   IDS_ERRORPAGES_DETAILS_RESPONSE_HEADERS_MULTIPLE_CONTENT_LENGTH,
   SUGGEST_RELOAD,
  },
  {net::ERR_RESPONSE_HEADERS_MULTIPLE_CONTENT_DISPOSITION,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_PAGE_NOT_WORKING,
   IDS_ERRORPAGES_SUMMARY_INVALID_RESPONSE,
   IDS_ERRORPAGES_DETAILS_RESPONSE_HEADERS_MULTIPLE_CONTENT_DISPOSITION,
   SUGGEST_RELOAD,
  },
  {net::ERR_RESPONSE_HEADERS_MULTIPLE_LOCATION,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_PAGE_NOT_WORKING,
   IDS_ERRORPAGES_SUMMARY_INVALID_RESPONSE,
   IDS_ERRORPAGES_DETAILS_RESPONSE_HEADERS_MULTIPLE_LOCATION,
   SUGGEST_RELOAD,
  },
  {net::ERR_CONTENT_LENGTH_MISMATCH,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_PAGE_NOT_WORKING,
   IDS_ERRORPAGES_SUMMARY_CONNECTION_CLOSED,
   IDS_ERRORPAGES_DETAILS_CONNECTION_CLOSED,
   SUGGEST_RELOAD,
  },
  {net::ERR_INCOMPLETE_CHUNKED_ENCODING,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_PAGE_NOT_WORKING,
   IDS_ERRORPAGES_SUMMARY_CONNECTION_CLOSED,
   IDS_ERRORPAGES_DETAILS_CONNECTION_CLOSED,
   SUGGEST_RELOAD,
  },
  {net::ERR_SSL_PROTOCOL_ERROR,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_INSECURE_CONNECTION,
   IDS_ERRORPAGES_SUMMARY_INVALID_RESPONSE,
   IDS_ERRORPAGES_DETAILS_SSL_PROTOCOL_ERROR,
   SUGGEST_NONE,
  },
  {net::ERR_BAD_SSL_CLIENT_AUTH_CERT,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_INSECURE_CONNECTION,
   IDS_ERRORPAGES_SUMMARY_BAD_SSL_CLIENT_AUTH_CERT,
   IDS_ERRORPAGES_DETAILS_BAD_SSL_CLIENT_AUTH_CERT,
   SUGGEST_NONE,
  },
  {net::ERR_SSL_WEAK_SERVER_EPHEMERAL_DH_KEY,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_INSECURE_CONNECTION,
   IDS_ERRORPAGES_SUMMARY_SSL_SECURITY_ERROR,
   IDS_ERRORPAGES_DETAILS_SSL_PROTOCOL_ERROR,
   SUGGEST_LEARNMORE,
  },
  {net::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_INSECURE_CONNECTION,
   IDS_CERT_ERROR_SUMMARY_PINNING_FAILURE_DETAILS,
   IDS_CERT_ERROR_SUMMARY_PINNING_FAILURE_DESCRIPTION,
   SUGGEST_NONE,
  },
  {net::ERR_TEMPORARILY_THROTTLED,
   IDS_ERRORPAGES_TITLE_ACCESS_DENIED,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_NOT_AVAILABLE,
   IDS_ERRORPAGES_DETAILS_TEMPORARILY_THROTTLED,
   SUGGEST_NONE,
  },
  {net::ERR_BLOCKED_BY_CLIENT,
   IDS_ERRORPAGES_TITLE_BLOCKED,
   IDS_ERRORPAGES_HEADING_BLOCKED,
   IDS_ERRORPAGES_SUMMARY_BLOCKED_BY_EXTENSION,
   IDS_ERRORPAGES_DETAILS_BLOCKED_BY_EXTENSION,
   SUGGEST_RELOAD | SUGGEST_DISABLE_EXTENSION,
  },
  {net::ERR_NETWORK_CHANGED,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_CONNECTION_INTERRUPTED,
   IDS_ERRORPAGES_SUMMARY_NETWORK_CHANGED,
   IDS_ERRORPAGES_DETAILS_NETWORK_CHANGED,
   SUGGEST_RELOAD | SUGGEST_CHECK_CONNECTION,
  },
  {net::ERR_BLOCKED_BY_ADMINISTRATOR,
   IDS_ERRORPAGES_TITLE_BLOCKED,
   IDS_ERRORPAGES_HEADING_BLOCKED,
   IDS_ERRORPAGES_SUMMARY_BLOCKED_BY_ADMINISTRATOR,
   IDS_ERRORPAGES_DETAILS_BLOCKED_BY_ADMINISTRATOR,
   SUGGEST_VIEW_POLICIES | SUGGEST_CONTACT_ADMINISTRATOR,
  },
  {net::ERR_BLOCKED_ENROLLMENT_CHECK_PENDING,
   IDS_ERRORPAGES_TITLE_BLOCKED,
   IDS_ERRORPAGES_HEADING_INTERNET_DISCONNECTED,
   IDS_ERRORPAGES_SUMMARY_BLOCKED_ENROLLMENT_CHECK_PENDING,
   IDS_ERRORPAGES_DETAILS_BLOCKED_ENROLLMENT_CHECK_PENDING,
   SUGGEST_CHECK_CONNECTION,
  },
  {net::ERR_SSL_FALLBACK_BEYOND_MINIMUM_VERSION,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_INSECURE_CONNECTION,
   IDS_ERRORPAGES_SUMMARY_SSL_SECURITY_ERROR,
   IDS_ERRORPAGES_DETAILS_SSL_FALLBACK_BEYOND_MINIMUM_VERSION,
   SUGGEST_NONE,
  },
  {net::ERR_SSL_VERSION_OR_CIPHER_MISMATCH,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_INSECURE_CONNECTION,
   IDS_ERRORPAGES_SUMMARY_SSL_VERSION_OR_CIPHER_MISMATCH,
   IDS_ERRORPAGES_DETAILS_SSL_VERSION_OR_CIPHER_MISMATCH,
   SUGGEST_UNSUPPORTED_CIPHER,
  },
  {net::ERR_TEMPORARY_BACKOFF,
   IDS_ERRORPAGES_TITLE_ACCESS_DENIED,
   IDS_ERRORPAGES_HEADING_ACCESS_DENIED,
   IDS_ERRORPAGES_SUMMARY_TEMPORARY_BACKOFF,
   IDS_ERRORPAGES_DETAILS_TEMPORARY_BACKOFF,
   SUGGEST_NONE,
  },
  {net::ERR_SSL_SERVER_CERT_BAD_FORMAT,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_INSECURE_CONNECTION,
   IDS_ERRORPAGES_SUMMARY_SSL_SECURITY_ERROR,
   IDS_ERRORPAGES_DETAILS_SSL_PROTOCOL_ERROR,
   SUGGEST_NONE,
  },
};

// Special error page to be used in the case of navigating back to a page
// generated by a POST.  LocalizedError::HasStrings expects this net error code
// to also appear in the array above.
const LocalizedErrorMap repost_error = {
  net::ERR_CACHE_MISS,
  IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
  IDS_HTTP_POST_WARNING_TITLE,
  IDS_ERRORPAGES_HTTP_POST_WARNING,
  IDS_ERRORPAGES_DETAILS_CACHE_READ_FAILURE,
  SUGGEST_RELOAD,
};

const LocalizedErrorMap http_error_options[] = {
  {403,
   IDS_ERRORPAGES_TITLE_ACCESS_DENIED,
   IDS_ERRORPAGES_HEADING_ACCESS_DENIED,
   IDS_ERRORPAGES_SUMMARY_FORBIDDEN,
   IDS_ERRORPAGES_DETAILS_FORBIDDEN,
   SUGGEST_NONE,
  },
  {410,
   IDS_ERRORPAGES_TITLE_NOT_FOUND,
   IDS_ERRORPAGES_HEADING_NOT_FOUND,
   IDS_ERRORPAGES_SUMMARY_GONE,
   IDS_ERRORPAGES_DETAILS_GONE,
   SUGGEST_NONE,
  },

  {500,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_PAGE_NOT_WORKING,
   IDS_ERRORPAGES_SUMMARY_WEBSITE_CANNOT_HANDLE_REQUEST,
   IDS_ERRORPAGES_DETAILS_INTERNAL_SERVER_ERROR,
   SUGGEST_RELOAD,
  },
  {501,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_PAGE_NOT_WORKING,
   IDS_ERRORPAGES_SUMMARY_WEBSITE_CANNOT_HANDLE_REQUEST,
   IDS_ERRORPAGES_DETAILS_NOT_IMPLEMENTED,
   SUGGEST_NONE,
  },
  {502,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_PAGE_NOT_WORKING,
   IDS_ERRORPAGES_SUMMARY_WEBSITE_CANNOT_HANDLE_REQUEST,
   IDS_ERRORPAGES_DETAILS_BAD_GATEWAY,
   SUGGEST_RELOAD,
  },
  {503,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_PAGE_NOT_WORKING,
   IDS_ERRORPAGES_SUMMARY_WEBSITE_CANNOT_HANDLE_REQUEST,
   IDS_ERRORPAGES_DETAILS_SERVICE_UNAVAILABLE,
   SUGGEST_RELOAD,
  },
  {504,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_PAGE_NOT_WORKING,
   IDS_ERRORPAGES_SUMMARY_GATEWAY_TIMEOUT,
   IDS_ERRORPAGES_DETAILS_GATEWAY_TIMEOUT,
   SUGGEST_RELOAD,
  },
};

const LocalizedErrorMap dns_probe_error_options[] = {
  {error_page::DNS_PROBE_POSSIBLE,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_DNS_PROBE_RUNNING,
   IDS_ERRORPAGES_DETAILS_DNS_PROBE_RUNNING,
   SUGGEST_RELOAD,
  },

  // DNS_PROBE_NOT_RUN is not here; NetErrorHelper will restore the original
  // error, which might be one of several DNS-related errors.

  {error_page::DNS_PROBE_STARTED,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_DNS_PROBE_RUNNING,
   IDS_ERRORPAGES_DETAILS_DNS_PROBE_RUNNING,
   // Include SUGGEST_RELOAD so the More button doesn't jump when we update.
   SUGGEST_RELOAD,
  },

  // DNS_PROBE_FINISHED_UNKNOWN is not here; NetErrorHelper will restore the
  // original error, which might be one of several DNS-related errors.

  {error_page::DNS_PROBE_FINISHED_NO_INTERNET,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_INTERNET_DISCONNECTED,
   IDS_ERRORPAGES_SUMMARY_INTERNET_DISCONNECTED,
   IDS_ERRORPAGES_DETAILS_INTERNET_DISCONNECTED,
   SUGGEST_CHECK_CONNECTION | SUGGEST_FIREWALL_CONFIG,
  },
  {error_page::DNS_PROBE_FINISHED_BAD_CONFIG,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_NAME_NOT_RESOLVED,
   IDS_ERRORPAGES_DETAILS_NAME_NOT_RESOLVED,
   SUGGEST_RELOAD | SUGGEST_DNS_CONFIG | SUGGEST_FIREWALL_CONFIG,
  },
  {error_page::DNS_PROBE_FINISHED_NXDOMAIN,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_NAME_NOT_RESOLVED,
   IDS_ERRORPAGES_DETAILS_NAME_NOT_RESOLVED,
   SUGGEST_RELOAD,
  },
};

const LocalizedErrorMap* FindErrorMapInArray(const LocalizedErrorMap* maps,
                                                   size_t num_maps,
                                                   int error_code) {
  for (size_t i = 0; i < num_maps; ++i) {
    if (maps[i].error_code == error_code)
      return &maps[i];
  }
  return NULL;
}

const LocalizedErrorMap* LookupErrorMap(const std::string& error_domain,
                                        int error_code, bool is_post) {
  if (error_domain == net::kErrorDomain) {
    // Display a different page in the special case of navigating through the
    // history to an uncached page created by a POST.
    if (is_post && error_code == net::ERR_CACHE_MISS)
      return &repost_error;
    return FindErrorMapInArray(net_error_options,
                               arraysize(net_error_options),
                               error_code);
  } else if (error_domain == LocalizedError::kHttpErrorDomain) {
    return FindErrorMapInArray(http_error_options,
                               arraysize(http_error_options),
                               error_code);
  } else if (error_domain == error_page::kDnsProbeErrorDomain) {
    const LocalizedErrorMap* map =
        FindErrorMapInArray(dns_probe_error_options,
                            arraysize(dns_probe_error_options),
                            error_code);
    DCHECK(map);
    return map;
  } else {
    NOTREACHED();
    return NULL;
  }
}

// Returns a dictionary containing the strings for the settings menu under the
// app menu, and the advanced settings button.
base::DictionaryValue* GetStandardMenuItemsText() {
  base::DictionaryValue* standard_menu_items_text = new base::DictionaryValue();
  standard_menu_items_text->SetString("settingsTitle",
      l10n_util::GetStringUTF16(IDS_SETTINGS_TITLE));
  standard_menu_items_text->SetString("advancedTitle",
      l10n_util::GetStringUTF16(IDS_SETTINGS_SHOW_ADVANCED_SETTINGS));
  return standard_menu_items_text;
}

// Gets the icon class for a given |error_domain| and |error_code|.
const char* GetIconClassForError(const std::string& error_domain,
                                 int error_code) {
  if ((error_code == net::ERR_INTERNET_DISCONNECTED &&
       error_domain == net::kErrorDomain) ||
      (error_code == error_page::DNS_PROBE_FINISHED_NO_INTERNET &&
       error_domain == error_page::kDnsProbeErrorDomain))
    return "icon-offline";

  return "icon-generic";
}

// If the first suggestion is for a Google cache copy link. Promote the
// suggestion to a separate set of strings for displaying as a button.
void AddGoogleCachedCopyButton(base::ListValue* suggestions,
                               base::DictionaryValue* error_strings) {
  if (!suggestions->empty()) {
    base::DictionaryValue* suggestion;
    suggestions->GetDictionary(0, &suggestion);
    int type = -1;
    suggestion->GetInteger("type", &type);

    if (type == kGoogleCachedCopySuggestionType) {
      base::string16 cache_url;
      suggestion->GetString("urlCorrection", &cache_url);
      int cache_tracking_id = -1;
      suggestion->GetInteger("trackingId", &cache_tracking_id);
      scoped_ptr<base::DictionaryValue> cache_button(new base::DictionaryValue);
      cache_button->SetString(
            "msg",
            l10n_util::GetStringUTF16(IDS_ERRORPAGES_BUTTON_SHOW_SAVED_COPY));
      cache_button->SetString("cacheUrl", cache_url);
      cache_button->SetInteger("trackingId", cache_tracking_id);
      error_strings->Set("cacheButton", cache_button.release());

      // Remove the item from suggestions dictionary so that it does not get
      // displayed by the template in the details section.
      suggestions->Remove(0, nullptr);
    }
  }
}

}  // namespace

const char LocalizedError::kHttpErrorDomain[] = "http";

void LocalizedError::GetStrings(int error_code,
                                const std::string& error_domain,
                                const GURL& failed_url,
                                bool is_post,
                                bool stale_copy_in_cache,
                                bool can_show_network_diagnostics_dialog,
                                OfflinePageStatus offline_page_status,
                                const std::string& locale,
                                const std::string& accept_languages,
                                scoped_ptr<error_page::ErrorPageParams> params,
                                base::DictionaryValue* error_strings) {
  webui::SetLoadTimeDataDefaults(locale, error_strings);

  // Grab the strings and settings that depend on the error type.  Init
  // options with default values.
  LocalizedErrorMap options = {
    0,
    IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
    IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
    IDS_ERRORPAGES_SUMMARY_NOT_AVAILABLE,
    kErrorPagesNoDetails,
    SUGGEST_NONE,
  };

  const LocalizedErrorMap* error_map = LookupErrorMap(error_domain, error_code,
                                                      is_post);
  if (error_map)
    options = *error_map;

  // If we got "access denied" but the url was a file URL, then we say it was a
  // file instead of just using the "not available" default message. Just adding
  // ERR_ACCESS_DENIED to the map isn't sufficient, since that message may be
  // generated by some OSs when the operation doesn't involve a file URL.
  if (error_domain == net::kErrorDomain &&
      error_code == net::ERR_ACCESS_DENIED &&
      failed_url.scheme() == "file") {
    options.title_resource_id = IDS_ERRORPAGES_TITLE_ACCESS_DENIED;
    options.heading_resource_id = IDS_ERRORPAGES_HEADING_FILE_ACCESS_DENIED;
    options.summary_resource_id = IDS_ERRORPAGES_SUMMARY_FILE_ACCESS_DENIED;
    options.details_resource_id = IDS_ERRORPAGES_DETAILS_FILE_ACCESS_DENIED;
    options.suggestions = SUGGEST_NONE;
  }

  base::string16 failed_url_string(url_formatter::FormatUrl(
      failed_url, accept_languages, url_formatter::kFormatUrlOmitNothing,
      net::UnescapeRule::NORMAL, nullptr, nullptr, nullptr));
  // URLs are always LTR.
  if (base::i18n::IsRTL())
    base::i18n::WrapStringWithLTRFormatting(&failed_url_string);
  error_strings->SetString("title",
      l10n_util::GetStringFUTF16(options.title_resource_id, failed_url_string));
  std::string icon_class = GetIconClassForError(error_domain, error_code);
  error_strings->SetString("iconClass", icon_class);

  base::string16 host_name(url_formatter::IDNToUnicode(failed_url.host(),
                                                       accept_languages));

  base::DictionaryValue* heading = new base::DictionaryValue;
  heading->SetString("msg",
                     l10n_util::GetStringUTF16(options.heading_resource_id));
  heading->SetString("hostName", host_name);
  error_strings->Set("heading", heading);

  base::DictionaryValue* summary = new base::DictionaryValue;

  // Set summary message under the heading.
  summary->SetString("msg",
      l10n_util::GetStringUTF16(options.summary_resource_id));
  // Add a DNS definition string.
  summary->SetString("dnsDefinition",
      l10n_util::GetStringUTF16(IDS_ERRORPAGES_SUMMARY_DNS_DEFINITION));

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // Check if easter egg should be disabled.
  if (command_line->HasSwitch(
          error_page::switches::kDisableDinosaurEasterEgg)) {
    // The presence of this string disables the easter egg. Acts as a flag.
    error_strings->SetString("disabledEasterEgg",
        l10n_util::GetStringUTF16(IDS_ERRORPAGE_FUN_DISABLED));
  }

  summary->SetString("failedUrl", failed_url_string);
  summary->SetString("hostName", host_name);

  error_strings->SetString(
      "details", l10n_util::GetStringUTF16(IDS_ERRORPAGE_NET_BUTTON_DETAILS));
  error_strings->SetString(
      "hideDetails", l10n_util::GetStringUTF16(
          IDS_ERRORPAGE_NET_BUTTON_HIDE_DETAILS));
  error_strings->Set("summary", summary);

  error_strings->SetString(
      "errorDetails",
      options.details_resource_id != kErrorPagesNoDetails
          ? l10n_util::GetStringUTF16(options.details_resource_id)
          : base::string16());

  base::string16 error_string;
  if (error_domain == net::kErrorDomain) {
    // Non-internationalized error string, for debugging Chrome itself.
    error_string = base::ASCIIToUTF16(net::ErrorToShortString(error_code));
  } else if (error_domain == error_page::kDnsProbeErrorDomain) {
    std::string ascii_error_string =
        error_page::DnsProbeStatusToString(error_code);
    error_string = base::ASCIIToUTF16(ascii_error_string);
  } else {
    DCHECK_EQ(LocalizedError::kHttpErrorDomain, error_domain);
    error_string = base::IntToString16(error_code);
  }
  error_strings->SetString("errorCode", error_string);

  // Platform specific information for diagnosing network issues on OSX and
  // Windows.
#if (defined(OS_MACOSX) && !defined(OS_IOS)) || defined(OS_WIN)
  if (error_domain == net::kErrorDomain &&
      error_code == net::ERR_INTERNET_DISCONNECTED) {
    int platform_string_id =
        IDS_ERRORPAGES_SUMMARY_INTERNET_DISCONNECTED_PLATFORM;
#if defined(OS_WIN)
    // Different versions of Windows have different instructions.
    base::win::Version windows_version = base::win::GetVersion();
    if (windows_version < base::win::VERSION_VISTA) {
      // XP, XP64, and Server 2003.
      platform_string_id =
          IDS_ERRORPAGES_SUMMARY_INTERNET_DISCONNECTED_PLATFORM_XP;
    } else if (windows_version == base::win::VERSION_VISTA) {
      // Vista
      platform_string_id =
          IDS_ERRORPAGES_SUMMARY_INTERNET_DISCONNECTED_PLATFORM_VISTA;
    }
#endif  // defined(OS_WIN)
    // Platform dependent portion of the summary section.
    summary->SetString("msg",
        l10n_util::GetStringFUTF16(
            IDS_ERRORPAGES_SUMMARY_INTERNET_DISCONNECTED_INSTRUCTIONS_TEMPLATE,
            l10n_util::GetStringUTF16(platform_string_id)));
  }
#endif  // (defined(OS_MACOSX) && !defined(OS_IOS)) || defined(OS_WIN)

  // If no parameters were provided, use the defaults.
  if (!params) {
    params.reset(new error_page::ErrorPageParams());
    params->suggest_reload = !!(options.suggestions & SUGGEST_RELOAD);
  }

  base::ListValue* suggestions = NULL;
  bool use_default_suggestions = true;
  if (!params->override_suggestions) {
    suggestions = new base::ListValue();
  } else {
    suggestions = params->override_suggestions.release();
    use_default_suggestions = false;
    AddGoogleCachedCopyButton(suggestions, error_strings);
  }
  error_strings->Set("suggestions", suggestions);

  if (params->search_url.is_valid()) {
    error_strings->SetString("searchHeader",
        l10n_util::GetStringUTF16(IDS_ERRORPAGES_SUGGESTION_GOOGLE_SEARCH));
    error_strings->SetString("searchUrl", params->search_url.spec());
    error_strings->SetString("searchTerms", params->search_terms);
    error_strings->SetInteger("searchTrackingId", params->search_tracking_id);
  }

  // Add the reload suggestion, if needed.
  if (params->suggest_reload) {
    if (!is_post) {
      base::DictionaryValue* reload_button = new base::DictionaryValue;
      reload_button->SetString(
          "msg", l10n_util::GetStringUTF16(IDS_ERRORPAGES_BUTTON_RELOAD));
      reload_button->SetString("reloadUrl", failed_url.spec());
      error_strings->Set("reloadButton", reload_button);
      reload_button->SetInteger("reloadTrackingId", params->reload_tracking_id);
    } else {
      // If the page was created by a post, it can't be reloaded in the same
      // way, so just add a suggestion instead.
      // TODO(mmenke):  Make the reload button bring up the repost confirmation
      //                dialog for pages resulting from posts.
      base::DictionaryValue* suggest_reload_repost = new base::DictionaryValue;
      suggest_reload_repost->SetString("header",
          l10n_util::GetStringUTF16(
              IDS_ERRORPAGES_SUGGESTION_RELOAD_REPOST_HEADER));
      suggest_reload_repost->SetString("body",
          l10n_util::GetStringUTF16(
              IDS_ERRORPAGES_SUGGESTION_RELOAD_REPOST_BODY));
      // Add at the front, so it appears before other suggestions, in the case
      // suggestions are being overridden by |params|.
      suggestions->Insert(0, suggest_reload_repost);
    }
  }

  // If not using the default suggestions, nothing else to do.
  if (!use_default_suggestions)
    return;

  const std::string& show_saved_copy_value =
      command_line->GetSwitchValueASCII(error_page::switches::kShowSavedCopy);
  bool show_saved_copy_primary =
      (show_saved_copy_value ==
       error_page::switches::kEnableShowSavedCopyPrimary);
  bool show_saved_copy_secondary =
      (show_saved_copy_value ==
       error_page::switches::kEnableShowSavedCopySecondary);
  bool show_saved_copy_visible =
      (stale_copy_in_cache && !is_post &&
      (show_saved_copy_primary || show_saved_copy_secondary));

  if (show_saved_copy_visible) {
    base::DictionaryValue* show_saved_copy_button = new base::DictionaryValue;
    show_saved_copy_button->SetString(
        "msg", l10n_util::GetStringUTF16(
            IDS_ERRORPAGES_BUTTON_SHOW_SAVED_COPY));
    show_saved_copy_button->SetString(
        "title",
        l10n_util::GetStringUTF16(IDS_ERRORPAGES_BUTTON_SHOW_SAVED_COPY_HELP));
    if (show_saved_copy_primary)
      show_saved_copy_button->SetString("primary", "true");
    error_strings->Set("showSavedCopyButton", show_saved_copy_button);
  }

#if defined(OS_ANDROID)
  // Offline button will not be provided when we want to show something in the
  // cache.
  if (!show_saved_copy_visible) {
    if (offline_page_status == OfflinePageStatus::HAS_OFFLINE_PAGE) {
      base::DictionaryValue* show_offline_copy_button =
          new base::DictionaryValue;
      base::string16 button_text =
          l10n_util::GetStringUTF16(IDS_ERRORPAGES_BUTTON_SHOW_OFFLINE_COPY);
      show_offline_copy_button->SetString("msg", button_text);
      error_strings->Set("showOfflineCopyButton", show_offline_copy_button);
    } else if (offline_page_status ==
               OfflinePageStatus::HAS_OTHER_OFFLINE_PAGES) {
      base::DictionaryValue* show_offline_pages_button =
          new base::DictionaryValue;
      base::string16 button_text = l10n_util::GetStringUTF16(
          offline_pages::GetOfflinePageFeatureMode() ==
              offline_pages::FeatureMode::ENABLED_AS_BOOKMARKS
                  ? IDS_ERRORPAGES_BUTTON_SHOW_OFFLINE_PAGES_AS_BOOKMARKS
                  : IDS_ERRORPAGES_BUTTON_SHOW_OFFLINE_PAGES);
      show_offline_pages_button->SetString("msg", button_text);
      error_strings->Set("showOfflinePagesButton", show_offline_pages_button);
    }
  }
#endif  // defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
  // ChromeOS has its own diagnostics extension, which doesn't rely on a
  // browser-initiated dialog.
  can_show_network_diagnostics_dialog = true;
#endif
  if (can_show_network_diagnostics_dialog && failed_url.is_valid() &&
      failed_url.SchemeIsHTTPOrHTTPS()) {
    error_strings->SetString(
        "diagnose", l10n_util::GetStringUTF16(IDS_ERRORPAGES_BUTTON_DIAGNOSE));
  }

  if (options.suggestions & SUGGEST_CHECK_CONNECTION) {
    base::DictionaryValue* suggest_check_connection = new base::DictionaryValue;
    suggest_check_connection->SetString("header",
        l10n_util::GetStringUTF16(
            IDS_ERRORPAGES_SUGGESTION_CHECK_CONNECTION_HEADER));
    suggest_check_connection->SetString("body",
        l10n_util::GetStringUTF16(
            IDS_ERRORPAGES_SUGGESTION_CHECK_CONNECTION_BODY));
    suggestions->Append(suggest_check_connection);
  }

  if (options.suggestions & SUGGEST_DNS_CONFIG) {
    base::DictionaryValue* suggest_dns_config = new base::DictionaryValue;
    suggest_dns_config->SetString("header",
        l10n_util::GetStringUTF16(
            IDS_ERRORPAGES_SUGGESTION_DNS_CONFIG_HEADER));
    suggest_dns_config->SetString("body",
        l10n_util::GetStringUTF16(
            IDS_ERRORPAGES_SUGGESTION_DNS_CONFIG_BODY));
    suggestions->Append(suggest_dns_config);

    base::DictionaryValue* suggest_network_prediction =
        GetStandardMenuItemsText();
    suggest_network_prediction->SetString("header",
        l10n_util::GetStringUTF16(
            IDS_ERRORPAGES_SUGGESTION_NETWORK_PREDICTION_HEADER));
    suggest_network_prediction->SetString("body",
        l10n_util::GetStringUTF16(
            IDS_ERRORPAGES_SUGGESTION_NETWORK_PREDICTION_BODY));
    suggest_network_prediction->SetString(
        "noNetworkPredictionTitle",
        l10n_util::GetStringUTF16(
            IDS_NETWORK_PREDICTION_ENABLED_DESCRIPTION));
    suggestions->Append(suggest_network_prediction);
  }

  // TODO(crbug.com/584615): Does it make sense to show all of these
  // suggestions on mobile? Several of them seem irrelevant in the mobile
  // context.
  if (options.suggestions & SUGGEST_FIREWALL_CONFIG) {
    base::DictionaryValue* suggest_firewall_config = new base::DictionaryValue;
    suggest_firewall_config->SetString("header",
        l10n_util::GetStringUTF16(
            IDS_ERRORPAGES_SUGGESTION_FIREWALL_CONFIG_HEADER));
    suggest_firewall_config->SetString("body",
        l10n_util::GetStringUTF16(
            IDS_ERRORPAGES_SUGGESTION_FIREWALL_CONFIG_BODY));
    suggestions->Append(suggest_firewall_config);
  }

  if (options.suggestions & SUGGEST_PROXY_CONFIG) {
    base::DictionaryValue* suggest_proxy_config = GetStandardMenuItemsText();
    suggest_proxy_config->SetString("header",
        l10n_util::GetStringUTF16(
            IDS_ERRORPAGES_SUGGESTION_PROXY_CONFIG_HEADER));
    suggest_proxy_config->SetString("body",
        l10n_util::GetStringFUTF16(IDS_ERRORPAGES_SUGGESTION_PROXY_CONFIG_BODY,
            l10n_util::GetStringUTF16(
                IDS_ERRORPAGES_SUGGESTION_PROXY_DISABLE_PLATFORM)));
    suggest_proxy_config->SetString("proxyTitle",
        l10n_util::GetStringUTF16(IDS_OPTIONS_PROXIES_CONFIGURE_BUTTON));

    suggestions->Append(suggest_proxy_config);
  }

  if (options.suggestions & SUGGEST_DISABLE_EXTENSION) {
    base::DictionaryValue* suggest_disable_extension =
        new base::DictionaryValue;
    // There's only a header for this suggestion.
    suggest_disable_extension->SetString("header",
        l10n_util::GetStringUTF16(
            IDS_ERRORPAGES_SUGGESTION_DISABLE_EXTENSION_HEADER));
    suggestions->Append(suggest_disable_extension);
  }

  if (options.suggestions & SUGGEST_VIEW_POLICIES) {
    base::DictionaryValue* suggest_view_policies = new base::DictionaryValue;
    suggest_view_policies->SetString(
        "header",
        l10n_util::GetStringUTF16(
            IDS_ERRORPAGES_SUGGESTION_VIEW_POLICIES_HEADER));
    suggest_view_policies->SetString(
        "body",
        l10n_util::GetStringUTF16(
            IDS_ERRORPAGES_SUGGESTION_VIEW_POLICIES_BODY));
    suggestions->Append(suggest_view_policies);
  }

  if (options.suggestions & SUGGEST_CONTACT_ADMINISTRATOR) {
    base::DictionaryValue* suggest_contant_administrator =
        new base::DictionaryValue;
    suggest_contant_administrator->SetString(
        "body",
        l10n_util::GetStringUTF16(
            IDS_ERRORPAGES_SUGGESTION_CONTACT_ADMINISTRATOR_BODY));
    suggestions->Append(suggest_contant_administrator);
  }

  if (options.suggestions & SUGGEST_LEARNMORE) {
    GURL learn_more_url;
    switch (options.error_code) {
      case net::ERR_TOO_MANY_REDIRECTS:
        learn_more_url = GURL(kRedirectLoopLearnMoreUrl);
        break;
      case net::ERR_SSL_WEAK_SERVER_EPHEMERAL_DH_KEY:
        learn_more_url = GURL(kWeakDHKeyLearnMoreUrl);
        break;
      default:
        break;
    }

    if (learn_more_url.is_valid()) {
      // Add the language parameter to the URL.
      std::string query = learn_more_url.query() + "&hl=" + locale;
      GURL::Replacements repl;
      repl.SetQueryStr(query);
      learn_more_url = learn_more_url.ReplaceComponents(repl);

      base::DictionaryValue* suggest_learn_more = new base::DictionaryValue;
      // There's only a body for this suggestion.
      suggest_learn_more->SetString("body",
          l10n_util::GetStringUTF16(IDS_ERRORPAGES_SUGGESTION_LEARNMORE_BODY));
      suggest_learn_more->SetString("learnMoreUrl", learn_more_url.spec());
      suggestions->Append(suggest_learn_more);
    }
  }

  if (options.suggestions & SUGGEST_UNSUPPORTED_CIPHER) {
    base::DictionaryValue* suggest_unsupported_cipher =
        new base::DictionaryValue;
    suggest_unsupported_cipher->SetString(
        "body",
        l10n_util::GetStringUTF16(
            IDS_ERRORPAGES_SUGGESTION_UNSUPPORTED_CIPHER));
    suggestions->Append(suggest_unsupported_cipher);
  }
}

base::string16 LocalizedError::GetErrorDetails(const std::string& error_domain,
                                               int error_code,
                                               bool is_post) {
  const LocalizedErrorMap* error_map =
      LookupErrorMap(error_domain, error_code, is_post);
  if (error_map)
    return l10n_util::GetStringUTF16(error_map->details_resource_id);
  else
    return l10n_util::GetStringUTF16(IDS_ERRORPAGES_DETAILS_UNKNOWN);
}

bool LocalizedError::HasStrings(const std::string& error_domain,
                                int error_code) {
  // Whether or not the there are strings for an error does not depend on
  // whether or not the page was be generated by a POST, so just claim it was
  // not.
  return LookupErrorMap(error_domain, error_code, /*is_post=*/false) != NULL;
}

}  // namespace error_page
