// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_input.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "content/public/common/url_constants.h"
#include "googleurl/src/url_canon_ip.h"
#include "net/base/net_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace {

void AdjustCursorPositionIfNecessary(size_t num_leading_chars_removed,
                                     size_t* cursor_position) {
  if (*cursor_position == string16::npos)
    return;
  if (num_leading_chars_removed < *cursor_position)
    *cursor_position -= num_leading_chars_removed;
  else
    *cursor_position = 0;
}

}  // namespace

AutocompleteInput::AutocompleteInput()
    : cursor_position_(string16::npos),
      type_(INVALID),
      prevent_inline_autocomplete_(false),
      prefer_keyword_(false),
      allow_exact_keyword_match_(true),
      matches_requested_(ALL_MATCHES) {
}

AutocompleteInput::AutocompleteInput(const string16& text,
                                     size_t cursor_position,
                                     const string16& desired_tld,
                                     bool prevent_inline_autocomplete,
                                     bool prefer_keyword,
                                     bool allow_exact_keyword_match,
                                     MatchesRequested matches_requested)
    : cursor_position_(cursor_position),
      desired_tld_(desired_tld),
      prevent_inline_autocomplete_(prevent_inline_autocomplete),
      prefer_keyword_(prefer_keyword),
      allow_exact_keyword_match_(allow_exact_keyword_match),
      matches_requested_(matches_requested) {
  DCHECK(cursor_position <= text.length() || cursor_position == string16::npos);
  // None of the providers care about leading white space so we always trim it.
  // Providers that care about trailing white space handle trimming themselves.
  if ((TrimWhitespace(text, TRIM_LEADING, &text_) & TRIM_LEADING) != 0)
    AdjustCursorPositionIfNecessary(text.length() - text_.length(),
                                    &cursor_position_);

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

  size_t chars_removed = RemoveForcedQueryStringIfNecessary(type_, &text_);
  AdjustCursorPositionIfNecessary(chars_removed, &cursor_position_);
  if (chars_removed) {
    // Remove spaces between opening question mark and first actual character.
    string16 trimmed_text;
    if ((TrimWhitespace(text_, TRIM_LEADING, &trimmed_text) & TRIM_LEADING) !=
        0) {
      AdjustCursorPositionIfNecessary(text_.length() - trimmed_text.length(),
                                      &cursor_position_);
      text_ = trimmed_text;
    }
  }
}

AutocompleteInput::~AutocompleteInput() {
}

// static
size_t AutocompleteInput::RemoveForcedQueryStringIfNecessary(Type type,
                                                             string16* text) {
  if (type != FORCED_QUERY || text->empty() || (*text)[0] != L'?')
    return 0;
  // Drop the leading '?'.
  text->erase(0, 1);
  return 1;
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
                                   size_t cursor_position,
                                   const url_parse::Parsed& parts) {
  DCHECK(cursor_position <= text.length() || cursor_position == string16::npos);
  text_ = text;
  cursor_position_ = cursor_position;
  parts_ = parts;
}

bool AutocompleteInput::Equals(const AutocompleteInput& other) const {
  return (text_ == other.text_) &&
         (cursor_position_ == other.cursor_position_) &&
         (type_ == other.type_) &&
         (desired_tld_ == other.desired_tld_) &&
         (scheme_ == other.scheme_) &&
         (prevent_inline_autocomplete_ == other.prevent_inline_autocomplete_) &&
         (prefer_keyword_ == other.prefer_keyword_) &&
         (matches_requested_ == other.matches_requested_);
}

void AutocompleteInput::Clear() {
  text_.clear();
  cursor_position_ = string16::npos;
  type_ = INVALID;
  parts_ = url_parse::Parsed();
  scheme_.clear();
  desired_tld_.clear();
  prevent_inline_autocomplete_ = false;
  prefer_keyword_ = false;
}
