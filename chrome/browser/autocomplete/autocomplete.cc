// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete.h"

#include <algorithm>
#include <iterator>
#include <set>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/i18n/number_formatting.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_controller_delegate.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/builtin_provider.h"
#include "chrome/browser/autocomplete/extension_app_provider.h"
#include "chrome/browser/autocomplete/history_contents_provider.h"
#include "chrome/browser/autocomplete/history_quick_provider.h"
#include "chrome/browser/autocomplete/history_url_provider.h"
#include "chrome/browser/autocomplete/keyword_provider.h"
#include "chrome/browser/autocomplete/search_provider.h"
#include "chrome/browser/autocomplete/shortcuts_provider.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/instant/instant_field_trial.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/webui/history_ui.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_service.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_canon_ip.h"
#include "googleurl/src/url_util.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "net/base/registry_controlled_domain.h"
#include "net/url_request/url_request.h"
#include "ui/base/l10n/l10n_util.h"

using base::TimeDelta;

// AutocompleteInput ----------------------------------------------------------

AutocompleteInput::AutocompleteInput()
  : type_(INVALID),
    prevent_inline_autocomplete_(false),
    prefer_keyword_(false),
    allow_exact_keyword_match_(true),
    matches_requested_(ALL_MATCHES) {
}

AutocompleteInput::AutocompleteInput(const string16& text,
                                     const string16& desired_tld,
                                     bool prevent_inline_autocomplete,
                                     bool prefer_keyword,
                                     bool allow_exact_keyword_match,
                                     MatchesRequested matches_requested)
    : desired_tld_(desired_tld),
      prevent_inline_autocomplete_(prevent_inline_autocomplete),
      prefer_keyword_(prefer_keyword),
      allow_exact_keyword_match_(allow_exact_keyword_match),
      matches_requested_(matches_requested) {
  // None of the providers care about leading white space so we always trim it.
  // Providers that care about trailing white space handle trimming themselves.
  TrimWhitespace(text, TRIM_LEADING, &text_);

  GURL canonicalized_url;
  type_ = Parse(text_, desired_tld, &parts_, &scheme_, &canonicalized_url);

  if (type_ == INVALID)
    return;

  if (((type_ == UNKNOWN) || (type_ == REQUESTED_URL) || (type_ == URL)) &&
      canonicalized_url.is_valid() &&
      (!canonicalized_url.IsStandard() || canonicalized_url.SchemeIsFile() ||
       canonicalized_url.SchemeIsFileSystem() ||
       !canonicalized_url.host().empty()))
    canonicalized_url_ = canonicalized_url;

  RemoveForcedQueryStringIfNecessary(type_, &text_);
}

AutocompleteInput::~AutocompleteInput() {
}

// static
void AutocompleteInput::RemoveForcedQueryStringIfNecessary(Type type,
                                                           string16* text) {
  if (type == FORCED_QUERY && !text->empty() && (*text)[0] == L'?')
    text->erase(0, 1);
}

// static
std::string AutocompleteInput::TypeToString(Type type) {
  switch (type) {
    case INVALID:       return "invalid";
    case UNKNOWN:       return "unknown";
    case REQUESTED_URL: return "requested-url";
    case URL:           return "url";
    case QUERY:         return "query";
    case FORCED_QUERY:  return "forced-query";

    default:
      NOTREACHED();
      return std::string();
  }
}

