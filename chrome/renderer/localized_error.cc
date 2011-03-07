// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/localized_error.h"

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/sys_info.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_set.h"
#include "googleurl/src/gurl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLError.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebURLError;

namespace {

static const char kRedirectLoopLearnMoreUrl[] =
    "http://www.google.com/support/chrome/bin/answer.py?answer=95626";
static const char kWeakDHKeyLearnMoreUrl[] =
    "http://sites.google.com/a/chromium.org/dev/err_ssl_weak_server_ephemeral_dh_key";
static const char kESETLearnMoreUrl[] =
    "http://kb.eset.com/esetkb/index?page=content&id=SOLN2588";

enum NAV_SUGGESTIONS {
  SUGGEST_NONE     = 0,
  SUGGEST_RELOAD   = 1 << 0,
  SUGGEST_HOSTNAME = 1 << 1,
  SUGGEST_CHECK_CONNECTION = 1 << 2,
  SUGGEST_DNS_CONFIG = 1 << 3,
  SUGGEST_FIREWALL_CONFIG = 1 << 4,
  SUGGEST_PROXY_CONFIG = 1 << 5,
  SUGGEST_LEARNMORE = 1 << 6,
};

struct LocalizedErrorMap {
  int error_code;
  unsigned int title_resource_id;
  unsigned int heading_resource_id;
  unsigned int summary_resource_id;
  unsigned int details_resource_id;
  int suggestions;  // Bitmap of SUGGEST_* values.
};

const LocalizedErrorMap net_error_options[] = {
  {net::ERR_TIMED_OUT,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_NOT_AVAILABLE,
   IDS_ERRORPAGES_DETAILS_TIMED_OUT,
   SUGGEST_RELOAD,
  },
  {net::ERR_CONNECTION_TIMED_OUT,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_NOT_AVAILABLE,
   IDS_ERRORPAGES_DETAILS_TIMED_OUT,
   SUGGEST_RELOAD,
  },
  {net::ERR_CONNECTION_CLOSED,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_NOT_AVAILABLE,
   IDS_ERRORPAGES_DETAILS_CONNECTION_CLOSED,
   SUGGEST_RELOAD,
  },
  {net::ERR_CONNECTION_RESET,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_NOT_AVAILABLE,
   IDS_ERRORPAGES_DETAILS_CONNECTION_RESET,
   SUGGEST_RELOAD,
  },
  {net::ERR_CONNECTION_REFUSED,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_NOT_AVAILABLE,
   IDS_ERRORPAGES_DETAILS_CONNECTION_REFUSED,
   SUGGEST_RELOAD,
  },
  {net::ERR_CONNECTION_FAILED,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
   IDS_ERRORPAGES_SUMMARY_NOT_AVAILABLE,
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
   IDS_ERRORPAGES_HEADING_PROXY_CONNECTION_FAILED,
   IDS_ERRORPAGES_SUMMARY_PROXY_CONNECTION_FAILED,
   IDS_ERRORPAGES_DETAILS_PROXY_CONNECTION_FAILED,
   SUGGEST_PROXY_CONFIG,
  },
  {net::ERR_INTERNET_DISCONNECTED,
   IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
   IDS_ERRORPAGES_HEADING_INTERNET_DISCONNECTED,
   IDS_ERRORPAGES_SUMMARY_INTERNET_DISCONNECTED,
   IDS_ERRORPAGES_DETAILS_INTERNET_DISCONNECTED,
   SUGGEST_NONE,
  },
  {net::ERR_FILE_NOT_FOUND,
   IDS_ERRORPAGES_TITLE_NOT_FOUND,
   IDS_ERRORPAGES_HEADING_NOT_FOUND,
   IDS_ERRORPAGES_SUMMARY_NOT_FOUND,
   IDS_ERRORPAGES_DETAILS_FILE_NOT_FOUND,
   SUGGEST_NONE,
  },
  {net::ERR_CACHE_MISS,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_CACHE_MISS,
   IDS_ERRORPAGES_SUMMARY_CACHE_MISS,
   IDS_ERRORPAGES_DETAILS_CACHE_MISS,
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
   IDS_ERRORPAGES_HEADING_NETWORK_IO_SUSPENDED,
   IDS_ERRORPAGES_SUMMARY_NETWORK_IO_SUSPENDED,
   IDS_ERRORPAGES_DETAILS_NETWORK_IO_SUSPENDED,
   SUGGEST_RELOAD,
  },
  {net::ERR_TOO_MANY_REDIRECTS,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_TOO_MANY_REDIRECTS,
   IDS_ERRORPAGES_SUMMARY_TOO_MANY_REDIRECTS,
   IDS_ERRORPAGES_DETAILS_TOO_MANY_REDIRECTS,
   SUGGEST_RELOAD | SUGGEST_LEARNMORE,
  },
  {net::ERR_EMPTY_RESPONSE,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_EMPTY_RESPONSE,
   IDS_ERRORPAGES_SUMMARY_EMPTY_RESPONSE,
   IDS_ERRORPAGES_DETAILS_EMPTY_RESPONSE,
   SUGGEST_RELOAD,
  },
  {net::ERR_SSL_PROTOCOL_ERROR,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_SSL_PROTOCOL_ERROR,
   IDS_ERRORPAGES_SUMMARY_SSL_PROTOCOL_ERROR,
   IDS_ERRORPAGES_DETAILS_SSL_PROTOCOL_ERROR,
   SUGGEST_NONE,
  },
  {net::ERR_SSL_UNSAFE_NEGOTIATION,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_SSL_PROTOCOL_ERROR,
   IDS_ERRORPAGES_SUMMARY_SSL_PROTOCOL_ERROR,
   IDS_ERRORPAGES_DETAILS_SSL_UNSAFE_NEGOTIATION,
   SUGGEST_NONE,
  },
  {net::ERR_BAD_SSL_CLIENT_AUTH_CERT,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_BAD_SSL_CLIENT_AUTH_CERT,
   IDS_ERRORPAGES_SUMMARY_BAD_SSL_CLIENT_AUTH_CERT,
   IDS_ERRORPAGES_DETAILS_BAD_SSL_CLIENT_AUTH_CERT,
   SUGGEST_NONE,
  },
  {net::ERR_SSL_WEAK_SERVER_EPHEMERAL_DH_KEY,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_WEAK_SERVER_EPHEMERAL_DH_KEY,
   IDS_ERRORPAGES_SUMMARY_WEAK_SERVER_EPHEMERAL_DH_KEY,
   IDS_ERRORPAGES_DETAILS_SSL_PROTOCOL_ERROR,
   SUGGEST_LEARNMORE,
  },
  {net::ERR_ESET_ANTI_VIRUS_SSL_INTERCEPTION,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_ESET_ANTI_VIRUS_SSL_INTERCEPTION,
   IDS_ERRORPAGES_SUMMARY_ESET_ANTI_VIRUS_SSL_INTERCEPTION,
   IDS_ERRORPAGES_DETAILS_SSL_PROTOCOL_ERROR,
   SUGGEST_LEARNMORE,
  },
  {net::ERR_TEMPORARILY_THROTTLED,
   IDS_ERRORPAGES_TITLE_ACCESS_DENIED,
   IDS_ERRORPAGES_HEADING_ACCESS_DENIED,
   IDS_ERRORPAGES_SUMMARY_TEMPORARILY_THROTTLED,
   IDS_ERRORPAGES_DETAILS_TEMPORARILY_THROTTLED,
   SUGGEST_NONE,
  },
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
   IDS_ERRORPAGES_HEADING_HTTP_SERVER_ERROR,
   IDS_ERRORPAGES_SUMMARY_INTERNAL_SERVER_ERROR,
   IDS_ERRORPAGES_DETAILS_INTERNAL_SERVER_ERROR,
   SUGGEST_RELOAD,
  },
  {501,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_HTTP_SERVER_ERROR,
   IDS_ERRORPAGES_SUMMARY_WEBSITE_CANNOT_HANDLE,
   IDS_ERRORPAGES_DETAILS_NOT_IMPLEMENTED,
   SUGGEST_NONE,
  },
  {502,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_HTTP_SERVER_ERROR,
   IDS_ERRORPAGES_SUMMARY_BAD_GATEWAY,
   IDS_ERRORPAGES_DETAILS_BAD_GATEWAY,
   SUGGEST_RELOAD,
  },
  {503,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_HTTP_SERVER_ERROR,
   IDS_ERRORPAGES_SUMMARY_SERVICE_UNAVAILABLE,
   IDS_ERRORPAGES_DETAILS_SERVICE_UNAVAILABLE,
   SUGGEST_RELOAD,
  },
  {504,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_HTTP_SERVER_ERROR,
   IDS_ERRORPAGES_SUMMARY_GATEWAY_TIMEOUT,
   IDS_ERRORPAGES_DETAILS_GATEWAY_TIMEOUT,
   SUGGEST_RELOAD,
  },
  {505,
   IDS_ERRORPAGES_TITLE_LOAD_FAILED,
   IDS_ERRORPAGES_HEADING_HTTP_SERVER_ERROR,
   IDS_ERRORPAGES_SUMMARY_WEBSITE_CANNOT_HANDLE,
   IDS_ERRORPAGES_DETAILS_HTTP_VERSION_NOT_SUPPORTED,
   SUGGEST_NONE,
  },
};

