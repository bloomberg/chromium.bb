// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_formatter/elide_url.h"

#include <stddef.h>

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/numerics/ranges.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/escape.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace {

#if !defined(OS_ANDROID)
const base::char16 kDot = '.';

// Build a path from the first |num_components| elements in |path_elements|.
// Prepends |path_prefix|, appends |filename|, inserts ellipsis if appropriate.
base::string16 BuildPathFromComponents(
    const base::string16& path_prefix,
    const std::vector<base::string16>& path_elements,
    const base::string16& filename,
    size_t num_components) {
  DCHECK_LE(num_components, path_elements.size());

  // Add the initial elements of the path.
  base::string16 path = path_prefix;

  // Build path from first |num_components| elements.
  for (size_t j = 0; j < num_components; ++j)
    path += path_elements[j] + gfx::kForwardSlash;

  // Add |filename|, ellipsis if necessary.
  if (num_components != (path_elements.size() - 1))
    path += base::string16(gfx::kEllipsisUTF16) + gfx::kForwardSlash;
  path += filename;

  return path;
}

// Takes a prefix (Domain, or Domain+subdomain) and a collection of path
// components and elides if possible. Returns a string containing the longest
// possible elided path, or an empty string if elision is not possible.
// Warning: This is O(url_path_elements.size() ^ 2), so it should not be called
// on a very large path.
base::string16 ElideComponentizedPath(
    const base::string16& url_path_prefix,
    const std::vector<base::string16>& url_path_elements,
    const base::string16& url_filename,
    const base::string16& url_query,
    const gfx::FontList& font_list,
    float available_pixel_width) {
  const size_t url_path_number_of_elements = url_path_elements.size();

  CHECK(url_path_number_of_elements);
  for (size_t i = url_path_number_of_elements - 1; i > 0; --i) {
    base::string16 elided_path = BuildPathFromComponents(
        url_path_prefix, url_path_elements, url_filename, i);
    if (available_pixel_width >= gfx::GetStringWidthF(elided_path, font_list))
      return gfx::ElideText(elided_path + url_query, font_list,
                            available_pixel_width, gfx::ELIDE_TAIL);
  }

  return base::string16();
}

// Splits the hostname in the |url| into sub-strings for the full hostname,
// the domain (TLD+1), and the subdomain (everything leading the domain).
void SplitHost(const GURL& url,
               base::string16* url_host,
               base::string16* url_domain,
               base::string16* url_subdomain) {
  // GURL stores IDN hostnames in punycode.  Convert back to Unicode for
  // display to the user.  (IDNToUnicode() will only perform this conversion
  // if it's safe to display this host/domain in Unicode.)
  *url_host = url_formatter::IDNToUnicode(url.host());

  // Get domain and registry information from the URL.
  std::string domain_puny =
      net::registry_controlled_domains::GetDomainAndRegistry(
          url, net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
  *url_domain = domain_puny.empty() ?
      *url_host : url_formatter::IDNToUnicode(domain_puny);

  // Add port if required.
  if (!url.port().empty()) {
    *url_host += base::UTF8ToUTF16(":" + url.port());
    *url_domain += base::UTF8ToUTF16(":" + url.port());
  }

  // Get sub domain.
  const size_t domain_start_index = url_host->find(*url_domain);
  base::string16 kWwwPrefix = base::UTF8ToUTF16("www.");
  if (domain_start_index != base::string16::npos)
    *url_subdomain = url_host->substr(0, domain_start_index);
  if ((*url_subdomain == kWwwPrefix || url_subdomain->empty() ||
       url.SchemeIsFile())) {
    url_subdomain->clear();
  }
}
#endif  // !defined(OS_ANDROID)

bool ShouldShowScheme(base::StringPiece scheme,
                      const url_formatter::SchemeDisplay scheme_display) {
  switch (scheme_display) {
    case url_formatter::SchemeDisplay::SHOW:
      return true;

    case url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS:
      return scheme != url::kHttpsScheme && scheme != url::kHttpScheme;

    case url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC:
      return scheme != url::kHttpsScheme && scheme != url::kWssScheme;
  }

  return true;
}

// TODO(jshin): Come up with a way to show Bidi URLs 'safely' (e.g. wrap up
// the entire url with {LSI, PDI} and individual domain labels with {FSI, PDI}).
// See http://crbug.com/650760 . For now, fall back to punycode if there's a
// strong RTL character.
base::string16 HostForDisplay(base::StringPiece host_in_puny) {
  base::string16 host = url_formatter::IDNToUnicode(host_in_puny);
  return base::i18n::StringContainsStrongRTLChars(host) ?
      base::ASCIIToUTF16(host_in_puny) : host;
}

}  // namespace

namespace url_formatter {

#if !defined(OS_ANDROID)

// TODO(pkasting): http://crbug.com/77883 This whole function gets
// kerning/ligatures/etc. issues potentially wrong by assuming that the width of
// a rendered string is always the sum of the widths of its substrings.  Also I
// suspect it could be made simpler.
base::string16 ElideUrl(const GURL& url,
                        const gfx::FontList& font_list,
                        float available_pixel_width) {
  // Get a formatted string and corresponding parsing of the url.
  url::Parsed parsed;
  const base::string16 url_string = url_formatter::FormatUrl(
      url, url_formatter::kFormatUrlOmitDefaults, net::UnescapeRule::SPACES,
      &parsed, nullptr, nullptr);
  if (available_pixel_width <= 0)
    return url_string;

  if (!url.IsStandard()) {
    return gfx::ElideText(url_string, font_list, available_pixel_width,
                          gfx::ELIDE_TAIL);
  }

  // Now start eliding url_string to fit within available pixel width.
  // Fist pass - check to see whether entire url_string fits.
  const float pixel_width_url_string =
      gfx::GetStringWidthF(url_string, font_list);
  if (available_pixel_width >= pixel_width_url_string)
    return url_string;

  // Get the path substring, including query and reference.
  const size_t path_start_index = parsed.path.begin;
  const size_t path_len = parsed.path.len;
  base::string16 url_path_query_etc;
  base::string16 url_path;
  if (parsed.path.is_valid()) {
    url_path_query_etc = url_string.substr(path_start_index);
    url_path = url_string.substr(path_start_index, path_len);
  }

  // Return general elided text if url minus the query fits.
  const base::string16 url_minus_query =
      url_string.substr(0, path_start_index + path_len);
  if (available_pixel_width >=
      gfx::GetStringWidthF(url_minus_query, font_list)) {
    return gfx::ElideText(url_string, font_list, available_pixel_width,
                          gfx::ELIDE_TAIL);
  }

  base::string16 url_host;
  base::string16 url_domain;
  base::string16 url_subdomain;
  SplitHost(url, &url_host, &url_domain, &url_subdomain);

  // If this is a file type, the path is now defined as everything after ":".
  // For example, "C:/aa/aa/bb", the path is "/aa/bb/cc". Interesting, the
  // domain is now C: - this is a nice hack for eliding to work pleasantly.
  if (url.SchemeIsFile()) {
    // Split the path string using ":"
    const base::string16 kColon(1, ':');
    std::vector<base::string16> file_path_split = base::SplitString(
        url_path, kColon, base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (file_path_split.size() > 1) {  // File is of type "file:///C:/.."
      url_host.clear();
      url_domain.clear();
      url_subdomain.clear();

      url_host = url_domain = file_path_split.at(0).substr(1) + kColon;
      url_path_query_etc = url_path = file_path_split.at(1);
    }
  }

  // Second Pass - remove scheme - the rest fits.
  const float pixel_width_url_host = gfx::GetStringWidthF(url_host, font_list);
  const float pixel_width_url_path =
      gfx::GetStringWidthF(url_path_query_etc, font_list);
  if (available_pixel_width >= pixel_width_url_host + pixel_width_url_path)
    return url_host + url_path_query_etc;

  // Third Pass: Subdomain, domain and entire path fits.
  const float pixel_width_url_domain =
      gfx::GetStringWidthF(url_domain, font_list);
  const float pixel_width_url_subdomain =
      gfx::GetStringWidthF(url_subdomain, font_list);
  if (available_pixel_width >=
      pixel_width_url_subdomain + pixel_width_url_domain + pixel_width_url_path)
    return url_subdomain + url_domain + url_path_query_etc;

  // Query element.
  base::string16 url_query;
  const float kPixelWidthDotsTrailer =
      gfx::GetStringWidthF(base::string16(gfx::kEllipsisUTF16), font_list);
  if (parsed.query.is_nonempty()) {
    url_query = base::UTF8ToUTF16("?") + url_string.substr(parsed.query.begin);
    if (available_pixel_width >=
        (pixel_width_url_subdomain + pixel_width_url_domain +
         pixel_width_url_path - gfx::GetStringWidthF(url_query, font_list))) {
      return gfx::ElideText(url_subdomain + url_domain + url_path_query_etc,
                            font_list, available_pixel_width, gfx::ELIDE_TAIL);
    }
  }

  // Parse url_path using '/'.
  std::vector<base::string16> url_path_elements =
      base::SplitString(url_path, base::string16(1, gfx::kForwardSlash),
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // Get filename - note that for a path ending with /
  // such as www.google.com/intl/ads/, the file name is ads/.
  base::string16 url_filename(
      url_path_elements.empty() ? base::string16() : url_path_elements.back());
  size_t url_path_number_of_elements = url_path_elements.size();
  if (url_filename.empty() && (url_path_number_of_elements > 1)) {
    // Path ends with a '/'.
    --url_path_number_of_elements;
    url_filename =
        url_path_elements[url_path_number_of_elements - 1] + gfx::kForwardSlash;
  }

  const size_t kMaxNumberOfUrlPathElementsAllowed = 1024;
  if (url_path_number_of_elements > kMaxNumberOfUrlPathElementsAllowed) {
    // Too long of a path (ElideComponentizedPath is O(N^2) so this would result
    // in degenerate behaviour). Just elide this as a text string.
    // TODO(mgiuca): Fix ElideComponentizedPath to deal with degenerate cases
    // itself, so we don't need this special case. We should not fall back on
    // ElideText if we don't know the entire domain will fit, or else we might
    // chop off the TLD. https://crbug.com/739975.
    return gfx::ElideText(url_subdomain + url_domain + url_path_query_etc,
                          font_list, available_pixel_width, gfx::ELIDE_TAIL);
  }

  // Start eliding the path and replacing elements by ".../".
  const base::string16 kEllipsisAndSlash =
      base::string16(gfx::kEllipsisUTF16) + gfx::kForwardSlash;
  const float pixel_width_ellipsis_slash =
      gfx::GetStringWidthF(kEllipsisAndSlash, font_list);

  // Check with both subdomain and domain.
  if (url_path_number_of_elements > 0) {
    base::string16 elided_path = ElideComponentizedPath(
        url_subdomain + url_domain, url_path_elements, url_filename, url_query,
        font_list, available_pixel_width);
    if (!elided_path.empty())
      return elided_path;
  }

  // Check with only domain.
  // If a subdomain is present, add an ellipsis before domain.
  // This is added only if the subdomain pixel width is larger than
  // the pixel width of kEllipsis. Otherwise, subdomain remains,
  // which means that this case has been resolved earlier.
  base::string16 url_elided_domain = url_subdomain + url_domain;
  if (pixel_width_url_subdomain > kPixelWidthDotsTrailer) {
    if (!url_subdomain.empty())
      url_elided_domain = kEllipsisAndSlash[0] + url_domain;
    else
      url_elided_domain = url_domain;

    if (url_path_number_of_elements > 0) {
      base::string16 elided_path = ElideComponentizedPath(
          url_elided_domain, url_path_elements, url_filename, url_query,
          font_list, available_pixel_width);
      if (!elided_path.empty())
        return elided_path;
    }
  }

  // Return elided domain/.../filename anyway.
  base::string16 final_elided_url_string(url_elided_domain);
  const float url_elided_domain_width =
      gfx::GetStringWidthF(url_elided_domain, font_list);

  // A hack to prevent trailing ".../...".
  if (url_path_number_of_elements > 0 &&
      url_elided_domain_width + pixel_width_ellipsis_slash +
              kPixelWidthDotsTrailer +
              gfx::GetStringWidthF(base::ASCIIToUTF16("UV"), font_list) <
          available_pixel_width) {
    final_elided_url_string += BuildPathFromComponents(
        base::string16(), url_path_elements, url_filename, 1);
  } else {
    final_elided_url_string += url_path;
  }

  return gfx::ElideText(final_elided_url_string, font_list,
                        available_pixel_width, gfx::ELIDE_TAIL);
}

base::string16 ElideHost(const GURL& url,
                         const gfx::FontList& font_list,
                         float available_pixel_width) {
  base::string16 url_host;
  base::string16 url_domain;
  base::string16 url_subdomain;
  SplitHost(url, &url_host, &url_domain, &url_subdomain);

  const float pixel_width_url_host = gfx::GetStringWidthF(url_host, font_list);
  if (available_pixel_width >= pixel_width_url_host)
    return url_host;

  if (url_subdomain.empty())
    return url_domain;

  const float pixel_width_url_domain =
      gfx::GetStringWidthF(url_domain, font_list);
  float subdomain_width = available_pixel_width - pixel_width_url_domain;
  if (subdomain_width <= 0)
    return base::string16(gfx::kEllipsisUTF16) + kDot + url_domain;

  return gfx::ElideText(url_host, font_list, available_pixel_width,
                        gfx::ELIDE_HEAD);
}

#endif  // !defined(OS_ANDROID)

base::string16 FormatUrlForSecurityDisplay(const GURL& url,
                                           const SchemeDisplay scheme_display) {
  if (!url.is_valid() || url.is_empty() || !url.IsStandard())
    return url_formatter::FormatUrl(url);

  const base::string16 colon(base::ASCIIToUTF16(":"));
  const base::string16 scheme_separator(
      base::ASCIIToUTF16(url::kStandardSchemeSeparator));

  if (url.SchemeIsFile()) {
    return base::ASCIIToUTF16(url::kFileScheme) + scheme_separator +
           base::UTF8ToUTF16(url.path());
  }

  if (url.SchemeIsFileSystem()) {
    const GURL* inner_url = url.inner_url();
    if (inner_url->SchemeIsFile()) {
      return base::ASCIIToUTF16(url::kFileSystemScheme) + colon +
             FormatUrlForSecurityDisplay(*inner_url) +
             base::UTF8ToUTF16(url.path());
    }
    return base::ASCIIToUTF16(url::kFileSystemScheme) + colon +
           FormatUrlForSecurityDisplay(*inner_url);
  }

  const GURL origin = url.GetOrigin();
  base::StringPiece scheme = origin.scheme_piece();
  base::StringPiece host = origin.host_piece();

  base::string16 result;
  if (ShouldShowScheme(scheme, scheme_display))
    result = base::UTF8ToUTF16(scheme) + scheme_separator;
  result += HostForDisplay(host);

  const int port = origin.IntPort();
  const int default_port = url::DefaultPortForScheme(
      scheme.data(), static_cast<int>(scheme.length()));
  if (port != url::PORT_UNSPECIFIED && port != default_port)
    result += colon + base::UTF8ToUTF16(origin.port_piece());

  return result;
}

base::string16 FormatOriginForSecurityDisplay(
    const url::Origin& origin,
    const SchemeDisplay scheme_display) {
  base::StringPiece scheme = origin.scheme();
  base::StringPiece host = origin.host();
  if (scheme.empty() && host.empty())
    return base::string16();

  const base::string16 colon(base::ASCIIToUTF16(":"));
  const base::string16 scheme_separator(
      base::ASCIIToUTF16(url::kStandardSchemeSeparator));

  base::string16 result;
  if (ShouldShowScheme(scheme, scheme_display))
    result = base::UTF8ToUTF16(scheme) + scheme_separator;
  result += HostForDisplay(host);

  int port = static_cast<int>(origin.port());
  const int default_port = url::DefaultPortForScheme(
      scheme.data(), static_cast<int>(scheme.length()));
  if (port != 0 && port != default_port)
    result += colon + base::NumberToString16(origin.port());

  return result;
}

}  // namespace url_formatter