// static
AutocompleteInput::Type AutocompleteInput::Parse(
    const string16& text,
    const string16& desired_tld,
    url_parse::Parsed* parts,
    string16* scheme,
    GURL* canonicalized_url) {
  const size_t first_non_white = text.find_first_not_of(kWhitespaceUTF16, 0);
  if (first_non_white == string16::npos)
    return INVALID;  // All whitespace.

  if (text.at(first_non_white) == L'?') {
    // If the first non-whitespace character is a '?', we magically treat this
    // as a query.
    return FORCED_QUERY;
  }

  // Ask our parsing back-end to help us understand what the user typed.  We
  // use the URLFixerUpper here because we want to be smart about what we
  // consider a scheme.  For example, we shouldn't consider www.google.com:80
  // to have a scheme.
  url_parse::Parsed local_parts;
  if (!parts)
    parts = &local_parts;
  const string16 parsed_scheme(URLFixerUpper::SegmentURL(text, parts));
  if (scheme)
    *scheme = parsed_scheme;
  if (canonicalized_url) {
    *canonicalized_url = URLFixerUpper::FixupURL(UTF16ToUTF8(text),
                                                 UTF16ToUTF8(desired_tld));
  }

  if (LowerCaseEqualsASCII(parsed_scheme, chrome::kFileScheme)) {
    // A user might or might not type a scheme when entering a file URL.  In
    // either case, |parsed_scheme| will tell us that this is a file URL, but
    // |parts->scheme| might be empty, e.g. if the user typed "C:\foo".
    return URL;
  }

  if (LowerCaseEqualsASCII(parsed_scheme, chrome::kFileSystemScheme)) {
    // This could theoretically be a strange search, but let's check.
    // If it's got an inner_url with a scheme, it's a URL, whether it's valid or
    // not.
    if (parts->inner_parsed() && parts->inner_parsed()->scheme.is_valid())
      return URL;
  }

  // If the user typed a scheme, and it's HTTP or HTTPS, we know how to parse it
  // well enough that we can fall through to the heuristics below.  If it's
  // something else, we can just determine our action based on what we do with
  // any input of this scheme.  In theory we could do better with some schemes
  // (e.g. "ftp" or "view-source") but I'll wait to spend the effort on that
  // until I run into some cases that really need it.
  if (parts->scheme.is_nonempty() &&
      !LowerCaseEqualsASCII(parsed_scheme, chrome::kHttpScheme) &&
      !LowerCaseEqualsASCII(parsed_scheme, chrome::kHttpsScheme)) {
    // See if we know how to handle the URL internally.
    if (ProfileIOData::IsHandledProtocol(UTF16ToASCII(parsed_scheme)))
      return URL;

    // There are also some schemes that we convert to other things before they
    // reach the renderer or else the renderer handles internally without
    // reaching the net::URLRequest logic.  We thus won't catch these above, but
    // we should still claim to handle them.
    if (LowerCaseEqualsASCII(parsed_scheme, chrome::kViewSourceScheme) ||
        LowerCaseEqualsASCII(parsed_scheme, chrome::kJavaScriptScheme) ||
        LowerCaseEqualsASCII(parsed_scheme, chrome::kDataScheme))
      return URL;

    // Finally, check and see if the user has explicitly opened this scheme as
    // a URL before, or if the "scheme" is actually a username.  We need to do
    // this last because some schemes (e.g. "javascript") may be treated as
    // "blocked" by the external protocol handler because we don't want pages to
    // open them, but users still can.
    // TODO(viettrungluu): get rid of conversion.
    ExternalProtocolHandler::BlockState block_state =
        ExternalProtocolHandler::GetBlockState(UTF16ToUTF8(parsed_scheme));
    switch (block_state) {
      case ExternalProtocolHandler::DONT_BLOCK:
        return URL;

      case ExternalProtocolHandler::BLOCK:
        // If we don't want the user to open the URL, don't let it be navigated
        // to at all.
        return QUERY;

      default: {
        // We don't know about this scheme.  It might be that the user typed a
        // URL of the form "username:password@foo.com".
        const string16 http_scheme_prefix =
            ASCIIToUTF16(std::string(chrome::kHttpScheme) +
                         content::kStandardSchemeSeparator);
        url_parse::Parsed http_parts;
        string16 http_scheme;
        GURL http_canonicalized_url;
        Type http_type = Parse(http_scheme_prefix + text, desired_tld,
                               &http_parts, &http_scheme,
                               &http_canonicalized_url);
        DCHECK_EQ(std::string(chrome::kHttpScheme), UTF16ToUTF8(http_scheme));

        if ((http_type == URL || http_type == REQUESTED_URL) &&
            http_parts.username.is_nonempty() &&
            http_parts.password.is_nonempty()) {
          // Manually re-jigger the parsed parts to match |text| (without the
          // http scheme added).
          http_parts.scheme.reset();
          url_parse::Component* components[] = {
            &http_parts.username,
            &http_parts.password,
            &http_parts.host,
            &http_parts.port,
            &http_parts.path,
            &http_parts.query,
            &http_parts.ref,
          };
          for (size_t i = 0; i < arraysize(components); ++i) {
            URLFixerUpper::OffsetComponent(
                -static_cast<int>(http_scheme_prefix.length()), components[i]);
          }

          *parts = http_parts;
          if (scheme)
            scheme->clear();
          if (canonicalized_url)
            *canonicalized_url = http_canonicalized_url;

          return http_type;
        }

        // We don't know about this scheme and it doesn't look like the user
        // typed a username and password.  It's likely to be a search operator
        // like "site:" or "link:".  We classify it as UNKNOWN so the user has
        // the option of treating it as a URL if we're wrong.
        // Note that SegmentURL() is smart so we aren't tricked by "c:\foo" or
        // "www.example.com:81" in this case.
        return UNKNOWN;
      }
    }
  }

  // Either the user didn't type a scheme, in which case we need to distinguish
  // between an HTTP URL and a query, or the scheme is HTTP or HTTPS, in which
  // case we should reject invalid formulations.

  // If we have an empty host it can't be a URL.
  if (!parts->host.is_nonempty())
    return QUERY;

  // Likewise, the RCDS can reject certain obviously-invalid hosts.  (We also
  // use the registry length later below.)
  const string16 host(text.substr(parts->host.begin, parts->host.len));
  const size_t registry_length =
      net::RegistryControlledDomainService::GetRegistryLength(UTF16ToUTF8(host),
                                                              false);
  if (registry_length == std::string::npos) {
    // Try to append the desired_tld.
    if (!desired_tld.empty()) {
      string16 host_with_tld(host);
      if (host[host.length() - 1] != '.')
        host_with_tld += '.';
      host_with_tld += desired_tld;
      if (net::RegistryControlledDomainService::GetRegistryLength(
          UTF16ToUTF8(host_with_tld), false) != std::string::npos)
        return REQUESTED_URL;  // Something like "99999999999" that looks like a
                               // bad IP address, but becomes valid on attaching
                               // a TLD.
    }
    return QUERY;  // Could be a broken IP address, etc.
  }


  // See if the hostname is valid.  While IE and GURL allow hostnames to contain
  // many other characters (perhaps for weird intranet machines), it's extremely
  // unlikely that a user would be trying to type those in for anything other
  // than a search query.
  url_canon::CanonHostInfo host_info;
  const std::string canonicalized_host(net::CanonicalizeHost(UTF16ToUTF8(host),
                                                             &host_info));
  if ((host_info.family == url_canon::CanonHostInfo::NEUTRAL) &&
      !net::IsCanonicalizedHostCompliant(canonicalized_host,
                                         UTF16ToUTF8(desired_tld))) {
    // Invalid hostname.  There are several possible cases:
    // * Our checker is too strict and the user pasted in a real-world URL
    //   that's "invalid" but resolves.  To catch these, we return UNKNOWN when
    //   the user explicitly typed a scheme, so we'll still search by default
    //   but we'll show the accidental search infobar if necessary.
    // * The user is typing a multi-word query.  If we see a space anywhere in
    //   the hostname we assume this is a search and return QUERY.
    // * Our checker is too strict and the user is typing a real-world hostname
    //   that's "invalid" but resolves.  We return UNKNOWN if the TLD is known.
    //   Note that we explicitly excluded hosts with spaces above so that
    //   "toys at amazon.com" will be treated as a search.
    // * The user is typing some garbage string.  Return QUERY.
    //
    // Thus we fall down in the following cases:
    // * Trying to navigate to a hostname with spaces
    // * Trying to navigate to a hostname with invalid characters and an unknown
    //   TLD
    // These are rare, though probably possible in intranets.
    return (parts->scheme.is_nonempty() ||
           ((registry_length != 0) && (host.find(' ') == string16::npos))) ?
        UNKNOWN : QUERY;
  }

  // A port number is a good indicator that this is a URL.  However, it might
  // also be a query like "1.66:1" that looks kind of like an IP address and
  // port number. So here we only check for "port numbers" that are illegal and
  // thus mean this can't be navigated to (e.g. "1.2.3.4:garbage"), and we save
  // handling legal port numbers until after the "IP address" determination
  // below.
  if (url_parse::ParsePort(text.c_str(), parts->port) ==
      url_parse::PORT_INVALID)
    return QUERY;

  // Now that we've ruled out all schemes other than http or https and done a
  // little more sanity checking, the presence of a scheme means this is likely
  // a URL.
  if (parts->scheme.is_nonempty())
    return URL;

  // See if the host is an IP address.
  if (host_info.family == url_canon::CanonHostInfo::IPV6)
    return URL;
  // If the user originally typed a host that looks like an IP address (a
  // dotted quad), they probably want to open it.  If the original input was
  // something else (like a single number), they probably wanted to search for
  // it, unless they explicitly typed a scheme.  This is true even if the URL
  // appears to have a path: "1.2/45" is more likely a search (for the answer
  // to a math problem) than a URL.  However, if there are more non-host
  // components, then maybe this really was intended to be a navigation.  For
  // this reason we only check the dotted-quad case here, and save the "other
  // IP addresses" case for after we check the number of non-host components
  // below.
  if ((host_info.family == url_canon::CanonHostInfo::IPV4) &&
      (host_info.num_ipv4_components == 4))
    return URL;

  // Presence of a password means this is likely a URL.  Note that unless the
  // user has typed an explicit "http://" or similar, we'll probably think that
  // the username is some unknown scheme, and bail out in the scheme-handling
  // code above.
  if (parts->password.is_nonempty())
    return URL;

  // Trailing slashes force the input to be treated as a URL.
  if (parts->path.is_nonempty()) {
    char c = text[parts->path.end() - 1];
    if ((c == '\\') || (c == '/'))
      return URL;
  }

  // If there is more than one recognized non-host component, this is likely to
  // be a URL, even if the TLD is unknown (in which case this is likely an
  // intranet URL).
  if (NumNonHostComponents(*parts) > 1)
    return URL;

  // If the host has a known TLD or a port, it's probably a URL, with the
  // following exceptions:
  // * Any "IP addresses" that make it here are more likely searches
  //   (see above).
  // * If we reach here with a username, our input looks like "user@host[.tld]".
  //   Because there is no scheme explicitly specified, we think this is more
  //   likely an email address than an HTTP auth attempt.  Hence, we search by
  //   default and let users correct us on a case-by-case basis.
  // Note that we special-case "localhost" as a known hostname.
  if ((host_info.family != url_canon::CanonHostInfo::IPV4) &&
      ((registry_length != 0) || (host == ASCIIToUTF16("localhost") ||
       parts->port.is_nonempty())))
    return parts->username.is_nonempty() ? UNKNOWN : URL;

  // If we reach this point, we know there's no known TLD on the input, so if
  // the user wishes to add a desired_tld, the fixup code will oblige; thus this
  // is a URL.
  if (!desired_tld.empty())
    return REQUESTED_URL;

  // No scheme, password, port, path, and no known TLD on the host.
  // This could be:
  // * An "incomplete IP address"; likely a search (see above).
  // * An email-like input like "user@host", where "host" has no known TLD.
  //   It's not clear what the user means here and searching seems reasonable.
  // * A single word "foo"; possibly an intranet site, but more likely a search.
  //   This is ideally an UNKNOWN, and we can let the Alternate Nav URL code
  //   catch our mistakes.
  // * A URL with a valid TLD we don't know about yet.  If e.g. a registrar adds
  //   "xxx" as a TLD, then until we add it to our data file, Chrome won't know
  //   "foo.xxx" is a real URL.  So ideally this is a URL, but we can't really
  //   distinguish this case from:
  // * A "URL-like" string that's not really a URL (like
  //   "browser.tabs.closeButtons" or "java.awt.event.*").  This is ideally a
  //   QUERY.  Since this is indistinguishable from the case above, and this
  //   case is much more likely, claim these are UNKNOWN, which should default
  //   to the right thing and let users correct us on a case-by-case basis.
  return UNKNOWN;
}

