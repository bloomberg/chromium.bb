// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/cookie_manager_traits.h"

namespace mojo {

content::mojom::CookiePriority
EnumTraits<content::mojom::CookiePriority, net::CookiePriority>::ToMojom(
    net::CookiePriority input) {
  switch (input) {
    case net::COOKIE_PRIORITY_LOW:
      return content::mojom::CookiePriority::LOW;
    case net::COOKIE_PRIORITY_MEDIUM:
      return content::mojom::CookiePriority::MEDIUM;
    case net::COOKIE_PRIORITY_HIGH:
      return content::mojom::CookiePriority::HIGH;
  }
  NOTREACHED();
  return static_cast<content::mojom::CookiePriority>(input);
}

bool EnumTraits<content::mojom::CookiePriority, net::CookiePriority>::FromMojom(
    content::mojom::CookiePriority input,
    net::CookiePriority* output) {
  switch (input) {
    case content::mojom::CookiePriority::LOW:
      *output = net::CookiePriority::COOKIE_PRIORITY_LOW;
      return true;
    case content::mojom::CookiePriority::MEDIUM:
      *output = net::CookiePriority::COOKIE_PRIORITY_MEDIUM;
      return true;
    case content::mojom::CookiePriority::HIGH:
      *output = net::CookiePriority::COOKIE_PRIORITY_HIGH;
      return true;
  }
  return false;
}

content::mojom::CookieSameSite
EnumTraits<content::mojom::CookieSameSite, net::CookieSameSite>::ToMojom(
    net::CookieSameSite input) {
  switch (input) {
    case net::CookieSameSite::NO_RESTRICTION:
      return content::mojom::CookieSameSite::NO_RESTRICTION;
    case net::CookieSameSite::LAX_MODE:
      return content::mojom::CookieSameSite::LAX_MODE;
    case net::CookieSameSite::STRICT_MODE:
      return content::mojom::CookieSameSite::STRICT_MODE;
  }
  NOTREACHED();
  return static_cast<content::mojom::CookieSameSite>(input);
}

bool EnumTraits<content::mojom::CookieSameSite, net::CookieSameSite>::FromMojom(
    content::mojom::CookieSameSite input,
    net::CookieSameSite* output) {
  switch (input) {
    case content::mojom::CookieSameSite::NO_RESTRICTION:
      *output = net::CookieSameSite::NO_RESTRICTION;
      return true;
    case content::mojom::CookieSameSite::LAX_MODE:
      *output = net::CookieSameSite::LAX_MODE;
      return true;
    case content::mojom::CookieSameSite::STRICT_MODE:
      *output = net::CookieSameSite::STRICT_MODE;
      return true;
  }
  return false;
}

content::mojom::CookieSameSiteFilter
EnumTraits<content::mojom::CookieSameSiteFilter,
           net::CookieOptions::SameSiteCookieMode>::
    ToMojom(net::CookieOptions::SameSiteCookieMode input) {
  switch (input) {
    case net::CookieOptions::SameSiteCookieMode::INCLUDE_STRICT_AND_LAX:
      return content::mojom::CookieSameSiteFilter::INCLUDE_STRICT_AND_LAX;
    case net::CookieOptions::SameSiteCookieMode::INCLUDE_LAX:
      return content::mojom::CookieSameSiteFilter::INCLUDE_LAX;
    case net::CookieOptions::SameSiteCookieMode::DO_NOT_INCLUDE:
      return content::mojom::CookieSameSiteFilter::DO_NOT_INCLUDE;
  }
  NOTREACHED();
  return static_cast<content::mojom::CookieSameSiteFilter>(input);
}

bool EnumTraits<content::mojom::CookieSameSiteFilter,
                net::CookieOptions::SameSiteCookieMode>::
    FromMojom(content::mojom::CookieSameSiteFilter input,
              net::CookieOptions::SameSiteCookieMode* output) {
  switch (input) {
    case content::mojom::CookieSameSiteFilter::INCLUDE_STRICT_AND_LAX:
      *output = net::CookieOptions::SameSiteCookieMode::INCLUDE_STRICT_AND_LAX;
      return true;
    case content::mojom::CookieSameSiteFilter::INCLUDE_LAX:
      *output = net::CookieOptions::SameSiteCookieMode::INCLUDE_LAX;
      return true;
    case content::mojom::CookieSameSiteFilter::DO_NOT_INCLUDE:
      *output = net::CookieOptions::SameSiteCookieMode::DO_NOT_INCLUDE;
      return true;
  }
  return false;
}

bool StructTraits<content::mojom::CookieOptionsDataView, net::CookieOptions>::
    Read(content::mojom::CookieOptionsDataView mojo_options,
         net::CookieOptions* cookie_options) {
  if (mojo_options.exclude_httponly())
    cookie_options->set_exclude_httponly();
  else
    cookie_options->set_include_httponly();
  if (mojo_options.update_access_time())
    cookie_options->set_update_access_time();
  else
    cookie_options->set_do_not_update_access_time();
  base::Optional<base::Time> optional_server_time;
  if (!mojo_options.ReadServerTime(&optional_server_time))
    return false;
  if (optional_server_time) {
    cookie_options->set_server_time(*optional_server_time);
  }
  return true;
}

bool StructTraits<
    content::mojom::CanonicalCookieDataView,
    net::CanonicalCookie>::Read(content::mojom::CanonicalCookieDataView cookie,
                                net::CanonicalCookie* out) {
  std::string name;
  if (!cookie.ReadName(&name))
    return false;

  std::string value;
  if (!cookie.ReadValue(&value))
    return false;

  std::string domain;
  if (!cookie.ReadDomain(&domain))
    return false;

  std::string path;
  if (!cookie.ReadPath(&path))
    return false;

  base::Optional<base::Time> creation_time;
  base::Optional<base::Time> expiry_time;
  base::Optional<base::Time> last_access_time;
  if (!cookie.ReadCreation(&creation_time))
    return false;

  if (!cookie.ReadExpiry(&expiry_time))
    return false;

  if (!cookie.ReadLastAccess(&last_access_time))
    return false;

  net::CookieSameSite site_restrictions;
  if (!cookie.ReadSiteRestrictions(&site_restrictions))
    return false;

  net::CookiePriority priority;
  if (!cookie.ReadPriority(&priority))
    return false;

  *out = net::CanonicalCookie(
      name, value, domain, path, creation_time ? *creation_time : base::Time(),
      expiry_time ? *expiry_time : base::Time(),
      last_access_time ? *last_access_time : base::Time(), cookie.secure(),
      cookie.httponly(), site_restrictions, priority);
  return out->IsCanonical();
}

}  // namespace mojo
