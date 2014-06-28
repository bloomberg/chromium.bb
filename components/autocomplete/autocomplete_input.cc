// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autocomplete/autocomplete_input.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autocomplete/autocomplete_scheme_classifier.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/url_fixer/url_fixer.h"
#include "net/base/net_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/url_canon_ip.h"
#include "url/url_util.h"

namespace {

// Hardcode constant to avoid any dependencies on content/.
const char kViewSourceScheme[] = "view-source";

void AdjustCursorPositionIfNecessary(size_t num_leading_chars_removed,
                                     size_t* cursor_position) {
  if (*cursor_position == base::string16::npos)
    return;
  if (num_leading_chars_removed < *cursor_position)
    *cursor_position -= num_leading_chars_removed;
  else
    *cursor_position = 0;
}

}  // namespace

AutocompleteInput::AutocompleteInput()
    : cursor_position_(base::string16::npos),
      current_page_classification_(metrics::OmniboxEventProto::INVALID_SPEC),
      type_(metrics::OmniboxInputType::INVALID),
      prevent_inline_autocomplete_(false),
      prefer_keyword_(false),
      allow_exact_keyword_match_(true),
      want_asynchronous_matches_(true) {
}

AutocompleteInput::AutocompleteInput(
    const base::string16& text,
    size_t cursor_position,
    const base::string16& desired_tld,
    const GURL& current_url,
    metrics::OmniboxEventProto::PageClassification current_page_classification,
    bool prevent_inline_autocomplete,
    bool prefer_keyword,
    bool allow_exact_keyword_match,
    bool want_asynchronous_matches,
    const AutocompleteSchemeClassifier& scheme_classifier)
    : cursor_position_(cursor_position),
      current_url_(current_url),
      current_page_classification_(current_page_classification),
      prevent_inline_autocomplete_(prevent_inline_autocomplete),
      prefer_keyword_(prefer_keyword),
      allow_exact_keyword_match_(allow_exact_keyword_match),
      want_asynchronous_matches_(want_asynchronous_matches) {
  DCHECK(cursor_position <= text.length() ||
         cursor_position == base::string16::npos)
      << "Text: '" << text << "', cp: " << cursor_position;
  // None of the providers care about leading white space so we always trim it.
  // Providers that care about trailing white space handle trimming themselves.
  if ((base::TrimWhitespace(text, base::TRIM_LEADING, &text_) &
       base::TRIM_LEADING) != 0)
    AdjustCursorPositionIfNecessary(text.length() - text_.length(),
                                    &cursor_position_);

  GURL canonicalized_url;
  type_ = Parse(text_, desired_tld, scheme_classifier, &parts_, &scheme_,
                &canonicalized_url);

  if (type_ == metrics::OmniboxInputType::INVALID)
    return;

  if (((type_ == metrics::OmniboxInputType::UNKNOWN) ||
       (type_ == metrics::OmniboxInputType::URL)) &&
      canonicalized_url.is_valid() &&
      (!canonicalized_url.IsStandard() || canonicalized_url.SchemeIsFile() ||
       canonicalized_url.SchemeIsFileSystem() ||
       !canonicalized_url.host().empty()))
    canonicalized_url_ = canonicalized_url;

  size_t chars_removed = RemoveForcedQueryStringIfNecessary(type_, &text_);
  AdjustCursorPositionIfNecessary(chars_removed, &cursor_position_);
  if (chars_removed) {
    // Remove spaces between opening question mark and first actual character.
    base::string16 trimmed_text;
    if ((base::TrimWhitespace(text_, base::TRIM_LEADING, &trimmed_text) &
         base::TRIM_LEADING) != 0) {
      AdjustCursorPositionIfNecessary(text_.length() - trimmed_text.length(),
                                      &cursor_position_);
      text_ = trimmed_text;
    }
  }
}

AutocompleteInput::~AutocompleteInput() {
}

// static
size_t AutocompleteInput::RemoveForcedQueryStringIfNecessary(
    metrics::OmniboxInputType::Type type,
    base::string16* text) {
  if ((type != metrics::OmniboxInputType::FORCED_QUERY) || text->empty() ||
      (*text)[0] != L'?')
    return 0;
  // Drop the leading '?'.
  text->erase(0, 1);
  return 1;
}

// static
std::string AutocompleteInput::TypeToString(
    metrics::OmniboxInputType::Type type) {
  switch (type) {
    case metrics::OmniboxInputType::INVALID:      return "invalid";
    case metrics::OmniboxInputType::UNKNOWN:      return "unknown";
    case metrics::OmniboxInputType::DEPRECATED_REQUESTED_URL:
      return "deprecated-requested-url";
    case metrics::OmniboxInputType::URL:          return "url";
    case metrics::OmniboxInputType::QUERY:        return "query";
    case metrics::OmniboxInputType::FORCED_QUERY: return "forced-query";
  }
  return std::string();
}