// static
void AutocompleteInput::ParseForEmphasizeComponents(
    const string16& text,
    const string16& desired_tld,
    url_parse::Component* scheme,
    url_parse::Component* host) {
  url_parse::Parsed parts;
  string16 scheme_str;
  Parse(text, desired_tld, &parts, &scheme_str, NULL);

  *scheme = parts.scheme;
  *host = parts.host;

  int after_scheme_and_colon = parts.scheme.end() + 1;
  // For the view-source scheme, we should emphasize the scheme and host of the
  // URL qualified by the view-source prefix.
  if (LowerCaseEqualsASCII(scheme_str, chrome::kViewSourceScheme) &&
      (static_cast<int>(text.length()) > after_scheme_and_colon)) {
    // Obtain the URL prefixed by view-source and parse it.
    string16 real_url(text.substr(after_scheme_and_colon));
    url_parse::Parsed real_parts;
    AutocompleteInput::Parse(real_url, desired_tld, &real_parts, NULL,
                             NULL);
    if (real_parts.scheme.is_nonempty() || real_parts.host.is_nonempty()) {
      if (real_parts.scheme.is_nonempty()) {
        *scheme = url_parse::Component(
            after_scheme_and_colon + real_parts.scheme.begin,
            real_parts.scheme.len);
      } else {
        scheme->reset();
      }
      if (real_parts.host.is_nonempty()) {
        *host = url_parse::Component(
            after_scheme_and_colon + real_parts.host.begin,
            real_parts.host.len);
      } else {
        host->reset();
      }
    }
  } else if (LowerCaseEqualsASCII(scheme_str, chrome::kFileSystemScheme) &&
             parts.inner_parsed() && parts.inner_parsed()->scheme.is_valid()) {
    *host = parts.inner_parsed()->host;
  }
}

