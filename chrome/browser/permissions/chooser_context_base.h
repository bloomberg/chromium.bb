// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_CHOOSER_CONTEXT_BASE_H_
#define CHROME_BROWSER_PERMISSIONS_CHOOSER_CONTEXT_BASE_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
class DictionaryValue;
class Value;
}

class GURL;
class HostContentSettingsMap;
class Profile;

// This is the base class for services that manage any type of permission that
// is granted through a chooser-style UI instead of a simple allow/deny prompt.
// Subclasses must define the structure of the objects that are stored.
class ChooserContextBase : public KeyedService {
 public:
  ChooserContextBase(Profile* profile,
                     ContentSettingsType data_content_settings_type);
  ~ChooserContextBase() override;

  // Returns the list of objects that |requesting_origin| has been granted
  // permission to access when embedded within |embedding_origin|.
  ScopedVector<base::DictionaryValue> GetGrantedObjects(
      const GURL& requesting_origin,
      const GURL& embedding_origin);

  // Grants |requesting_origin| access to |object| when embedded within
  // |embedding_origin| by writing it into |host_content_settings_map_|.
  void GrantObjectPermission(const GURL& requesting_origin,
                             const GURL& embedding_origin,
                             scoped_ptr<base::DictionaryValue> object);

  // Revokes |requesting_origin|'s permission to access |object| when embedded
  // within |embedding_origin|.
  void RevokeObjectPermission(const GURL& requesting_origin,
                              const GURL& embedding_origin,
                              const base::DictionaryValue& object);

  // Validates the structure of an object read from
  // |host_content_settings_map_|.
  virtual bool IsValidObject(const base::DictionaryValue& object) = 0;

 private:
  scoped_ptr<base::DictionaryValue> GetWebsiteSetting(
      const GURL& requesting_origin,
      const GURL& embedding_origin);
  void SetWebsiteSetting(const GURL& requesting_origin,
                         const GURL& embedding_origin,
                         scoped_ptr<base::Value> value);

  HostContentSettingsMap* const host_content_settings_map_;
  const ContentSettingsType data_content_settings_type_;
};

#endif  // CHROME_BROWSER_PERMISSIONS_CHOOSER_CONTEXT_BASE_H_
