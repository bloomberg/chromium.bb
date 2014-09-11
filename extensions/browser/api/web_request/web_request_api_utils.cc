// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_api_utils.h"

#include <algorithm>

#include "base/macros.h"

namespace extension_web_request_api_utils {

namespace {

static const char* kResourceTypeStrings[] = {
  "main_frame",
  "sub_frame",
  "stylesheet",
  "script",
  "image",
  "object",
  "xmlhttprequest",
  "other",
  "other",
};

static ResourceType kResourceTypeValues[] = {
  content::RESOURCE_TYPE_MAIN_FRAME,
  content::RESOURCE_TYPE_SUB_FRAME,
  content::RESOURCE_TYPE_STYLESHEET,
  content::RESOURCE_TYPE_SCRIPT,
  content::RESOURCE_TYPE_IMAGE,
  content::RESOURCE_TYPE_OBJECT,
  content::RESOURCE_TYPE_XHR,
  content::RESOURCE_TYPE_LAST_TYPE,  // represents "other"
  // TODO(jochen): We duplicate the last entry, so the array's size is not a
  // power of two. If it is, this triggers a bug in gcc 4.4 in Release builds
  // (http://gcc.gnu.org/bugzilla/show_bug.cgi?id=43949). Once we use a version
  // of gcc with this bug fixed, or the array is changed so this duplicate
  // entry is no longer required, this should be removed.
  content::RESOURCE_TYPE_LAST_TYPE,
};

}

#define ARRAYEND(array) (array + arraysize(array))

bool IsRelevantResourceType(ResourceType type) {
  ResourceType* iter =
      std::find(kResourceTypeValues, ARRAYEND(kResourceTypeValues), type);
  return iter != ARRAYEND(kResourceTypeValues);
}

const char* ResourceTypeToString(ResourceType type) {
  ResourceType* iter =
      std::find(kResourceTypeValues, ARRAYEND(kResourceTypeValues), type);
  if (iter == ARRAYEND(kResourceTypeValues))
    return "other";

  return kResourceTypeStrings[iter - kResourceTypeValues];
}

bool ParseResourceType(const std::string& type_str,
                       ResourceType* type) {
  const char** iter =
      std::find(kResourceTypeStrings, ARRAYEND(kResourceTypeStrings), type_str);
  if (iter == ARRAYEND(kResourceTypeStrings))
    return false;
  *type = kResourceTypeValues[iter - kResourceTypeStrings];
  return true;
}

}  // namespace extension_web_request_api_utils