// static
string16 AutocompleteInput::FormattedStringWithEquivalentMeaning(
    const GURL& url,
    const string16& formatted_url) {
  if (!net::CanStripTrailingSlash(url))
    return formatted_url;
  const string16 url_with_path(formatted_url + char16('/'));
  return (AutocompleteInput::Parse(formatted_url, string16(), NULL, NULL,
                                   NULL) ==
          AutocompleteInput::Parse(url_with_path, string16(), NULL, NULL,
                                   NULL)) ?
      formatted_url : url_with_path;
}

// static
int AutocompleteInput::NumNonHostComponents(const url_parse::Parsed& parts) {
  int num_nonhost_components = 0;
  if (parts.scheme.is_nonempty())
    ++num_nonhost_components;
  if (parts.username.is_nonempty())
    ++num_nonhost_components;
  if (parts.password.is_nonempty())
    ++num_nonhost_components;
  if (parts.port.is_nonempty())
    ++num_nonhost_components;
  if (parts.path.is_nonempty())
    ++num_nonhost_components;
  if (parts.query.is_nonempty())
    ++num_nonhost_components;
  if (parts.ref.is_nonempty())
    ++num_nonhost_components;
  return num_nonhost_components;
}

void AutocompleteInput::UpdateText(const string16& text,
                                   const url_parse::Parsed& parts) {
  text_ = text;
  parts_ = parts;
}

bool AutocompleteInput::Equals(const AutocompleteInput& other) const {
  return (text_ == other.text_) &&
         (type_ == other.type_) &&
         (desired_tld_ == other.desired_tld_) &&
         (scheme_ == other.scheme_) &&
         (prevent_inline_autocomplete_ == other.prevent_inline_autocomplete_) &&
         (prefer_keyword_ == other.prefer_keyword_) &&
         (matches_requested_ == other.matches_requested_);
}

void AutocompleteInput::Clear() {
  text_.clear();
  type_ = INVALID;
  parts_ = url_parse::Parsed();
  scheme_.clear();
  desired_tld_.clear();
  prevent_inline_autocomplete_ = false;
  prefer_keyword_ = false;
}

// AutocompleteProvider -------------------------------------------------------

// static
const size_t AutocompleteProvider::kMaxMatches = 3;

AutocompleteProvider::ACProviderListener::~ACProviderListener() {
}

AutocompleteProvider::AutocompleteProvider(ACProviderListener* listener,
                                           Profile* profile,
                                           const char* name)
    : profile_(profile),
      listener_(listener),
      done_(true),
      name_(name) {
}

void AutocompleteProvider::Stop() {
  done_ = true;
}

void AutocompleteProvider::DeleteMatch(const AutocompleteMatch& match) {
  DLOG(WARNING) << "The AutocompleteProvider '" << name()
                << "' has not implemented DeleteMatch.";
}

AutocompleteProvider::~AutocompleteProvider() {
  Stop();
}

// static
bool AutocompleteProvider::HasHTTPScheme(const string16& input) {
  std::string utf8_input(UTF16ToUTF8(input));
  url_parse::Component scheme;
  if (url_util::FindAndCompareScheme(utf8_input, chrome::kViewSourceScheme,
                                     &scheme))
    utf8_input.erase(0, scheme.end() + 1);
  return url_util::FindAndCompareScheme(utf8_input, chrome::kHttpScheme, NULL);
}

