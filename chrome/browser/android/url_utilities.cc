// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/google/core/browser/google_util.h"
#include "jni/UrlUtilities_jni.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

static const char* const g_supported_schemes[] = { "about", "data", "file",
    "http", "https", "inline", "javascript", nullptr };

static const char* const g_downloadable_schemes[] = {
    "data", "blob", "file", "filesystem", "http", "https", nullptr };

static const char* const g_fallback_valid_schemes[] = {
    "http", "https", nullptr };

// TODO(mariakhomenko): figure out how to keep this list updated.
static const char* const g_google_tld_list[] = {"ac", "ad", "ae", "af", "ag",
    "al", "am", "as", "at", "aw", "az", "ba", "be", "bf", "bg", "bi", "biz",
    "bj", "bm", "bn", "bo", "bs", "bt", "by", "bz", "ca", "cat", "cc", "cd",
    "cf", "cg", "ch", "ci", "cl", "cm", "cn", "co", "co.ao", "co.at", "co.ba",
    "co.bi", "co.bw", "co.ci", "co.ck", "co.cr", "co.gg", "co.gl", "co.gy",
    "co.hu", "co.id", "co.il", "co.im", "co.in", "co.it", "co.je", "co.jp",
    "co.ke", "co.kr", "co.ls", "co.ma", "co.mu", "co.mw", "co.mz", "co.nz",
    "co.pn", "co.rs",  "co.th", "co.tt", "co.tz", "co.ua", "co.ug", "co.uk",
    "co.uz", "co.ve", "co.vi", "co.za", "co.zm", "co.zw", "com", "com.af",
    "com.ag", "com.ai", "com.ar", "com.au", "com.az", "com.bd", "com.bh",
    "com.bi", "com.bn", "com.bo", "com.br", "com.bs", "com.by", "com.bz",
    "com.cn", "com.co", "com.cu", "com.cy", "com.do", "com.dz", "com.ec",
    "com.eg", "com.er", "com.et", "com.fj", "com.ge", "com.gh", "com.gi",
    "com.gl", "com.gp", "com.gr", "com.gt", "com.gy", "com.hk", "com.hn",
    "com.hr", "com.ht", "com.iq", "com.jm", "com.jo", "com.kg", "com.kh",
    "com.ki", "com.kw", "com.kz", "com.lb", "com.lc", "com.lk", "com.lv",
    "com.ly", "com.mk", "com.mm", "com.mt", "com.mu", "com.mw", "com.mx",
    "com.my", "com.na", "com.nc", "com.nf", "com.ng", "com.ni", "com.np",
    "com.nr", "com.om", "com.pa", "com.pe", "com.pg", "com.ph", "com.pk",
    "com.pl", "com.pr", "com.ps", "com.pt", "com.py", "com.qa", "com.ru",
    "com.sa", "com.sb", "com.sc", "com.sg", "com.sl", "com.sv", "com.tj",
    "com.tm", "com.tn", "com.tr", "com.tt", "com.tw", "com.ua", "com.uy",
    "com.uz", "com.vc", "com.ve", "com.vi", "com.vn", "com.ws", "cv", "cx",
    "cz", "de", "dj", "dk", "dm", "do", "dz", "ec", "ee", "es", "eu", "fi",
    "fm", "fr", "ga", "gd", "ge", "gf", "gg", "gl", "gm", "gp", "gr", "gw",
    "gy", "hk", "hn", "hr", "ht", "hu", "ie", "im", "in", "info", "in.rs", "io",
    "iq", "is", "it", "it.ao", "je", "jo", "jobs", "jp", "kg", "ki", "kids.us",
    "km", "kn", "kr", "kz", "la", "li", "lk", "lt", "lu", "lv", "ma", "md",
    "me", "mg", "mh", "mk", "ml", "mn", "mobi", "mr", "ms", "mu", "mv", "mw",
    "mx", "name", "ne", "ne.jp", "net", "net.in", "net.nz", "nf", "ng", "nl",
    "no", "nom.es", "nr", "nu", "off.ai", "org", "org.af", "org.es", "org.in",
    "org.nz", "org.uk", "pf", "ph", "pk", "pl", "pn", "pr", "pro", "ps", "pt",
    "qa", "re", "ro", "rs", "ru", "rw", "sc", "se", "sg", "sh", "si", "sk",
    "sl", "sm", "sn", "so", "sr", "st", "sz", "td", "tel", "tg", "tk", "tl",
    "tm", "tn", "to", "tt", "tv", "tw", "ua", "ug", "us", "uz", "vc", "vg",
    "vn", "vu", "ws", "yt"};

GURL ConvertJavaStringToGURL(JNIEnv* env, jstring url) {
  return url ? GURL(ConvertJavaStringToUTF8(env, url)) : GURL();
}

bool CheckSchemeBelongsToList(
    JNIEnv* env,
    const JavaParamRef<jstring>& url,
    const char* const* scheme_list) {
  GURL gurl = ConvertJavaStringToGURL(env, url);
  if (gurl.is_valid()) {
    for (size_t i = 0; scheme_list[i]; i++) {
      if (gurl.scheme() == scheme_list[i]) {
        return true;
      }
    }
  }
  return false;
}

