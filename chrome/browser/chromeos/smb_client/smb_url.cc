// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smb_url.h"

#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/smb_client/smb_constants.h"
#include "url/url_canon_stdstring.h"

namespace chromeos {
namespace smb_client {

namespace {

const char kDoubleBackslash[] = "\\\\";

// Returns true if |url| starts with "smb://" or "\\".
bool ShouldProcessUrl(const std::string& url) {
  return base::StartsWith(url, kSmbSchemePrefix,
                          base::CompareCase::INSENSITIVE_ASCII) ||
         base::StartsWith(url, kDoubleBackslash,
                          base::CompareCase::INSENSITIVE_ASCII);
}

// Adds "smb://" to the beginning of |url| if not present.
std::string AddSmbSchemeIfMissing(const std::string& url) {
  DCHECK(ShouldProcessUrl(url));

  if (base::StartsWith(url, kSmbSchemePrefix,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return url;
  }

  return std::string(kSmbSchemePrefix) + url;
}

// Returns true if |parsed| contains a username, password, port, query, or ref.
bool ContainsUnnecessaryComponents(const url::Parsed& parsed) {
  return parsed.username.is_nonempty() || parsed.password.is_nonempty() ||
         parsed.port.is_nonempty() || parsed.query.is_nonempty() ||
         parsed.ref.is_nonempty();
}

// Parses |url| into |parsed|. Returns true if the URL does not contain
// unnecessary components.
bool ParseAndValidateUrl(const std::string& url, url::Parsed* parsed) {
  DCHECK(parsed);
  DCHECK(ShouldProcessUrl(url));

  url::ParseStandardURL(url.c_str(), url.size(), parsed);
  return !ContainsUnnecessaryComponents(*parsed);
}

}  // namespace

SmbUrl::SmbUrl(const std::string& raw_url) {
  // Only process |url| if it starts with "smb://" or "\\".
  if (ShouldProcessUrl(raw_url)) {
    // Add "smb://" if |url| starts with "\\" and canonicalize the URL.
    CanonicalizeSmbUrl(AddSmbSchemeIfMissing(raw_url));
  }
}

SmbUrl::~SmbUrl() = default;

SmbUrl::SmbUrl(SmbUrl&& smb_url) = default;

std::string SmbUrl::GetHost() const {
  DCHECK(IsValid());

  return url_.substr(host_.begin, host_.len);
}

const std::string& SmbUrl::ToString() const {
  DCHECK(IsValid());

  return url_;
}

std::string SmbUrl::ReplaceHost(const std::string& new_host) const {
  DCHECK(IsValid());

  std::string temp = url_;
  temp.replace(host_.begin, host_.len, new_host);
  return temp;
}

bool SmbUrl::IsValid() const {
  return !url_.empty() && host_.is_valid();
}

void SmbUrl::CanonicalizeSmbUrl(const std::string& url) {
  DCHECK(!IsValid());
  DCHECK(ShouldProcessUrl(url));

  // Get the initial parse of |url| and ensure that it does not contain
  // unnecessary components.
  url::Parsed initial_parsed;
  if (!ParseAndValidateUrl(url, &initial_parsed)) {
    return;
  }

  url::StdStringCanonOutput canonical_output(&url_);

  url::Component scheme;
  if (!url::CanonicalizeScheme(url.c_str(), initial_parsed.scheme,
                               &canonical_output, &scheme)) {
    Reset();
    return;
  }

  canonical_output.push_back('/');
  canonical_output.push_back('/');

  url::Component unused_path;
  if (!(url::CanonicalizeHost(url.c_str(), initial_parsed.host,
                              &canonical_output, &host_) &&
        url::CanonicalizePath(url.c_str(), initial_parsed.path,
                              &canonical_output, &unused_path))) {
    Reset();
    return;
  }

  // Check IsValid here since url::Canonicalize* may return true even if it did
  // not actually parse the component.
  if (!IsValid()) {
    Reset();
    return;
  }

  canonical_output.Complete();

  DCHECK(host_.is_nonempty());
  DCHECK_EQ(url_.substr(scheme.begin, scheme.len), kSmbScheme);
}

void SmbUrl::Reset() {
  host_.reset();
  url_.clear();
}

}  // namespace smb_client
}  // namespace chromeos