void AutocompleteProvider::UpdateStarredStateOfMatches() {
  if (matches_.empty())
    return;

  if (!profile_)
    return;
  BookmarkModel* bookmark_model = profile_->GetBookmarkModel();
  if (!bookmark_model || !bookmark_model->IsLoaded())
    return;

  for (ACMatches::iterator i = matches_.begin(); i != matches_.end(); ++i)
    i->starred = bookmark_model->IsBookmarked(GURL(i->destination_url));
}

string16 AutocompleteProvider::StringForURLDisplay(const GURL& url,
                                                   bool check_accept_lang,
                                                   bool trim_http) const {
  std::string languages = (check_accept_lang && profile_) ?
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages) : std::string();
  return net::FormatUrl(
      url,
      languages,
      net::kFormatUrlOmitAll & ~(trim_http ? 0 : net::kFormatUrlOmitHTTP),
      net::UnescapeRule::SPACES, NULL, NULL, NULL);
}

// AutocompleteResult ---------------------------------------------------------

// static
const size_t AutocompleteResult::kMaxMatches = 6;
const int AutocompleteResult::kLowestDefaultScore = 1200;

void AutocompleteResult::Selection::Clear() {
  destination_url = GURL();
  provider_affinity = NULL;
  is_history_what_you_typed_match = false;
}

AutocompleteResult::AutocompleteResult() {
  // Reserve space for the max number of matches we'll show.
  matches_.reserve(kMaxMatches);

  // It's probably safe to do this in the initializer list, but there's little
  // penalty to doing it here and it ensures our object is fully constructed
  // before calling member functions.
  default_match_ = end();
}

AutocompleteResult::~AutocompleteResult() {}

void AutocompleteResult::CopyFrom(const AutocompleteResult& rhs) {
  if (this == &rhs)
    return;

  matches_ = rhs.matches_;
  // Careful!  You can't just copy iterators from another container, you have to
  // reconstruct them.
  default_match_ = (rhs.default_match_ == rhs.end()) ?
      end() : (begin() + (rhs.default_match_ - rhs.begin()));

  alternate_nav_url_ = rhs.alternate_nav_url_;
}

void AutocompleteResult::CopyOldMatches(const AutocompleteInput& input,
                                        const AutocompleteResult& old_matches) {
  if (old_matches.empty())
    return;

  if (empty()) {
    // If we've got no matches we can copy everything from the last result.
    CopyFrom(old_matches);
    for (ACMatches::iterator i = begin(); i != end(); ++i)
      i->from_previous = true;
    return;
  }

  // In hopes of providing a stable popup we try to keep the number of matches
  // per provider consistent. Other schemes (such as blindly copying the most
  // relevant matches) typically result in many successive 'What You Typed'
  // results filling all the matches, which looks awful.
  //
  // Instead of starting with the current matches and then adding old matches
  // until we hit our overall limit, we copy enough old matches so that each
  // provider has at least as many as before, and then use SortAndCull() to
  // clamp globally. This way, old high-relevance matches will starve new
  // low-relevance matches, under the assumption that the new matches will
  // ultimately be similar.  If the assumption holds, this prevents seeing the
  // new low-relevance match appear and then quickly get pushed off the bottom;
  // if it doesn't, then once the providers are done and we expire the old
  // matches, the new ones will all become visible, so we won't have lost
  // anything permanently.
  ProviderToMatches matches_per_provider, old_matches_per_provider;
  BuildProviderToMatches(&matches_per_provider);
  old_matches.BuildProviderToMatches(&old_matches_per_provider);
  for (ProviderToMatches::const_iterator i = old_matches_per_provider.begin();
       i != old_matches_per_provider.end(); ++i) {
    MergeMatchesByProvider(i->second, matches_per_provider[i->first]);
  }

  SortAndCull(input);
}

void AutocompleteResult::AppendMatches(const ACMatches& matches) {
#ifndef NDEBUG
  for (ACMatches::const_iterator i = matches.begin(); i != matches.end(); ++i) {
    DCHECK_EQ(AutocompleteMatch::SanitizeString(i->contents), i->contents);
    DCHECK_EQ(AutocompleteMatch::SanitizeString(i->description),
              i->description);
  }
#endif
  std::copy(matches.begin(), matches.end(), std::back_inserter(matches_));
  default_match_ = end();
  alternate_nav_url_ = GURL();
}

void AutocompleteResult::AddMatch(const AutocompleteMatch& match) {
  DCHECK(default_match_ != end());
  DCHECK_EQ(AutocompleteMatch::SanitizeString(match.contents), match.contents);
  DCHECK_EQ(AutocompleteMatch::SanitizeString(match.description),
            match.description);
  ACMatches::iterator insertion_point =
      std::upper_bound(begin(), end(), match, &AutocompleteMatch::MoreRelevant);
  matches_difference_type default_offset = default_match_ - begin();
  if ((insertion_point - begin()) <= default_offset)
    ++default_offset;
  matches_.insert(insertion_point, match);
  default_match_ = begin() + default_offset;
}

