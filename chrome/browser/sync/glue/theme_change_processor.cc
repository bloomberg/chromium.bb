// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/theme_change_processor.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/theme_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "sync/internal_api/public/change_record.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/util/unrecoverable_error_handler.h"
#include "sync/internal_api/public/write_node.h"
#include "sync/internal_api/public/write_transaction.h"
#include "sync/protocol/theme_specifics.pb.h"

namespace browser_sync {

ThemeChangeProcessor::ThemeChangeProcessor(
    DataTypeErrorHandler* error_handler)
    : ChangeProcessor(error_handler),
      profile_(NULL) {
  DCHECK(error_handler);
}

ThemeChangeProcessor::~ThemeChangeProcessor() {}

void ThemeChangeProcessor::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(running());
  DCHECK(profile_);
  DCHECK(type == chrome::NOTIFICATION_BROWSER_THEME_CHANGED);

  csync::WriteTransaction trans(FROM_HERE, share_handle());
  csync::WriteNode node(&trans);
  if (node.InitByClientTagLookup(syncable::THEMES,
                                 kCurrentThemeClientTag) !=
          csync::BaseNode::INIT_OK) {
    std::string err = "Could not create node with client tag: ";
    error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
                                          err + kCurrentThemeClientTag);
    return;
  }

  sync_pb::ThemeSpecifics old_theme_specifics = node.GetThemeSpecifics();
  // Make sure to base new_theme_specifics on old_theme_specifics so
  // we preserve the state of use_system_theme_by_default.
  sync_pb::ThemeSpecifics new_theme_specifics = old_theme_specifics;
  GetThemeSpecificsFromCurrentTheme(profile_, &new_theme_specifics);
  // Do a write only if something actually changed so as to guard
  // against cycles.
  if (!AreThemeSpecificsEqual(old_theme_specifics, new_theme_specifics)) {
    node.SetThemeSpecifics(new_theme_specifics);
  }
  return;
}

void ThemeChangeProcessor::ApplyChangesFromSyncModel(
    const csync::BaseTransaction* trans,
    const csync::ImmutableChangeRecordList& changes) {
  if (!running()) {
    return;
  }
  // TODO(akalin): Normally, we should only have a single change and
  // it should be an update.  However, the syncapi may occasionally
  // generates multiple changes.  When we fix syncapi to not do that,
  // we can remove the extra logic below.  See:
  // http://code.google.com/p/chromium/issues/detail?id=41696 .
  if (changes.Get().empty()) {
    error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
        "Change list unexpectedly empty");
    return;
  }
  const size_t change_count = changes.Get().size();
  if (change_count > 1u) {
    LOG(WARNING) << change_count << " theme changes detected; "
                 << "only applying the last one";
  }
  const csync::ChangeRecord& change =
      changes.Get()[change_count - 1];
  if (change.action != csync::ChangeRecord::ACTION_UPDATE &&
      change.action != csync::ChangeRecord::ACTION_DELETE) {
    std::string err = "strange theme change.action " +
        base::IntToString(change.action);
    error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE, err);
    return;
  }
  sync_pb::ThemeSpecifics theme_specifics;
  // If the action is a delete, simply use the default values for
  // ThemeSpecifics, which would cause the default theme to be set.
  if (change.action != csync::ChangeRecord::ACTION_DELETE) {
    csync::ReadNode node(trans);
    if (node.InitByIdLookup(change.id) != csync::BaseNode::INIT_OK) {
      error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
          "Theme node lookup failed.");
      return;
    }
    DCHECK_EQ(node.GetModelType(), syncable::THEMES);
    DCHECK(profile_);
    theme_specifics = node.GetThemeSpecifics();
  }
  ScopedStopObserving<ThemeChangeProcessor> stop_observing(this);
  SetCurrentThemeFromThemeSpecificsIfNecessary(theme_specifics, profile_);
}

void ThemeChangeProcessor::StartImpl(Profile* profile) {
  DCHECK(profile);
  profile_ = profile;
  StartObserving();
}

void ThemeChangeProcessor::StopImpl() {
  StopObserving();
  profile_ = NULL;
}

void ThemeChangeProcessor::StartObserving() {
  DCHECK(profile_);
  DVLOG(1) << "Observing BROWSER_THEME_CHANGED";
  notification_registrar_.Add(
      this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
      content::Source<ThemeService>(
          ThemeServiceFactory::GetForProfile(profile_)));
}

void ThemeChangeProcessor::StopObserving() {
  DCHECK(profile_);
  DVLOG(1) << "Unobserving all notifications";
  notification_registrar_.RemoveAll();
}

}  // namespace browser_sync
