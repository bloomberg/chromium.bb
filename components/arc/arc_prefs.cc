// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_prefs.h"

#include <string>

#include "components/prefs/pref_registry_simple.h"

namespace arc {
namespace prefs {

// Stores the user id received from DM Server when enrolling a Play user on an
// Active Directory managed device. Used to report to DM Server that the account
// is still used.
const char kArcActiveDirectoryPlayUserId[] =
    "arc.active_directory_play_user_id";
// A preference to keep list of Android apps and their state.
const char kArcApps[] = "arc.apps";
// A preference to store backup and restore state for Android apps.
const char kArcBackupRestoreEnabled[] = "arc.backup_restore.enabled";
// A preference to indicate that Android's data directory should be removed.
const char kArcDataRemoveRequested[] = "arc.data.remove_requested";
// A preference representing whether a user has opted in to use Google Play
// Store on ARC.
// TODO(hidehiko): For historical reason, now the preference name does not
// directly reflect "Google Play Store". We should get and set the values via
// utility methods (IsArcPlayStoreEnabledForProfile() and
// SetArcPlayStoreEnabledForProfile()) in chrome/browser/chromeos/arc/arc_util.
const char kArcEnabled[] = "arc.enabled";
// A preference that indicated whether Android reported it's compliance status
// with provided policies. This is used only as a signal to start Android kiosk.
const char kArcPolicyComplianceReported[] = "arc.policy_compliance_reported";
// A preference that indicates that user accepted PlayStore terms.
const char kArcTermsAccepted[] = "arc.terms.accepted";
// A preference to keep user's consent to use location service.
const char kArcLocationServiceEnabled[] = "arc.location_service.enabled";
// A preference to keep list of Android packages and their infomation.
const char kArcPackages[] = "arc.packages";
// A preference that indicates that Play Auto Install flow was already started.
const char kArcPaiStarted[] = "arc.pai.started";
// A preference to keep deferred requests of setting notifications enabled flag.
const char kArcSetNotificationsEnabledDeferred[] =
    "arc.set_notifications_enabled_deferred";
// A preference that indicates status of Android sign-in.
const char kArcSignedIn[] = "arc.signedin";
// A preference that indicates an ARC comaptible filesystem was chosen for
// the user directory (i.e., the user finished required migration.)
extern const char kArcCompatibleFilesystemChosen[] =
    "arc.compatible_filesystem.chosen";
// A preference that indicates that user accepted Voice Interaction Value Prop.
const char kArcVoiceInteractionValuePropAccepted[] =
    "arc.voice_interaction_value_prop.accepted";
// Integer pref indicating the ecryptfs to ext4 migration strategy. One of
// options: forbidden = 0, migrate = 1, wipe = 2 or ask the user = 3.
const char kEcryptfsMigrationStrategy[] = "ecryptfs_migration_strategy";
// A preference that indicates whether the SMS Connect feature is enabled.
const char kSmsConnectEnabled[] = "multidevice.sms_connect_enabled";
// A preference that indicates the user has enabled voice interaction services.
const char kVoiceInteractionEnabled[] = "settings.voice_interaction.enabled";
// A preference that indicates the user has allowed voice interaction services
// to access the "context" (text and graphic content that is currently on
// screen).
const char kVoiceInteractionContextEnabled[] =
    "settings.voice_interaction.context.enabled";
// A preference indicating whether voice interaction settings have been read
// from ARC. This synchronization only happens when user goes through the flow
// to set up voice interaction.
const char kVoiceInteractionPrefSynced[] =
    "settings.voice_interaction.context.synced";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  // TODO(dspaid): Implement a mechanism to allow this to sync on first boot
  // only.

  // This is used to delete the Play user ID if ARC is disabled for an
  // Active Directory managed device.
  registry->RegisterStringPref(kArcActiveDirectoryPlayUserId, std::string());

  // Note that ArcBackupRestoreEnabled and ArcLocationServiceEnabled prefs have
  // to be off by default, until an explicit gesture from the user to enable
  // them is received. This is crucial in the cases when these prefs transition
  // from a previous managed state to the unmanaged.
  registry->RegisterBooleanPref(kArcBackupRestoreEnabled, false);
  registry->RegisterBooleanPref(kArcLocationServiceEnabled, false);

  // This is used to decide whether migration from ecryptfs to ext4 is allowed.
  registry->RegisterIntegerPref(prefs::kEcryptfsMigrationStrategy, 0);

  // Sorted in lexicographical order.
  registry->RegisterBooleanPref(kArcDataRemoveRequested, false);
  registry->RegisterBooleanPref(kArcEnabled, false);
  registry->RegisterBooleanPref(kArcPaiStarted, false);
  registry->RegisterBooleanPref(kArcPolicyComplianceReported, false);
  registry->RegisterBooleanPref(kArcSignedIn, false);
  registry->RegisterBooleanPref(kArcTermsAccepted, false);
  registry->RegisterBooleanPref(kArcVoiceInteractionValuePropAccepted, false);
  registry->RegisterBooleanPref(kSmsConnectEnabled, true);
  registry->RegisterBooleanPref(kVoiceInteractionContextEnabled, false);
  registry->RegisterBooleanPref(kVoiceInteractionEnabled, false);
  registry->RegisterBooleanPref(kVoiceInteractionPrefSynced, false);
}

}  // namespace prefs
}  // namespace arc