void AutocompleteResult::SortAndCull(const AutocompleteInput& input) {
  for (ACMatches::iterator i = matches_.begin(); i != matches_.end(); ++i)
    i->ComputeStrippedDestinationURL();

  // Remove duplicates.
  std::sort(matches_.begin(), matches_.end(),
            &AutocompleteMatch::DestinationSortFunc);
  matches_.erase(std::unique(matches_.begin(), matches_.end(),
                             &AutocompleteMatch::DestinationsEqual),
                 matches_.end());

  // Sort and trim to the most relevant kMaxMatches matches.
  const size_t num_matches = std::min(kMaxMatches, matches_.size());
  std::partial_sort(matches_.begin(), matches_.begin() + num_matches,
                    matches_.end(), &AutocompleteMatch::MoreRelevant);
  matches_.resize(num_matches);

  default_match_ = begin();

  // Set the alternate nav URL.
  alternate_nav_url_ = GURL();
  if (((input.type() == AutocompleteInput::UNKNOWN) ||
       (input.type() == AutocompleteInput::REQUESTED_URL)) &&
      (default_match_ != end()) &&
      (default_match_->transition != content::PAGE_TRANSITION_TYPED) &&
      (default_match_->transition != content::PAGE_TRANSITION_KEYWORD) &&
      (input.canonicalized_url() != default_match_->destination_url))
    alternate_nav_url_ = input.canonicalized_url();
}

bool AutocompleteResult::HasCopiedMatches() const {
  for (ACMatches::const_iterator i = begin(); i != end(); ++i) {
    if (i->from_previous)
      return true;
  }
  return false;
}

size_t AutocompleteResult::size() const {
  return matches_.size();
}

bool AutocompleteResult::empty() const {
  return matches_.empty();
}

AutocompleteResult::const_iterator AutocompleteResult::begin() const {
  return matches_.begin();
}

AutocompleteResult::iterator AutocompleteResult::begin() {
  return matches_.begin();
}

AutocompleteResult::const_iterator AutocompleteResult::end() const {
  return matches_.end();
}

AutocompleteResult::iterator AutocompleteResult::end() {
  return matches_.end();
}

// Returns the match at the given index.
const AutocompleteMatch& AutocompleteResult::match_at(size_t index) const {
  DCHECK(index < matches_.size());
  return matches_[index];
}

void AutocompleteResult::Reset() {
  matches_.clear();
  default_match_ = end();
}

void AutocompleteResult::Swap(AutocompleteResult* other) {
  const size_t default_match_offset = default_match_ - begin();
  const size_t other_default_match_offset =
      other->default_match_ - other->begin();
  matches_.swap(other->matches_);
  default_match_ = begin() + other_default_match_offset;
  other->default_match_ = other->begin() + default_match_offset;
  alternate_nav_url_.Swap(&(other->alternate_nav_url_));
}

#ifndef NDEBUG
void AutocompleteResult::Validate() const {
  for (const_iterator i(begin()); i != end(); ++i)
    i->Validate();
}
#endif

void AutocompleteResult::BuildProviderToMatches(
    ProviderToMatches* provider_to_matches) const {
  for (ACMatches::const_iterator i = begin(); i != end(); ++i)
    (*provider_to_matches)[i->provider].push_back(*i);
}

// static
bool AutocompleteResult::HasMatchByDestination(const AutocompleteMatch& match,
                                               const ACMatches& matches) {
  for (ACMatches::const_iterator i = matches.begin(); i != matches.end(); ++i) {
    if (i->destination_url == match.destination_url)
      return true;
  }
  return false;
}

void AutocompleteResult::MergeMatchesByProvider(const ACMatches& old_matches,
                                                const ACMatches& new_matches) {
  if (new_matches.size() >= old_matches.size())
    return;

  size_t delta = old_matches.size() - new_matches.size();
  const int max_relevance = (new_matches.empty() ?
      matches_.front().relevance : new_matches[0].relevance) - 1;
  // Because the goal is a visibly-stable popup, rather than one that preserves
  // the highest-relevance matches, we copy in the lowest-relevance matches
  // first. This means that within each provider's "group" of matches, any
  // synchronous matches (which tend to have the highest scores) will
  // "overwrite" the initial matches from that provider's previous results,
  // minimally disturbing the rest of the matches.
  for (ACMatches::const_reverse_iterator i = old_matches.rbegin();
       i != old_matches.rend() && delta > 0; ++i) {
    if (!HasMatchByDestination(*i, new_matches)) {
      AutocompleteMatch match = *i;
      match.relevance = std::min(max_relevance, match.relevance);
      match.from_previous = true;
      AddMatch(match);
      delta--;
    }
  }
}

// AutocompleteController -----------------------------------------------------

const int AutocompleteController::kNoItemSelected = -1;

// Amount of time (in ms) between when the user stops typing and when we remove
// any copied entries. We do this from the time the user stopped typing as some
// providers (such as SearchProvider) wait for the user to stop typing before
// they initiate a query.
static const int kExpireTimeMS = 500;

AutocompleteController::AutocompleteController(
    Profile* profile,
    AutocompleteControllerDelegate* delegate)
    : delegate_(delegate),
      keyword_provider_(NULL),
      done_(true),
      in_start_(false),
      profile_(profile) {
  search_provider_ = new SearchProvider(this, profile);
  providers_.push_back(search_provider_);
  // TODO(mrossetti): Remove the following and permanently modify the
  // HistoryURLProvider to not search titles once HQP is turned on permanently.
  bool hqp_enabled = !CommandLine::ForCurrentProcess()->HasSwitch(
                         switches::kDisableHistoryQuickProvider);
  if (hqp_enabled)
    providers_.push_back(new HistoryQuickProvider(this, profile));
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableShortcutsProvider))
    providers_.push_back(new ShortcutsProvider(this, profile));
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableHistoryURLProvider))
    providers_.push_back(new HistoryURLProvider(this, profile));
