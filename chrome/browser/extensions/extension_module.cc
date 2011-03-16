// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_module.h"

#include <string>

#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"

ExtensionPrefs* SetUpdateUrlDataFunction::extension_prefs() {
  return profile()->GetExtensionService()->extension_prefs();
}

bool SetUpdateUrlDataFunction::RunImpl() {
  std::string data;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &data));

  extension_prefs()->SetUpdateUrlData(extension_id(), data);
  return true;
}

bool IsAllowedIncognitoAccessFunction::RunImpl() {
  ExtensionService* ext_service = profile()->GetExtensionService();
  const Extension* extension = GetExtension();

  result_.reset(Value::CreateBooleanValue(
        ext_service->IsIncognitoEnabled(extension)));
  return true;
}

bool IsAllowedFileSchemeAccessFunction::RunImpl() {
  ExtensionService* ext_service = profile()->GetExtensionService();
  const Extension* extension = GetExtension();

  result_.reset(Value::CreateBooleanValue(
        ext_service->AllowFileAccess(extension)));
  return true;
}
