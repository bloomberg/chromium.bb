// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/browser/shell_app_sorting.h"

#include "sync/api/string_ordinal.h"

namespace {

// Ordinals for a single app on a single page.
const char kFirstApp[] = "a";
const char kNextApp[] = "b";
const char kFirstPage[] = "a";

}  // namespace

namespace apps {

ShellAppSorting::ShellAppSorting() {
}

ShellAppSorting::~ShellAppSorting() {
}

void ShellAppSorting::SetExtensionScopedPrefs(
    extensions::ExtensionScopedPrefs* prefs) {
}

void ShellAppSorting::SetExtensionSyncService(
    ExtensionSyncService* extension_sync_service) {
}

void ShellAppSorting::Initialize(
    const extensions::ExtensionIdList& extension_ids) {
}

void ShellAppSorting::FixNTPOrdinalCollisions() {
}

void ShellAppSorting::EnsureValidOrdinals(
    const std::string& extension_id,
    const syncer::StringOrdinal& suggested_page) {
}

void ShellAppSorting::OnExtensionMoved(
    const std::string& moved_extension_id,
    const std::string& predecessor_extension_id,
    const std::string& successor_extension_id) {
}

syncer::StringOrdinal ShellAppSorting::GetAppLaunchOrdinal(
    const std::string& extension_id) const {
  return syncer::StringOrdinal(kFirstApp);
}

void ShellAppSorting::SetAppLaunchOrdinal(
    const std::string& extension_id,
    const syncer::StringOrdinal& new_app_launch_ordinal) {
}

syncer::StringOrdinal ShellAppSorting::CreateFirstAppLaunchOrdinal(
    const syncer::StringOrdinal& page_ordinal) const {
  return syncer::StringOrdinal(kFirstApp);
}

syncer::StringOrdinal ShellAppSorting::CreateNextAppLaunchOrdinal(
    const syncer::StringOrdinal& page_ordinal) const {
  return syncer::StringOrdinal(kNextApp);
}

syncer::StringOrdinal ShellAppSorting::CreateFirstAppPageOrdinal() const {
  return syncer::StringOrdinal(kFirstPage);
}

syncer::StringOrdinal ShellAppSorting::GetNaturalAppPageOrdinal() const {
  return syncer::StringOrdinal(kFirstPage);
}

syncer::StringOrdinal ShellAppSorting::GetPageOrdinal(
    const std::string& extension_id) const {
  return syncer::StringOrdinal(kFirstPage);
}

void ShellAppSorting::SetPageOrdinal(
    const std::string& extension_id,
    const syncer::StringOrdinal& new_page_ordinal) {
}

void ShellAppSorting::ClearOrdinals(const std::string& extension_id) {
}

int ShellAppSorting::PageStringOrdinalAsInteger(
    const syncer::StringOrdinal& page_ordinal) const {
  return 0;
}

syncer::StringOrdinal ShellAppSorting::PageIntegerAsStringOrdinal(
    size_t page_index) {
  return syncer::StringOrdinal(kFirstPage);
}

void ShellAppSorting::SetExtensionVisible(const std::string& extension_id,
                                          bool visible) {
}

}  // namespace apps