#if !defined(OS_ANDROID)
  // No search provider/"tab to search" on Android.
  keyword_provider_ = new KeywordProvider(this, profile);
  providers_.push_back(keyword_provider_);
#endif  // !OS_ANDROID
  providers_.push_back(new HistoryContentsProvider(this, profile, hqp_enabled));
  providers_.push_back(new BuiltinProvider(this, profile));
  providers_.push_back(new ExtensionAppProvider(this, profile));
  for (ACProviders::iterator i(providers_.begin()); i != providers_.end(); ++i)
    (*i)->AddRef();
}

AutocompleteController::~AutocompleteController() {
  // The providers may have tasks outstanding that hold refs to them.  We need
  // to ensure they won't call us back if they outlive us.  (Practically,
  // calling Stop() should also cancel those tasks and make it so that we hold
  // the only refs.)  We also don't want to bother notifying anyone of our
  // result changes here, because the notification observer is in the midst of
  // shutdown too, so we don't ask Stop() to clear |result_| (and notify).
  result_.Reset();  // Not really necessary.
  Stop(false);

  for (ACProviders::iterator i(providers_.begin()); i != providers_.end(); ++i)
    (*i)->Release();

  providers_.clear();  // Not really necessary.
}

void AutocompleteController::Start(
    const string16& text,
    const string16& desired_tld,
    bool prevent_inline_autocomplete,
    bool prefer_keyword,
    bool allow_exact_keyword_match,
    AutocompleteInput::MatchesRequested matches_requested) {
  const string16 old_input_text(input_.text());
  const AutocompleteInput::MatchesRequested old_matches_requested =
      input_.matches_requested();
  input_ = AutocompleteInput(text, desired_tld, prevent_inline_autocomplete,
      prefer_keyword, allow_exact_keyword_match, matches_requested);

  // See if we can avoid rerunning autocomplete when the query hasn't changed
  // much.  When the user presses or releases the ctrl key, the desired_tld
  // changes, and when the user finishes an IME composition, inline autocomplete
  // may no longer be prevented.  In both these cases the text itself hasn't
  // changed since the last query, and some providers can do much less work (and
  // get matches back more quickly).  Taking advantage of this reduces flicker.
  //
  // NOTE: This comes after constructing |input_| above since that construction
  // can change the text string (e.g. by stripping off a leading '?').
  const bool minimal_changes = (input_.text() == old_input_text) &&
      (input_.matches_requested() == old_matches_requested);

  expire_timer_.Stop();

  // Start the new query.
  in_start_ = true;
  base::TimeTicks start_time = base::TimeTicks::Now();
  for (ACProviders::iterator i(providers_.begin()); i != providers_.end();
       ++i) {
    (*i)->Start(input_, minimal_changes);
    if (matches_requested != AutocompleteInput::ALL_MATCHES)
      DCHECK((*i)->done());
  }
  if (matches_requested == AutocompleteInput::ALL_MATCHES &&
      (text.length() < 6)) {
    base::TimeTicks end_time = base::TimeTicks::Now();
    std::string name = "Omnibox.QueryTime." + base::IntToString(text.length()) +
        InstantFieldTrial::GetModeAsString(profile_);
    base::Histogram* counter = base::Histogram::FactoryGet(
        name, 1, 1000, 50, base::Histogram::kUmaTargetedHistogramFlag);
    counter->Add(static_cast<int>((end_time - start_time).InMilliseconds()));
  }
  in_start_ = false;
  CheckIfDone();
  UpdateResult(true);

  if (!done_)
    StartExpireTimer();
}

void AutocompleteController::Stop(bool clear_result) {
  for (ACProviders::const_iterator i(providers_.begin()); i != providers_.end();
       ++i) {
    (*i)->Stop();
  }

  expire_timer_.Stop();
  done_ = true;
  if (clear_result && !result_.empty()) {
    result_.Reset();
    // NOTE: We pass in false since we're trying to only clear the popup, not
    // touch the edit... this is all a mess and should be cleaned up :(
    NotifyChanged(false);
  }
}

void AutocompleteController::DeleteMatch(const AutocompleteMatch& match) {
  DCHECK(match.deletable);
  match.provider->DeleteMatch(match);  // This may synchronously call back to
                                       // OnProviderUpdate().
  // If DeleteMatch resulted in a callback to OnProviderUpdate and we're
  // not done, we might attempt to redisplay the deleted match. Make sure
  // we aren't displaying it by removing any old entries.
  ExpireCopiedEntries();
}

void AutocompleteController::ExpireCopiedEntries() {
  // Clear out the results. This ensures no results from the previous result set
  // are copied over.
  result_.Reset();
  // We allow matches from the previous result set to starve out matches from
  // the new result set. This means in order to expire matches we have to query
  // the providers again.
  UpdateResult(false);
}

void AutocompleteController::OnProviderUpdate(bool updated_matches) {
  CheckIfDone();
  // Multiple providers may provide synchronous results, so we only update the
  // results if we're not in Start().
  if (!in_start_ && (updated_matches || done_))
    UpdateResult(false);
}