net::registry_controlled_domains::PrivateRegistryFilter GetRegistryFilter(
    jboolean include_private) {
  return include_private
      ? net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES
      : net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES;
}

}  // namespace

static jboolean SameDomainOrHost(JNIEnv* env,
                                 const JavaParamRef<jclass>& clazz,
                                 const JavaParamRef<jstring>& url_1_str,
                                 const JavaParamRef<jstring>& url_2_str,
                                 jboolean include_private) {
  GURL url_1 = ConvertJavaStringToGURL(env, url_1_str);
  GURL url_2 = ConvertJavaStringToGURL(env, url_2_str);

  net::registry_controlled_domains::PrivateRegistryFilter filter =
      GetRegistryFilter(include_private);

  return net::registry_controlled_domains::SameDomainOrHost(url_1,
                                                            url_2,
                                                            filter);
}

static jboolean SameHost(JNIEnv* env,
                         const JavaParamRef<jclass>& clazz,
                         const JavaParamRef<jstring>& url_1_str,
                         const JavaParamRef<jstring>& url_2_str) {
  GURL url_1 = ConvertJavaStringToGURL(env, url_1_str);
  GURL url_2 = ConvertJavaStringToGURL(env, url_2_str);
  return url_1.host_piece() == url_2.host_piece();
}

static ScopedJavaLocalRef<jstring> GetDomainAndRegistry(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& url,
    jboolean include_private) {
  DCHECK(url);
  GURL gurl = ConvertJavaStringToGURL(env, url);
  if (gurl.is_empty())
    return ScopedJavaLocalRef<jstring>();

  net::registry_controlled_domains::PrivateRegistryFilter filter =
      GetRegistryFilter(include_private);

  return base::android::ConvertUTF8ToJavaString(
      env,
      net::registry_controlled_domains::GetDomainAndRegistry(gurl, filter));
}

static jboolean IsGoogleSearchUrl(JNIEnv* env,
                                  const JavaParamRef<jclass>& clazz,
                                  const JavaParamRef<jstring>& url) {
  GURL gurl = ConvertJavaStringToGURL(env, url);
  if (gurl.is_empty())
    return false;
  bool is_search = google_util::IsGoogleSearchUrl(gurl);
  if (!is_search)
    return is_search;

  size_t length = net::registry_controlled_domains::GetRegistryLength(
      gurl,
      net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
      net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
  if ((length == 0) || (length == std::string::npos))
    return false;

  std::string tld(gurl.host().substr(gurl.host().length() - length,
                                     std::string::npos));
  // TODO(mariakhomenko): binary search instead?
  for (size_t i = 0; g_google_tld_list[i]; i++) {
    if (g_google_tld_list[i] == tld) {
      return true;
    }
  }
  return false;
}

static jboolean IsGoogleHomePageUrl(JNIEnv* env,
                                    const JavaParamRef<jclass>& clazz,
                                    const JavaParamRef<jstring>& url) {
  GURL gurl = ConvertJavaStringToGURL(env, url);
  if (gurl.is_empty())
    return false;
  return google_util::IsGoogleHomePageUrl(gurl);
}

static jboolean UrlsMatchIgnoringFragments(JNIEnv* env,
                                           const JavaParamRef<jclass>& clazz,
                                           const JavaParamRef<jstring>& url,
                                           const JavaParamRef<jstring>& url2) {
  GURL gurl = ConvertJavaStringToGURL(env, url);
  GURL gurl2 = ConvertJavaStringToGURL(env, url2);
  if (gurl.is_empty())
    return gurl2.is_empty();
  if (!gurl.is_valid() || !gurl2.is_valid())
    return false;

  GURL::Replacements replacements;
  replacements.SetRefStr("");
  return gurl.ReplaceComponents(replacements) ==
         gurl2.ReplaceComponents(replacements);
}

static jboolean UrlsFragmentsDiffer(JNIEnv* env,
                                    const JavaParamRef<jclass>& clazz,
                                    const JavaParamRef<jstring>& url,
                                    const JavaParamRef<jstring>& url2) {
  GURL gurl = ConvertJavaStringToGURL(env, url);
  GURL gurl2 = ConvertJavaStringToGURL(env, url2);
  if (gurl.is_empty())
    return !gurl2.is_empty();
  if (!gurl.is_valid() || !gurl2.is_valid())
    return true;
  return gurl.ref() != gurl2.ref();
}

static jboolean IsAcceptedScheme(JNIEnv* env,
                                 const JavaParamRef<jclass>& clazz,
                                 const JavaParamRef<jstring>& url) {
  return CheckSchemeBelongsToList(env, url, g_supported_schemes);
}

static jboolean IsValidForIntentFallbackNavigation(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& url) {
  return CheckSchemeBelongsToList(env, url, g_fallback_valid_schemes);
}

static jboolean IsDownloadable(JNIEnv* env,
                               const JavaParamRef<jclass>& clazz,
                               const JavaParamRef<jstring>& url) {
  return CheckSchemeBelongsToList(env, url, g_downloadable_schemes);
}


// Register native methods
bool RegisterUrlUtilities(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
