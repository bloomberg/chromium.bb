// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PROTOCOL_PROTO_ENUM_CONVERSIONS_H_
#define COMPONENTS_SYNC_PROTOCOL_PROTO_ENUM_CONVERSIONS_H_

// Keep this file in sync with the .proto files in this directory.

#include "components/sync/protocol/app_list_specifics.pb.h"
#include "components/sync/protocol/app_specifics.pb.h"
#include "components/sync/protocol/client_debug_info.pb.h"
#include "components/sync/protocol/reading_list_specifics.pb.h"
#include "components/sync/protocol/session_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"

// Utility functions to get the string equivalent for some sync proto
// enums.

namespace syncer {

// The returned strings (which don't have to be freed) are in ASCII.
// The result of passing in an invalid enum value is undefined.

const char* GetAppListItemTypeString(
    sync_pb::AppListSpecifics::AppListItemType item_type);

const char* GetBrowserTypeString(
    sync_pb::SessionWindow::BrowserType browser_type);

const char* GetPageTransitionString(
    sync_pb::SyncEnums::PageTransition page_transition);

const char* GetPageTransitionRedirectTypeString(
    sync_pb::SyncEnums::PageTransitionRedirectType redirect_type);

const char* GetWifiCredentialSecurityClassString(
    sync_pb::WifiCredentialSpecifics::SecurityClass security_class);

const char* GetUpdatesSourceString(
    sync_pb::GetUpdatesCallerInfo::GetUpdatesSource updates_source);

const char* GetUpdatesOriginString(sync_pb::SyncEnums::GetUpdatesOrigin origin);

const char* GetReadingListEntryStatusString(
    sync_pb::ReadingListSpecifics::ReadingListEntryStatus status);

const char* GetResponseTypeString(
    sync_pb::CommitResponse::ResponseType response_type);

const char* GetErrorTypeString(sync_pb::SyncEnums::ErrorType error_type);

const char* GetActionString(sync_pb::SyncEnums::Action action);

const char* GetLaunchTypeString(sync_pb::AppSpecifics::LaunchType launch_type);

const char* GetWalletInfoTypeString(
    sync_pb::AutofillWalletSpecifics::WalletInfoType wallet_info_type);

const char* GetWalletMetadataTypeString(
    sync_pb::WalletMetadataSpecifics::Type wallet_metadata_type);

const char* GetWalletCardStatusString(
    sync_pb::WalletMaskedCreditCard::WalletCardStatus wallet_card_status);

const char* GetWalletCardTypeString(
    sync_pb::WalletMaskedCreditCard::WalletCardType wallet_card_type);

const char* GetDeviceTypeString(sync_pb::SyncEnums::DeviceType device_type);

const char* GetFaviconTypeString(sync_pb::SessionTab::FaviconType favicon_type);

const char* PassphraseTypeString(sync_pb::NigoriSpecifics::PassphraseType type);

const char* SingletonDebugEventTypeString(
    sync_pb::SyncEnums::SingletonDebugEventType type);

const char* GetBlockedStateString(sync_pb::TabNavigation::BlockedState state);
const char* GetPasswordStateString(sync_pb::TabNavigation::PasswordState state);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_PROTOCOL_PROTO_ENUM_CONVERSIONS_H_