// static
metrics::OmniboxInputType::Type AutocompleteInput::Parse(
    const base::string16& text,
    const base::string16& desired_tld,
    const AutocompleteSchemeClassifier& scheme_classifier,
    url::Parsed* parts,
    base::string16* scheme,
    GURL* canonicalized_url) {
  size_t first_non_white = text.find_first_not_of(base::kWhitespaceUTF16, 0);
  if (first_non_white == base::string16::npos)
    return metrics::OmniboxInputType::INVALID;  // All whitespace.

  if (text[first_non_white] == L'?') {
    // If the first non-whitespace character is a '?', we magically treat this
    // as a query.
    return metrics::OmniboxInputType::FORCED_QUERY;
  }

  // Ask our parsing back-end to help us understand what the user typed.  We
  // use the URLFixerUpper here because we want to be smart about what we
  // consider a scheme.  For example, we shouldn't consider www.google.com:80
  // to have a scheme.
  url::Parsed local_parts;
  if (!parts)
    parts = &local_parts;
  const base::string16 parsed_scheme(url_fixer::SegmentURL(text, parts));
  if (scheme)
    *scheme = parsed_scheme;
  const std::string parsed_scheme_utf8(base::UTF16ToUTF8(parsed_scheme));

  // If we can't canonicalize the user's input, the rest of the autocomplete
  // system isn't going to be able to produce a navigable URL match for it.
  // So we just return QUERY immediately in these cases.
  GURL placeholder_canonicalized_url;
  if (!canonicalized_url)
    canonicalized_url = &placeholder_canonicalized_url;
  *canonicalized_url = url_fixer::FixupURL(base::UTF16ToUTF8(text),
                                           base::UTF16ToUTF8(desired_tld));
  if (!canonicalized_url->is_valid())
    return metrics::OmniboxInputType::QUERY;

  if (LowerCaseEqualsASCII(parsed_scheme_utf8, url::kFileScheme)) {
    // A user might or might not type a scheme when entering a file URL.  In
    // either case, |parsed_scheme_utf8| will tell us that this is a file URL,
    // but |parts->scheme| might be empty, e.g. if the user typed "C:\foo".
    return metrics::OmniboxInputType::URL;
  }

  // If the user typed a scheme, and it's HTTP or HTTPS, we know how to parse it
  // well enough that we can fall through to the heuristics below.  If it's
  // something else, we can just determine our action based on what we do with
  // any input of this scheme.  In theory we could do better with some schemes
  // (e.g. "ftp" or "view-source") but I'll wait to spend the effort on that
  // until I run into some cases that really need it.
  if (parts->scheme.is_nonempty() &&
      !LowerCaseEqualsASCII(parsed_scheme_utf8, url::kHttpScheme) &&
      !LowerCaseEqualsASCII(parsed_scheme_utf8, url::kHttpsScheme)) {
    metrics::OmniboxInputType::Type type =
        scheme_classifier.GetInputTypeForScheme(parsed_scheme_utf8);
    if (type != metrics::OmniboxInputType::INVALID)
      return type;

    // We don't know about this scheme.  It might be that the user typed a
    // URL of the form "username:password@foo.com".
    const base::string16 http_scheme_prefix =
        base::ASCIIToUTF16(std::string(url::kHttpScheme) +
                           url::kStandardSchemeSeparator);
    url::Parsed http_parts;
    base::string16 http_scheme;
    GURL http_canonicalized_url;
    metrics::OmniboxInputType::Type http_type =
        Parse(http_scheme_prefix + text, desired_tld, scheme_classifier,
              &http_parts, &http_scheme, &http_canonicalized_url);
    DCHECK_EQ(std::string(url::kHttpScheme),
              base::UTF16ToUTF8(http_scheme));

    if ((http_type == metrics::OmniboxInputType::URL) &&
        http_parts.username.is_nonempty() &&
        http_parts.password.is_nonempty()) {
      // Manually re-jigger the parsed parts to match |text| (without the
      // http scheme added).
      http_parts.scheme.reset();
      url::Component* components[] = {
        &http_parts.username,
        &http_parts.password,
        &http_parts.host,
        &http_parts.port,
        &http_parts.path,
        &http_parts.query,
        &http_parts.ref,
      };
      for (size_t i = 0; i < arraysize(components); ++i) {
        url_fixer::OffsetComponent(
            -static_cast<int>(http_scheme_prefix.length()), components[i]);
      }

      *parts = http_parts;
      if (scheme)
        scheme->clear();
      *canonicalized_url = http_canonicalized_url;

      return metrics::OmniboxInputType::URL;
    }

    // We don't know about this scheme and it doesn't look like the user
    // typed a username and password.  It's likely to be a search operator
    // like "site:" or "link:".  We classify it as UNKNOWN so the user has
    // the option of treating it as a URL if we're wrong.
    // Note that SegmentURL() is smart so we aren't tricked by "c:\foo" or
    // "www.example.com:81" in this case.
    return metrics::OmniboxInputType::UNKNOWN;
  }

  // Either the user didn't type a scheme, in which case we need to distinguish
  // between an HTTP URL and a query, or the scheme is HTTP or HTTPS, in which
  // case we should reject invalid formulations.

  // If we have an empty host it can't be a valid HTTP[S] URL.  (This should
  // only trigger for input that begins with a colon, which GURL will parse as a
  // valid, non-standard URL; for standard URLs, an empty host would have
  // resulted in an invalid |canonicalized_url| above.)
  if (!parts->host.is_nonempty())
    return metrics::OmniboxInputType::QUERY;

  // Sanity-check: GURL should have failed to canonicalize this URL if it had an
  // invalid port.
  DCHECK_NE(url::PORT_INVALID, url::ParsePort(text.c_str(), parts->port));

  // Likewise, the RCDS can reject certain obviously-invalid hosts.  (We also
  // use the registry length later below.)
  const base::string16 host(text.substr(parts->host.begin, parts->host.len));
  const size_t registry_length =
      net::registry_controlled_domains::GetRegistryLength(
          base::UTF16ToUTF8(host),
          net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
          net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
  if (registry_length == std::string::npos) {
    // Try to append the desired_tld.
    if (!desired_tld.empty()) {
      base::string16 host_with_tld(host);
      if (host[host.length() - 1] != '.')
        host_with_tld += '.';
      host_with_tld += desired_tld;
      const size_t tld_length =
          net::registry_controlled_domains::GetRegistryLength(
              base::UTF16ToUTF8(host_with_tld),
              net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
              net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
      if (tld_length != std::string::npos) {
        // Something like "99999999999" that looks like a bad IP
        // address, but becomes valid on attaching a TLD.
        return metrics::OmniboxInputType::URL;
      }
    }
    // Could be a broken IP address, etc.
    return metrics::OmniboxInputType::QUERY;
  }


  // See if the hostname is valid.  While IE and GURL allow hostnames to contain
  // many other characters (perhaps for weird intranet machines), it's extremely
  // unlikely that a user would be trying to type those in for anything other
  // than a search query.
  url::CanonHostInfo host_info;
  const std::string canonicalized_host(net::CanonicalizeHost(
      base::UTF16ToUTF8(host), &host_info));
  if ((host_info.family == url::CanonHostInfo::NEUTRAL) &&
      !net::IsCanonicalizedHostCompliant(canonicalized_host,
                                         base::UTF16ToUTF8(desired_tld))) {
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
           ((registry_length != 0) &&
            (host.find(' ') == base::string16::npos))) ?
        metrics::OmniboxInputType::UNKNOWN : metrics::OmniboxInputType::QUERY;
  }

  // Now that we've ruled out all schemes other than http or https and done a
  // little more sanity checking, the presence of a scheme means this is likely
  // a URL.
  if (parts->scheme.is_nonempty())
    return metrics::OmniboxInputType::URL;

  // See if the host is an IP address.
  if (host_info.family == url::CanonHostInfo::IPV6)
    return metrics::OmniboxInputType::URL;
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
  if ((host_info.family == url::CanonHostInfo::IPV4) &&
      (host_info.num_ipv4_components == 4))
    return metrics::OmniboxInputType::URL;

  // Presence of a password means this is likely a URL.  Note that unless the
  // user has typed an explicit "http://" or similar, we'll probably think that
  // the username is some unknown scheme, and bail out in the scheme-handling
  // code above.
  if (parts->password.is_nonempty())
    return metrics::OmniboxInputType::URL;

  // Trailing slashes force the input to be treated as a URL.
  if (parts->path.is_nonempty()) {
    char c = text[parts->path.end() - 1];
    if ((c == '\\') || (c == '/'))
      return metrics::OmniboxInputType::URL;
  }

  // If there is more than one recognized non-host component, this is likely to
  // be a URL, even if the TLD is unknown (in which case this is likely an
  // intranet URL).
  if (NumNonHostComponents(*parts) > 1)
    return metrics::OmniboxInputType::URL;

  // If the host has a known TLD or a port, it's probably a URL, with the
  // following exceptions:
  // * Any "IP addresses" that make it here are more likely searches
  //   (see above).
  // * If we reach here with a username, our input looks like "user@host[.tld]".
  //   Because there is no scheme explicitly specified, we think this is more
  //   likely an email address than an HTTP auth attempt.  Hence, we search by
  //   default and let users correct us on a case-by-case basis.
  // Note that we special-case "localhost" as a known hostname.
  if ((host_info.family != url::CanonHostInfo::IPV4) &&
      ((registry_length != 0) || (host == base::ASCIIToUTF16("localhost") ||
       parts->port.is_nonempty()))) {
    return parts->username.is_nonempty() ? metrics::OmniboxInputType::UNKNOWN :
                                           metrics::OmniboxInputType::URL;
  }

  // If we reach this point, we know there's no known TLD on the input, so if
  // the user wishes to add a desired_tld, the fixup code will oblige; thus this
  // is a URL.
  if (!desired_tld.empty())
    return metrics::OmniboxInputType::URL;

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
  return metrics::OmniboxInputType::UNKNOWN;
}

// static
void AutocompleteInput::ParseForEmphasizeComponents(
    const base::string16& text,
    const AutocompleteSchemeClassifier& scheme_classifier,
    url::Component* scheme,
    url::Component* host) {
  url::Parsed parts;
  base::string16 scheme_str;
  Parse(text, base::string16(), scheme_classifier, &parts, &scheme_str, NULL);

  *scheme = parts.scheme;
  *host = parts.host;

  int after_scheme_and_colon = parts.scheme.end() + 1;
  // For the view-source scheme, we should emphasize the scheme and host of the
  // URL qualified by the view-source prefix.
  if (LowerCaseEqualsASCII(scheme_str, kViewSourceScheme) &&
      (static_cast<int>(text.length()) > after_scheme_and_colon)) {
    // Obtain the URL prefixed by view-source and parse it.
    base::string16 real_url(text.substr(after_scheme_and_colon));
    url::Parsed real_parts;
    AutocompleteInput::Parse(real_url, base::string16(), scheme_classifier,
                             &real_parts, NULL, NULL);
    if (real_parts.scheme.is_nonempty() || real_parts.host.is_nonempty()) {
      if (real_parts.scheme.is_nonempty()) {
        *scheme = url::Component(
            after_scheme_and_colon + real_parts.scheme.begin,
            real_parts.scheme.len);
      } else {
        scheme->reset();
      }
      if (real_parts.host.is_nonempty()) {
        *host = url::Component(after_scheme_and_colon + real_parts.host.begin,
                               real_parts.host.len);
      } else {
        host->reset();
      }
    }
  } else if (LowerCaseEqualsASCII(scheme_str, url::kFileSystemScheme) &&
             parts.inner_parsed() && parts.inner_parsed()->scheme.is_valid()) {
    *host = parts.inner_parsed()->host;
  }
}

// static
base::string16 AutocompleteInput::FormattedStringWithEquivalentMeaning(
    const GURL& url,
    const base::string16& formatted_url,
    const AutocompleteSchemeClassifier& scheme_classifier) {
  if (!net::CanStripTrailingSlash(url))
    return formatted_url;
  const base::string16 url_with_path(formatted_url + base::char16('/'));
  return (AutocompleteInput::Parse(formatted_url, base::string16(),
                                   scheme_classifier, NULL, NULL, NULL) ==
          AutocompleteInput::Parse(url_with_path, base::string16(),
                                   scheme_classifier, NULL, NULL, NULL)) ?
      formatted_url : url_with_path;
}

// static
int AutocompleteInput::NumNonHostComponents(const url::Parsed& parts) {
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

// static
bool AutocompleteInput::HasHTTPScheme(const base::string16& input) {
  std::string utf8_input(base::UTF16ToUTF8(input));
  url::Component scheme;
  if (url::FindAndCompareScheme(utf8_input, kViewSourceScheme, &scheme)) {
    utf8_input.erase(0, scheme.end() + 1);
  }
  return url::FindAndCompareScheme(utf8_input, url::kHttpScheme, NULL);
}

void AutocompleteInput::UpdateText(const base::string16& text,
                                   size_t cursor_position,
                                   const url::Parsed& parts) {
  DCHECK(cursor_position <= text.length() ||
         cursor_position == base::string16::npos)
      << "Text: '" << text << "', cp: " << cursor_position;
  text_ = text;
  cursor_position_ = cursor_position;
  parts_ = parts;
}

void AutocompleteInput::Clear() {
  text_.clear();
  cursor_position_ = base::string16::npos;
  current_url_ = GURL();
  current_page_classification_ = metrics::OmniboxEventProto::INVALID_SPEC;
  type_ = metrics::OmniboxInputType::INVALID;
  parts_ = url::Parsed();
  scheme_.clear();
  canonicalized_url_ = GURL();
  prevent_inline_autocomplete_ = false;
  prefer_keyword_ = false;
  allow_exact_keyword_match_ = false;
  want_asynchronous_matches_ = true;
}
