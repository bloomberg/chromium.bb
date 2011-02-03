// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep this file in sync with the .proto files in this directory.

#include "chrome/browser/sync/protocol/proto_value_conversions.h"

#include "base/base64.h"
#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/sync/protocol/app_specifics.pb.h"
#include "chrome/browser/sync/protocol/autofill_specifics.pb.h"
#include "chrome/browser/sync/protocol/bookmark_specifics.pb.h"
#include "chrome/browser/sync/protocol/encryption.pb.h"
#include "chrome/browser/sync/protocol/extension_specifics.pb.h"
#include "chrome/browser/sync/protocol/nigori_specifics.pb.h"
#include "chrome/browser/sync/protocol/password_specifics.pb.h"
#include "chrome/browser/sync/protocol/preference_specifics.pb.h"
#include "chrome/browser/sync/protocol/proto_enum_conversions.h"
#include "chrome/browser/sync/protocol/session_specifics.pb.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/protocol/theme_specifics.pb.h"
#include "chrome/browser/sync/protocol/typed_url_specifics.pb.h"

namespace browser_sync {

namespace {

// Basic Type -> Value functions.

StringValue* MakeInt64Value(int64 x) {
  return Value::CreateStringValue(base::Int64ToString(x));
}

// TODO(akalin): Perhaps make JSONWriter support BinaryValue and use
// that instead of a StringValue.
StringValue* MakeBytesValue(const std::string& bytes) {
  std::string bytes_base64;
  if (!base::Base64Encode(bytes, &bytes_base64)) {
    NOTREACHED();
  }
  return Value::CreateStringValue(bytes_base64);
}

// T is the enum type.
template <class T>
StringValue* MakeEnumValue(T t, const char* (*converter_fn)(T)) {
  return Value::CreateStringValue(converter_fn(t));
}

// T is the field type, F is either RepeatedField or RepeatedPtrField,
// and V is a subclass of Value.
template <class T, class F, class V>
ListValue* MakeRepeatedValue(const F& fields, V* (*converter_fn)(T)) {
  ListValue* list = new ListValue();
  for (typename F::const_iterator it = fields.begin(); it != fields.end();
       ++it) {
    list->Append(converter_fn(*it));
  }
  return list;
}

}  // namespace

// Helper macros to reduce the amount of boilerplate.

#define SET(field, fn) value->Set(#field, fn(proto.field()))
#define SET_REP(field, fn) \
  value->Set(#field, MakeRepeatedValue(proto.field(), fn))
#define SET_ENUM(field, fn) \
  value->Set(#field, MakeEnumValue(proto.field(), fn))

#define SET_BOOL(field) SET(field, Value::CreateBooleanValue)
#define SET_BYTES(field) SET(field, MakeBytesValue)
#define SET_INT32(field) SET(field, MakeInt64Value)
#define SET_INT32_REP(field) SET_REP(field, MakeInt64Value)
#define SET_INT64(field) SET(field, MakeInt64Value)
#define SET_INT64_REP(field) SET_REP(field, MakeInt64Value)
#define SET_STR(field) SET(field, Value::CreateStringValue)

#define SET_EXTENSION(ns, field, fn)                                    \
  do {                                                                  \
    if (specifics.HasExtension(ns::field)) {                            \
      value->Set(#field, fn(specifics.GetExtension(ns::field)));        \
    }                                                                   \
  } while (0)

// If you add another macro, don't forget to add an #undef at the end
// of this file, too.

DictionaryValue* EncryptedDataToValue(const sync_pb::EncryptedData& proto) {
  DictionaryValue* value = new DictionaryValue();
  SET_STR(key_name);
  // TODO(akalin): Shouldn't blob be of type bytes instead of string?
  SET_BYTES(blob);
  return value;
}

DictionaryValue* AppSpecificsToValue(
    const sync_pb::AppSpecifics& proto) {
  DictionaryValue* value = new DictionaryValue();
  SET(extension, ExtensionSpecificsToValue);
  return value;
}

DictionaryValue* AutofillSpecificsToValue(
    const sync_pb::AutofillSpecifics& proto) {
  DictionaryValue* value = new DictionaryValue();
  SET_STR(name);
  SET_STR(value);
  SET_INT64_REP(usage_timestamp);
  SET(profile, AutofillProfileSpecificsToValue);
  SET_BYTES(encrypted_credit_card);
  SET(credit_card, AutofillCreditCardSpecificsToValue);
  return value;
}

DictionaryValue* AutofillCreditCardSpecificsToValue(
    const sync_pb::AutofillCreditCardSpecifics& proto) {
  DictionaryValue* value = new DictionaryValue();
  SET_STR(label);
  SET_STR(name_on_card);
  SET_STR(type);
  SET_STR(card_number);
  SET_STR(expiration_month);
  SET_STR(expiration_year);
  SET_STR(verification_code);
  SET_STR(billing_address);
  SET_STR(shipping_address);
  return value;
}

DictionaryValue* AutofillProfileSpecificsToValue(
    const sync_pb::AutofillProfileSpecifics& proto) {
  DictionaryValue* value = new DictionaryValue();
  SET_STR(label);
  SET_STR(guid);

  SET_STR(name_first);
  SET_STR(name_middle);
  SET_STR(name_last);
  SET_STR(email_address);
  SET_STR(company_name);

  SET_STR(address_home_line1);
  SET_STR(address_home_line2);
  SET_STR(address_home_city);
  SET_STR(address_home_state);
  SET_STR(address_home_zip);
  SET_STR(address_home_country);

  SET_STR(phone_home_whole_number);
  SET_STR(phone_fax_whole_number);
  return value;
}

DictionaryValue* BookmarkSpecificsToValue(
    const sync_pb::BookmarkSpecifics& proto) {
  DictionaryValue* value = new DictionaryValue();
  SET_STR(url);
  SET_BYTES(favicon);
  return value;
}

DictionaryValue* ExtensionSpecificsToValue(
    const sync_pb::ExtensionSpecifics& proto) {
  DictionaryValue* value = new DictionaryValue();
  SET_STR(id);
  SET_STR(version);
  SET_STR(update_url);
  SET_BOOL(enabled);
  SET_BOOL(incognito_enabled);
  SET_STR(name);
  return value;
}

DictionaryValue* NigoriSpecificsToValue(
    const sync_pb::NigoriSpecifics& proto) {
  DictionaryValue* value = new DictionaryValue();
  SET(encrypted, EncryptedDataToValue);
  SET_BOOL(using_explicit_passphrase);
  return value;
}

DictionaryValue* PasswordSpecificsToValue(
    const sync_pb::PasswordSpecifics& proto) {
  DictionaryValue* value = new DictionaryValue();
  SET(encrypted, EncryptedDataToValue);
  return value;
}

DictionaryValue* PreferenceSpecificsToValue(
    const sync_pb::PreferenceSpecifics& proto) {
  DictionaryValue* value = new DictionaryValue();
  SET_STR(name);
  SET_STR(value);
  return value;
}

DictionaryValue* SessionHeaderToValue(
    const sync_pb::SessionHeader& proto) {
  DictionaryValue* value = new DictionaryValue();
  SET_REP(window, SessionWindowToValue);
  return value;
}

DictionaryValue* SessionTabToValue(
    const sync_pb::SessionTab& proto) {
  DictionaryValue* value = new DictionaryValue();
  SET_INT32(tab_id);
  SET_INT32(window_id);
  SET_INT32(tab_visual_index);
  SET_INT32(current_navigation_index);
  SET_BOOL(pinned);
  SET_STR(extension_app_id);
  SET_REP(navigation, TabNavigationToValue);
  return value;
}

DictionaryValue* SessionWindowToValue(
    const sync_pb::SessionWindow& proto) {
  DictionaryValue* value = new DictionaryValue();
  SET_INT32(window_id);
  SET_INT32(selected_tab_index);
  SET_INT32_REP(tab);
  SET_ENUM(browser_type, GetBrowserTypeString);
  return value;
}

DictionaryValue* TabNavigationToValue(
    const sync_pb::TabNavigation& proto) {
  DictionaryValue* value = new DictionaryValue();
  SET_INT32(index);
  SET_STR(virtual_url);
  SET_STR(referrer);
  SET_STR(title);
  SET_STR(state);
  SET_ENUM(page_transition, GetPageTransitionString);
  SET_ENUM(navigation_qualifier, GetPageTransitionQualifierString);
  return value;
}

DictionaryValue* SessionSpecificsToValue(
    const sync_pb::SessionSpecifics& proto) {
  DictionaryValue* value = new DictionaryValue();
  SET_STR(session_tag);
  SET(header, SessionHeaderToValue);
  SET(tab, SessionTabToValue);
  return value;
}

DictionaryValue* ThemeSpecificsToValue(
    const sync_pb::ThemeSpecifics& proto) {
  DictionaryValue* value = new DictionaryValue();
  SET_BOOL(use_custom_theme);
  SET_BOOL(use_system_theme_by_default);
  SET_STR(custom_theme_name);
  SET_STR(custom_theme_id);
  SET_STR(custom_theme_update_url);
  return value;
}

DictionaryValue* TypedUrlSpecificsToValue(
    const sync_pb::TypedUrlSpecifics& proto) {
  DictionaryValue* value = new DictionaryValue();
  SET_STR(url);
  SET_STR(title);
  SET_INT32(typed_count);
  SET_BOOL(hidden);
  SET_INT64_REP(visit);
  return value;
}

DictionaryValue* EntitySpecificsToValue(
    const sync_pb::EntitySpecifics& specifics) {
  DictionaryValue* value = new DictionaryValue();
  SET_EXTENSION(sync_pb, app, AppSpecificsToValue);
  SET_EXTENSION(sync_pb, autofill, AutofillSpecificsToValue);
  SET_EXTENSION(sync_pb, autofill_profile, AutofillProfileSpecificsToValue);
  SET_EXTENSION(sync_pb, bookmark, BookmarkSpecificsToValue);
  SET_EXTENSION(sync_pb, extension, ExtensionSpecificsToValue);
  SET_EXTENSION(sync_pb, nigori, NigoriSpecificsToValue);
  SET_EXTENSION(sync_pb, password, PasswordSpecificsToValue);
  SET_EXTENSION(sync_pb, preference, PreferenceSpecificsToValue);
  SET_EXTENSION(sync_pb, session, SessionSpecificsToValue);
  SET_EXTENSION(sync_pb, theme, ThemeSpecificsToValue);
  SET_EXTENSION(sync_pb, typed_url, TypedUrlSpecificsToValue);
  return value;
}

#undef SET
#undef SET_REP

#undef SET_BOOL
#undef SET_BYTES
#undef SET_INT32
#undef SET_INT64
#undef SET_INT64_REP
#undef SET_STR

#undef SET_EXTENSION

}  // namespace browser_sync
