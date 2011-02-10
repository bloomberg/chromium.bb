// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep this file in sync with the .proto files in this directory.

#ifndef CHROME_BROWSER_SYNC_PROTOCOL_PROTO_VALUE_CONVERSIONS_H_
#define CHROME_BROWSER_SYNC_PROTOCOL_PROTO_VALUE_CONVERSIONS_H_
#pragma once

class DictionaryValue;

namespace sync_pb {
class AppSpecifics;
class AutofillCreditCardSpecifics;
class AutofillProfileSpecifics;
class AutofillSpecifics;
class BookmarkSpecifics;
class EncryptedData;
class EntitySpecifics;
class ExtensionSpecifics;
class NigoriSpecifics;
class PasswordSpecifics;
class PasswordSpecificsData;
class PreferenceSpecifics;
class SessionHeader;
class SessionSpecifics;
class SessionTab;
class SessionWindow;
class TabNavigation;
class ThemeSpecifics;
class TypedUrlSpecifics;
}  // namespace sync_pb

// Utility functions to convert sync protocol buffers to dictionaries.
// Each protocol field is mapped to a key of the same name.  Repeated
// fields are mapped to array values and sub-messages are mapped to
// sub-dictionary values.
//
// TODO(akalin): Add has_* information.
//
// TODO(akalin): Improve enum support.

namespace browser_sync {

// Ownership of all returned DictionaryValues are transferred to the
// caller.

// TODO(akalin): Perhaps extend this to decrypt?
DictionaryValue* EncryptedDataToValue(
    const sync_pb::EncryptedData& encrypted_data);

// Sub-protocols of SessionSpecifics.

DictionaryValue* SessionHeaderToValue(
    const sync_pb::SessionHeader& session_header);

DictionaryValue* SessionTabToValue(
    const sync_pb::SessionTab& session_tab);

DictionaryValue* SessionWindowToValue(
    const sync_pb::SessionWindow& session_window);

DictionaryValue* TabNavigationToValue(
    const sync_pb::TabNavigation& tab_navigation);

// Sub-protocol of PasswordSpecifics.

DictionaryValue* PasswordSpecificsDataToValue(
    const sync_pb::PasswordSpecificsData& password_specifics_data);

// Main *SpecificsToValue functions.

DictionaryValue* AppSpecificsToValue(
    const sync_pb::AppSpecifics& app_specifics);

DictionaryValue* AutofillSpecificsToValue(
    const sync_pb::AutofillSpecifics& autofill_specifics);

DictionaryValue* AutofillCreditCardSpecificsToValue(
    const sync_pb::AutofillCreditCardSpecifics&
        autofill_credit_card_specifics);

DictionaryValue* AutofillProfileSpecificsToValue(
    const sync_pb::AutofillProfileSpecifics& autofill_profile_specifics);

DictionaryValue* BookmarkSpecificsToValue(
    const sync_pb::BookmarkSpecifics& bookmark_specifics);

DictionaryValue* ExtensionSpecificsToValue(
    const sync_pb::ExtensionSpecifics& extension_specifics);

DictionaryValue* NigoriSpecificsToValue(
    const sync_pb::NigoriSpecifics& nigori_specifics);

DictionaryValue* PasswordSpecificsToValue(
    const sync_pb::PasswordSpecifics& password_specifics);

DictionaryValue* PreferenceSpecificsToValue(
    const sync_pb::PreferenceSpecifics& password_specifics);

DictionaryValue* SessionSpecificsToValue(
    const sync_pb::SessionSpecifics& session_specifics);

DictionaryValue* ThemeSpecificsToValue(
    const sync_pb::ThemeSpecifics& theme_specifics);

DictionaryValue* TypedUrlSpecificsToValue(
    const sync_pb::TypedUrlSpecifics& typed_url_specifics);

// Any present extensions are mapped to sub-dictionary values with the
// key equal to the extension name.
DictionaryValue* EntitySpecificsToValue(
    const sync_pb::EntitySpecifics& entity_specifics);

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_PROTOCOL_PROTO_VALUE_CONVERSIONS_H_