void AutocompleteController::UpdateResult(bool is_synchronous_pass) {
  AutocompleteResult last_result;
  last_result.Swap(&result_);

  for (ACProviders::const_iterator i(providers_.begin()); i != providers_.end();
       ++i)
    result_.AppendMatches((*i)->matches());

  // Sort the matches and trim to a small number of "best" matches.
  result_.SortAndCull(input_);

  // Need to validate before invoking CopyOldMatches as the old matches are not
  // valid against the current input.
#ifndef NDEBUG
  result_.Validate();
#endif

  if (!done_) {
    // This conditional needs to match the conditional in Start that invokes
    // StartExpireTimer.
    result_.CopyOldMatches(input_, last_result);
  }

  UpdateKeywordDescriptions(&result_);
  UpdateAssociatedKeywords(&result_);

  bool notify_default_match = is_synchronous_pass;
  if (!is_synchronous_pass) {
    const bool last_default_was_valid =
        last_result.default_match() != last_result.end();
    const bool default_is_valid = result_.default_match() != result_.end();
    // We've gotten async results. Send notification that the default match
    // updated if fill_into_edit differs or associated_keyword differ.  (The
    // latter can change if we've just started Chrome and the keyword database
    // finishes loading while processing this request.) We don't check the URL
    // as that may change for the default match even though the fill into edit
    // hasn't changed (see SearchProvider for one case of this).
    notify_default_match =
        (last_default_was_valid != default_is_valid) ||
        (default_is_valid &&
          ((result_.default_match()->fill_into_edit !=
            last_result.default_match()->fill_into_edit) ||
            (result_.default_match()->associated_keyword.get() !=
              last_result.default_match()->associated_keyword.get())));
  }

  NotifyChanged(notify_default_match);
}

void AutocompleteController::UpdateAssociatedKeywords(
    AutocompleteResult* result) {
  if (!keyword_provider_)
    return;

  std::set<string16> keywords;
  for (ACMatches::iterator match(result->begin()); match != result->end();
       ++match) {
    string16 keyword(match->GetSubstitutingExplicitlyInvokedKeyword(profile_));
    if (!keyword.empty()) {
      keywords.insert(keyword);
    } else {
      string16 keyword = match->associated_keyword.get() ?
          match->associated_keyword->keyword :
          keyword_provider_->GetKeywordForText(match->fill_into_edit);

      // Only add the keyword if the match does not have a duplicate keyword
      // with a more relevant match.
      if (!keyword.empty() && !keywords.count(keyword)) {
        keywords.insert(keyword);

        if (!match->associated_keyword.get())
          match->associated_keyword.reset(new AutocompleteMatch(
              keyword_provider_->CreateAutocompleteMatch(match->fill_into_edit,
                  keyword, input_)));
      } else {
        match->associated_keyword.reset();
      }
    }
  }
}

void AutocompleteController::UpdateKeywordDescriptions(
    AutocompleteResult* result) {
  string16 last_keyword;
  for (AutocompleteResult::iterator i = result->begin(); i != result->end();
       ++i) {
    if (((i->provider == keyword_provider_) && !i->keyword.empty()) ||
        ((i->provider == search_provider_) &&
         (i->type == AutocompleteMatch::SEARCH_WHAT_YOU_TYPED ||
          i->type == AutocompleteMatch::SEARCH_HISTORY ||
          i->type == AutocompleteMatch::SEARCH_SUGGEST))) {
      i->description.clear();
      i->description_class.clear();
      DCHECK(!i->keyword.empty());
      if (i->keyword != last_keyword) {
        const TemplateURL* template_url = i->GetTemplateURL(profile_);
        if (template_url) {
          i->description = l10n_util::GetStringFUTF16(
              IDS_AUTOCOMPLETE_SEARCH_DESCRIPTION,
              template_url->AdjustedShortNameForLocaleDirection());
          i->description_class.push_back(
              ACMatchClassification(0, ACMatchClassification::DIM));
        }
        last_keyword = i->keyword;
      }
    } else {
      last_keyword.clear();
    }
  }
}

void AutocompleteController::NotifyChanged(bool notify_default_match) {
  if (delegate_)
    delegate_->OnResultChanged(notify_default_match);
  if (done_) {
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_AUTOCOMPLETE_CONTROLLER_RESULT_READY,
        content::Source<AutocompleteController>(this),
        content::NotificationService::NoDetails());
  }
}

void AutocompleteController::CheckIfDone() {
  for (ACProviders::const_iterator i(providers_.begin()); i != providers_.end();
       ++i) {
    if (!(*i)->done()) {
      done_ = false;
      return;
    }
  }
  done_ = true;
}

void AutocompleteController::StartExpireTimer() {
  if (result_.HasCopiedMatches())
    expire_timer_.Start(FROM_HERE,
                        base::TimeDelta::FromMilliseconds(kExpireTimeMS),
                        this, &AutocompleteController::ExpireCopiedEntries);
}

// AutocompleteLog ---------------------------------------------------------

AutocompleteLog::AutocompleteLog(
    const string16& text,
    bool just_deleted_text,
    AutocompleteInput::Type input_type,
    size_t selected_index,
    SessionID::id_type tab_id,
    metrics::OmniboxEventProto::PageClassification current_page_classification,
    base::TimeDelta elapsed_time_since_user_first_modified_omnibox,
    size_t inline_autocompleted_length,
    const AutocompleteResult& result)
    : text(text),
      just_deleted_text(just_deleted_text),
      input_type(input_type),
      selected_index(selected_index),
      tab_id(tab_id),
      current_page_classification(current_page_classification),
      elapsed_time_since_user_first_modified_omnibox(
          elapsed_time_since_user_first_modified_omnibox),
      inline_autocompleted_length(inline_autocompleted_length),
      result(result) {
}
