// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_utils.h"

#include <string>

#include "base/logging.h"
#include "base/strings/string_util.h"

namespace content {

namespace {

const char kDisallowedCharacterErrorMessage[] =
    "The scope/script URL includes disallowed escaped character.";
const char kPathRestrictionErrorMessage[] =
    "The scope must be under the directory of the script URL.";

bool ContainsDisallowedCharacter(const GURL& url) {
  std::string path = url.path();
  DCHECK(base::IsStringUTF8(path));

  // We should avoid these escaped characters in the path component because
  // these can be handled differently depending on server implementation.
  if (path.find("%2f") != std::string::npos ||
      path.find("%2F") != std::string::npos) {
    return true;
  }
  if (path.find("%5c") != std::string::npos ||
      path.find("%5C") != std::string::npos) {
    return true;
  }
  return false;
}

std::string GetDirectoryPath(const GURL& url) {
  std::string path = url.path();
  std::string directory_path =
      path.substr(0, path.size() - url.ExtractFileName().size());
  DCHECK(EndsWith(directory_path, "/", true));
  return directory_path;
}

}  // namespace

// static
bool ServiceWorkerUtils::ScopeMatches(const GURL& scope, const GURL& url) {
  DCHECK(!scope.has_ref());
  DCHECK(!url.has_ref());
  return StartsWithASCII(url.spec(), scope.spec(), true);
}

// static
bool ServiceWorkerUtils::IsPathRestrictionSatisfied(
    const GURL& scope,
    const GURL& script_url,
    std::string* error_message) {
  DCHECK(scope.is_valid());
  DCHECK(!scope.has_ref());
  DCHECK(script_url.is_valid());
  DCHECK(!script_url.has_ref());
  DCHECK(error_message);

  if (ContainsDisallowedCharacter(scope) ||
      ContainsDisallowedCharacter(script_url)) {
    *error_message = kDisallowedCharacterErrorMessage;
    return false;
  }

  // |scope|'s path should be under the |script_url|'s directory.
  if (!StartsWithASCII(scope.path(), GetDirectoryPath(script_url), true)) {
    *error_message = kPathRestrictionErrorMessage;
    return false;
  }
  return true;
}

bool LongestScopeMatcher::MatchLongest(const GURL& scope) {
  if (!ServiceWorkerUtils::ScopeMatches(scope, url_))
    return false;
  if (match_.is_empty() || match_.spec().size() < scope.spec().size()) {
    match_ = scope;
    return true;
  }
  return false;
}

}  // namespace content