const char* HttpErrorToString(int status_code) {
  switch (status_code) {
  case 403:
    return "Forbidden";
  case 410:
    return "Gone";
  case 500:
    return "Internal Server Error";
  case 501:
    return "Not Implemented";
  case 502:
    return "Bad Gateway";
  case 503:
    return "Service Unavailable";
  case 504:
    return "Gateway Timeout";
  case 505:
    return "HTTP Version Not Supported";
  default:
    return "";
  }
}

string16 GetErrorDetailsString(const std::string& error_domain,
                               int error_code,
                               const string16& details) {
  int error_page_template;
  const char* error_string;
  if (error_domain == net::kErrorDomain) {
    error_page_template = IDS_ERRORPAGES_DETAILS_TEMPLATE;
    error_string = net::ErrorToString(error_code);
    DCHECK(error_code < 0);  // Net error codes are negative.
    error_code = -error_code;
  } else if (error_domain == LocalizedError::kHttpErrorDomain) {
    error_page_template = IDS_ERRORPAGES_HTTP_DETAILS_TEMPLATE;
    error_string = HttpErrorToString(error_code);
  } else {
    NOTREACHED();
    return string16();
  }
  return l10n_util::GetStringFUTF16(
      error_page_template,
      base::IntToString16(error_code),
      ASCIIToUTF16(error_string),
      details);
}

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
                                        int error_code) {
  if (error_domain == net::kErrorDomain) {
    return FindErrorMapInArray(net_error_options,
                               arraysize(net_error_options),
                               error_code);
  } else if (error_domain == LocalizedError::kHttpErrorDomain) {
    return FindErrorMapInArray(http_error_options,
                               arraysize(http_error_options),
                               error_code);
  } else {
    NOTREACHED();
    return NULL;
  }
}

