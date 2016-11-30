// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/arc_app_shelf_id.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/crx_file/id_util.h"

namespace arc {

namespace {

// String representation of Intent starts with this prefix.
constexpr char kIntent[] = "#Intent";
// Prefix in intent that specifies Arc shelf group. S. means string type.
constexpr char kShelfGroupIntentPrefix[] = "S.org.chromium.arc.shelf_group_id=";
// Prefix to specify Arc shelf group.
constexpr char kShelfGroupPrefix[] = "shelf_group:";

}  // namespace

ArcAppShelfId::ArcAppShelfId(const std::string& shelf_group_id,
                             const std::string& app_id)
    : shelf_group_id_(shelf_group_id), app_id_(app_id) {
  DCHECK(crx_file::id_util::IdIsValid(app_id));
}

ArcAppShelfId::ArcAppShelfId(const ArcAppShelfId& other) = default;

ArcAppShelfId::~ArcAppShelfId() = default;

// static
ArcAppShelfId ArcAppShelfId::FromString(const std::string& id) {
  if (!base::StartsWith(id, kShelfGroupPrefix, base::CompareCase::SENSITIVE))
    return ArcAppShelfId(std::string(), id);

  const std::vector<std::string> parts = base::SplitString(
      id, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  DCHECK_EQ(3u, parts.size());

  return ArcAppShelfId(parts[1], parts[2]);
}

// static
ArcAppShelfId ArcAppShelfId::FromIntentAndAppId(const std::string& intent,
                                                const std::string& app_id) {
  if (intent.empty())
    return ArcAppShelfId(std::string(), app_id);

  const std::string prefix(kShelfGroupIntentPrefix);
  const std::vector<std::string> parts = base::SplitString(
      intent, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (parts.empty() || parts[0] != kIntent)
    return ArcAppShelfId(std::string(), app_id);

  for (const auto& part : parts) {
    if (base::StartsWith(part, prefix, base::CompareCase::SENSITIVE))
      return ArcAppShelfId(part.substr(prefix.length()), app_id);
  }

  return ArcAppShelfId(std::string(), app_id);
}

std::string ArcAppShelfId::ToString() const {
  if (!has_shelf_group_id())
    return app_id_;

  return base::StringPrintf("%s%s:%s", kShelfGroupPrefix,
                            shelf_group_id_.c_str(), app_id_.c_str());
}

bool ArcAppShelfId::operator<(const ArcAppShelfId& other) const {
  const int compare_group = shelf_group_id_.compare(other.shelf_group_id());
  if (compare_group == 0)
    return app_id_ < other.app_id();
  return compare_group < 0;
}

}  // namespace arc
