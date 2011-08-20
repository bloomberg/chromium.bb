// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_settings_noop_storage.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/task.h"

static void CallOnSuccess(
    ExtensionSettingsStorage::Callback* callback, DictionaryValue* settings) {
  callback->OnSuccess(settings);
  delete callback;
}

static void Succeed(
    ExtensionSettingsStorage::Callback* callback, DictionaryValue* settings) {
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&CallOnSuccess, callback, settings));
}

void ExtensionSettingsNoopStorage::DeleteSoon() {
  delete this;
}

void ExtensionSettingsNoopStorage::Get(
    const std::string& key, ExtensionSettingsStorage::Callback* callback) {
  Succeed(callback, new DictionaryValue());
}

void ExtensionSettingsNoopStorage::Get(
    const ListValue& keys, ExtensionSettingsStorage::Callback* callback) {
  Succeed(callback, new DictionaryValue());
}

void ExtensionSettingsNoopStorage::Get(
    ExtensionSettingsStorage::Callback* callback) {
  Succeed(callback, new DictionaryValue());
}

void ExtensionSettingsNoopStorage::Set(
    const std::string& key,
    const Value& value,
    ExtensionSettingsStorage::Callback* callback) {
  DictionaryValue* settings = new DictionaryValue();
  settings->SetWithoutPathExpansion(key, value.DeepCopy());
  Succeed(callback, settings);
}

void ExtensionSettingsNoopStorage::Set(
    const DictionaryValue& values,
    ExtensionSettingsStorage::Callback* callback) {
  Succeed(callback, values.DeepCopy());
}

void ExtensionSettingsNoopStorage::Remove(
    const std::string& key, ExtensionSettingsStorage::Callback *callback) {
  Succeed(callback, NULL);
}

void ExtensionSettingsNoopStorage::Remove(
    const ListValue& keys, ExtensionSettingsStorage::Callback *callback) {
  Succeed(callback, NULL);
}

void ExtensionSettingsNoopStorage::Clear(
    ExtensionSettingsStorage::Callback* callback) {
  Succeed(callback, NULL);
}