bool LocaleIsRTL() {
#if defined(TOOLKIT_GTK)
  // base::i18n::IsRTL() uses the GTK text direction, which doesn't work within
  // the renderer sandbox.
  return base::i18n::ICUIsRTL();
#else
  return base::i18n::IsRTL();
#endif
}

}  // namespace

const char LocalizedError::kHttpErrorDomain[] = "http";

void LocalizedError::GetStrings(const WebKit::WebURLError& error,
                                DictionaryValue* error_strings) {
  bool rtl = LocaleIsRTL();
  error_strings->SetString("textdirection", rtl ? "rtl" : "ltr");

  // Grab the strings and settings that depend on the error type.  Init
  // options with default values.
  LocalizedErrorMap options = {
    0,
    IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
    IDS_ERRORPAGES_HEADING_NOT_AVAILABLE,
    IDS_ERRORPAGES_SUMMARY_NOT_AVAILABLE,
    IDS_ERRORPAGES_DETAILS_UNKNOWN,
    SUGGEST_NONE,
  };

  const std::string error_domain = error.domain.utf8();
  int error_code = error.reason;
  const LocalizedErrorMap* error_map =
      LookupErrorMap(error_domain, error_code);
  if (error_map)
    options = *error_map;

  if (options.suggestions != SUGGEST_NONE) {
    error_strings->SetString(
        "suggestionsHeading",
        l10n_util::GetStringUTF16(IDS_ERRORPAGES_SUGGESTION_HEADING));
  }

  const GURL failed_url = error.unreachableURL;
  string16 failed_url_string(ASCIIToUTF16(failed_url.spec()));
  // URLs are always LTR.
  if (rtl)
    base::i18n::WrapStringWithLTRFormatting(&failed_url_string);
  error_strings->SetString("title",
      l10n_util::GetStringFUTF16(options.title_resource_id, failed_url_string));
  error_strings->SetString("heading",
      l10n_util::GetStringUTF16(options.heading_resource_id));

  DictionaryValue* summary = new DictionaryValue;
  summary->SetString("msg",
      l10n_util::GetStringUTF16(options.summary_resource_id));
  // TODO(tc): We want the unicode url and host here since they're being
  //           displayed.
  summary->SetString("failedUrl", failed_url_string);
  summary->SetString("hostName", failed_url.host());
  summary->SetString("productName",
                     l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  error_strings->Set("summary", summary);

  string16 details = l10n_util::GetStringUTF16(options.details_resource_id);
  error_strings->SetString("details",
      GetErrorDetailsString(error_domain, error_code, details));

  // Platform specific instructions for diagnosing network issues on OSX and
  // Windows.
#if defined(OS_MACOSX) || defined(OS_WIN)
  if (error_domain == net::kErrorDomain &&
      error_code == net::ERR_INTERNET_DISCONNECTED) {
    int platform_string_id =
        IDS_ERRORPAGES_SUMMARY_INTERNET_DISCONNECTED_PLATFORM;
#if defined(OS_WIN)
    // Different versions of Windows have different instructions.
    int32 major_version, minor_version, bugfix_version;
    base::SysInfo::OperatingSystemVersionNumbers(
        &major_version, &minor_version, &bugfix_version);
    if (major_version < 6) {
      // XP, XP64, and Server 2003.
      platform_string_id =
          IDS_ERRORPAGES_SUMMARY_INTERNET_DISCONNECTED_PLATFORM_XP;
    } else if (major_version == 6 && minor_version == 0) {
      // Vista
      platform_string_id =
          IDS_ERRORPAGES_SUMMARY_INTERNET_DISCONNECTED_PLATFORM_VISTA;
    }
#endif  // defined(OS_WIN)
    // Lead with the general error description, and suffix with the platform
    // dependent portion of the summary section.
    summary->SetString("msg",
        l10n_util::GetStringFUTF16(
            IDS_ERRORPAGES_SUMMARY_INTERNET_DISCONNECTED_INSTRUCTIONS_TEMPLATE,
            l10n_util::GetStringUTF16(options.summary_resource_id),
            l10n_util::GetStringUTF16(platform_string_id)));
  }
#endif  // defined(OS_MACOSX) || defined(OS_WIN)

  if (options.suggestions & SUGGEST_RELOAD) {
    DictionaryValue* suggest_reload = new DictionaryValue;
    suggest_reload->SetString("msg",
        l10n_util::GetStringUTF16(IDS_ERRORPAGES_SUGGESTION_RELOAD));
    suggest_reload->SetString("reloadUrl", failed_url_string);
    error_strings->Set("suggestionsReload", suggest_reload);
  }

  if (options.suggestions & SUGGEST_HOSTNAME) {
    // Only show the "Go to hostname" suggestion if the failed_url has a path.
    if (std::string() == failed_url.path()) {
      DictionaryValue* suggest_home_page = new DictionaryValue;
      suggest_home_page->SetString("suggestionsHomepageMsg",
          l10n_util::GetStringUTF16(IDS_ERRORPAGES_SUGGESTION_HOMEPAGE));
      string16 homepage(ASCIIToUTF16(failed_url.GetWithEmptyPath().spec()));
      // URLs are always LTR.
      if (rtl)
        base::i18n::WrapStringWithLTRFormatting(&homepage);
      suggest_home_page->SetString("homePage", homepage);
      // TODO(tc): we actually want the unicode hostname
      suggest_home_page->SetString("hostName", failed_url.host());
      error_strings->Set("suggestionsHomepage", suggest_home_page);
    }
  }

  if (options.suggestions & SUGGEST_CHECK_CONNECTION) {
    DictionaryValue* suggest_check_connection = new DictionaryValue;
    suggest_check_connection->SetString("msg",
        l10n_util::GetStringUTF16(IDS_ERRORPAGES_SUGGESTION_CHECK_CONNECTION));
    error_strings->Set("suggestionsCheckConnection", suggest_check_connection);
  }

  if (options.suggestions & SUGGEST_DNS_CONFIG) {
    DictionaryValue* suggest_dns_config = new DictionaryValue;
    suggest_dns_config->SetString("msg",
        l10n_util::GetStringUTF16(IDS_ERRORPAGES_SUGGESTION_DNS_CONFIG));
    error_strings->Set("suggestionsDNSConfig", suggest_dns_config);

    DictionaryValue* suggest_dns_prefetch = new DictionaryValue;
    suggest_dns_prefetch->SetString("msg",
        l10n_util::GetStringUTF16(IDS_ERRORPAGES_SUGGESTION_DNS_PREFETCH));
    suggest_dns_prefetch->SetString("settingsTitle",
        l10n_util::GetStringUTF16(IDS_SETTINGS_TITLE));
    suggest_dns_prefetch->SetString("advancedTitle",
        l10n_util::GetStringUTF16(IDS_OPTIONS_ADVANCED_TAB_LABEL));
    suggest_dns_prefetch->SetString(
        "noPrefetchTitle",
        l10n_util::GetStringUTF16(
            IDS_NETWORK_DNS_PREFETCH_ENABLED_DESCRIPTION));
    error_strings->Set("suggestionsDisableDNSPrefetch", suggest_dns_prefetch);
  }

  if (options.suggestions & SUGGEST_FIREWALL_CONFIG) {
    DictionaryValue* suggest_firewall_config = new DictionaryValue;
    suggest_firewall_config->SetString("msg",
        l10n_util::GetStringUTF16(IDS_ERRORPAGES_SUGGESTION_FIREWALL_CONFIG));
    suggest_firewall_config->SetString("productName",
        l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
    error_strings->Set("suggestionsFirewallConfig", suggest_firewall_config);
  }

  if (options.suggestions & SUGGEST_PROXY_CONFIG) {
    DictionaryValue* suggest_proxy_config = new DictionaryValue;
    suggest_proxy_config->SetString("msg",
        l10n_util::GetStringUTF16(IDS_ERRORPAGES_SUGGESTION_PROXY_CONFIG));
    error_strings->Set("suggestionsProxyConfig", suggest_proxy_config);

    DictionaryValue* suggest_proxy_disable = new DictionaryValue;
    suggest_proxy_disable->SetString("msg",
        l10n_util::GetStringFUTF16(IDS_ERRORPAGES_SUGGESTION_PROXY_DISABLE,
            l10n_util::GetStringUTF16(
                IDS_ERRORPAGES_SUGGESTION_PROXY_DISABLE_PLATFORM)));
    error_strings->Set("suggestionsProxyDisable", suggest_proxy_disable);
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
      case net::ERR_ESET_ANTI_VIRUS_SSL_INTERCEPTION:
        learn_more_url = GURL(kESETLearnMoreUrl);
        break;
      default:
        break;
    }

    if (learn_more_url.is_valid()) {
      // Add the language parameter to the URL.
      std::string query = learn_more_url.query() + "&hl=" +
          webkit_glue::GetWebKitLocale();
      GURL::Replacements repl;
      repl.SetQueryStr(query);
      learn_more_url = learn_more_url.ReplaceComponents(repl);

      DictionaryValue* suggest_learn_more = new DictionaryValue;
      suggest_learn_more->SetString("msg",
          l10n_util::GetStringUTF16(IDS_ERRORPAGES_SUGGESTION_LEARNMORE));
      suggest_learn_more->SetString("learnMoreUrl", learn_more_url.spec());
      error_strings->Set("suggestionsLearnMore", suggest_learn_more);
    }
  }
}

bool LocalizedError::HasStrings(const std::string& error_domain,
                                int error_code) {
  return LookupErrorMap(error_domain, error_code) != NULL;
}

void LocalizedError::GetFormRepostStrings(const GURL& display_url,
                                          DictionaryValue* error_strings) {
  bool rtl = LocaleIsRTL();
  error_strings->SetString("textdirection", rtl ? "rtl" : "ltr");

  string16 failed_url(ASCIIToUTF16(display_url.spec()));
  // URLs are always LTR.
  if (rtl)
    base::i18n::WrapStringWithLTRFormatting(&failed_url);
  error_strings->SetString(
      "title", l10n_util::GetStringFUTF16(IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
                                          failed_url));
  error_strings->SetString(
      "heading", l10n_util::GetStringUTF16(IDS_HTTP_POST_WARNING_TITLE));
  DictionaryValue* summary = new DictionaryValue;
  summary->SetString(
      "msg", l10n_util::GetStringUTF16(IDS_ERRORPAGES_HTTP_POST_WARNING));
  error_strings->Set("summary", summary);
}

void LocalizedError::GetAppErrorStrings(
    const WebURLError& error,
    const GURL& display_url,
    const Extension* app,
    DictionaryValue* error_strings) {
  DCHECK(app);

  bool rtl = LocaleIsRTL();
  error_strings->SetString("textdirection", rtl ? "rtl" : "ltr");

  string16 failed_url(ASCIIToUTF16(display_url.spec()));
  // URLs are always LTR.
  if (rtl)
    base::i18n::WrapStringWithLTRFormatting(&failed_url);
  error_strings->SetString(
     "url", l10n_util::GetStringFUTF16(IDS_ERRORPAGES_TITLE_NOT_AVAILABLE,
                                       failed_url.c_str()));

  error_strings->SetString("title", app->name());
  error_strings->SetString("icon",
      app->GetIconURL(Extension::EXTENSION_ICON_LARGE,
                      ExtensionIconSet::MATCH_SMALLER).spec());
  error_strings->SetString("name", app->name());
  error_strings->SetString("msg",
      l10n_util::GetStringUTF16(IDS_ERRORPAGES_APP_WARNING));
}
