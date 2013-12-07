// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/app_shell_app_sorting.h"

#include "sync/api/string_ordinal.h"

namespace {

// Ordinals for a single app on a single page.
const char kFirstApp[] = "a";
const char kNextApp[] = "b";
const char kFirstPage[] = "a";

}  // namespace

namespace apps {

AppShellAppSorting::AppShellAppSorting() {
}

AppShellAppSorting::~AppShellAppSorting() {
}

void AppShellAppSorting::SetExtensionScopedPrefs(
    extensions::ExtensionScopedPrefs* prefs) {
}

void AppShellAppSorting::SetExtensionSyncService(
    ExtensionSyncService* extension_sync_service) {
}

void AppShellAppSorting::Initialize(
    const extensions::ExtensionIdList& extension_ids) {
}

void AppShellAppSorting::FixNTPOrdinalCollisions() {
}

void AppShellAppSorting::EnsureValidOrdinals(
    const std::string& extension_id,
    const syncer::StringOrdinal& suggested_page) {
}

void AppShellAppSorting::OnExtensionMoved(
    const std::string& moved_extension_id,
    const std::string& predecessor_extension_id,
    const std::string& successor_extension_id) {
}

syncer::StringOrdinal AppShellAppSorting::GetAppLaunchOrdinal(
    const std::string& extension_id) const {
  return syncer::StringOrdinal(kFirstApp);
}

void AppShellAppSorting::SetAppLaunchOrdinal(
    const std::string& extension_id,
    const syncer::StringOrdinal& new_app_launch_ordinal) {
}

syncer::StringOrdinal AppShellAppSorting::CreateFirstAppLaunchOrdinal(
    const syncer::StringOrdinal& page_ordinal) const {
  return syncer::StringOrdinal(kFirstApp);
}

syncer::StringOrdinal AppShellAppSorting::CreateNextAppLaunchOrdinal(
    const syncer::StringOrdinal& page_ordinal) const {
  return syncer::StringOrdinal(kNextApp);
}

syncer::StringOrdinal AppShellAppSorting::CreateFirstAppPageOrdinal() const {
  return syncer::StringOrdinal(kFirstPage);
}

syncer::StringOrdinal AppShellAppSorting::GetNaturalAppPageOrdinal() const {
  return syncer::StringOrdinal(kFirstPage);
}

syncer::StringOrdinal AppShellAppSorting::GetPageOrdinal(
    const std::string& extension_id) const {
  return syncer::StringOrdinal(kFirstPage);
}

void AppShellAppSorting::SetPageOrdinal(
    const std::string& extension_id,
    const syncer::StringOrdinal& new_page_ordinal) {
}

void AppShellAppSorting::ClearOrdinals(const std::string& extension_id) {
}

int AppShellAppSorting::PageStringOrdinalAsInteger(
    const syncer::StringOrdinal& page_ordinal) const {
  return 0;
}

syncer::StringOrdinal AppShellAppSorting::PageIntegerAsStringOrdinal(
    size_t page_index) {
  return syncer::StringOrdinal(kFirstPage);
}

void AppShellAppSorting::MarkExtensionAsHidden(
    const std::string& extension_id) {
}

}  // namespace apps
