// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/state_store.h"

#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"

namespace {

std::string GetFullKey(const std::string& extension_id,
                       const std::string& key) {
  return extension_id + "." + key;
}

}  // namespace

namespace extensions {

StateStore::StateStore(Profile* profile, const FilePath& db_path)
    : store_(db_path) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_INSTALLED,
                 content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
                 content::Source<Profile>(profile));
}

StateStore::StateStore(Profile* profile, ValueStore* value_store)
    : store_(value_store) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_INSTALLED,
                 content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
                 content::Source<Profile>(profile));
}

StateStore::~StateStore() {
}

void StateStore::RegisterKey(const std::string& key) {
  registered_keys_.insert(key);
}

void StateStore::GetExtensionValue(const std::string& extension_id,
                                   const std::string& key,
                                   ReadCallback callback) {
  store_.Get(GetFullKey(extension_id, key), callback);
}

void StateStore::SetExtensionValue(
    const std::string& extension_id,
    const std::string& key,
    scoped_ptr<base::Value> value) {
  store_.Set(GetFullKey(extension_id, key), value.Pass());
}

void StateStore::Observe(int type,
                         const content::NotificationSource& source,
                         const content::NotificationDetails& details) {
  std::string extension_id;

  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_INSTALLED:
      extension_id = content::Details<const Extension>(details).ptr()->id();
      break;
    case chrome::NOTIFICATION_EXTENSION_UNINSTALLED:
      extension_id =
          *content::Details<const std::string>(details).ptr();
      break;
    default:
      NOTREACHED();
      return;
  }

  for (std::set<std::string>::iterator key = registered_keys_.begin();
       key != registered_keys_.end(); ++key) {
    store_.Remove(GetFullKey(extension_id, *key));
  }
}

}  // namespace extensions
