// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/web_intents_handler.h"

#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "extensions/common/error_utils.h"
#include "webkit/glue/web_intent_service_data.h"

namespace extensions {

namespace keys = extension_manifest_keys;
namespace values = extension_manifest_values;
namespace errors = extension_manifest_errors;

namespace {

// Parses a single web intent action in the manifest.
bool LoadWebIntentsAction(const std::string& action_name,
                          const DictionaryValue& intent_service,
                          Extension* extension,
                          string16* error,
                          WebIntentServiceDataList* intents_services) {
  DCHECK(error);
  webkit_glue::WebIntentServiceData service;
  std::string value;

  service.action = UTF8ToUTF16(action_name);

  const ListValue* mime_types = NULL;
  if (!intent_service.HasKey(keys::kIntentType) ||
      !intent_service.GetList(keys::kIntentType, &mime_types) ||
      mime_types->GetSize() == 0) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidIntentType, action_name);
    return false;
  }

  std::string href;
  if (intent_service.HasKey(keys::kIntentPath)) {
    if (!intent_service.GetString(keys::kIntentPath, &href)) {
      *error = ASCIIToUTF16(errors::kInvalidIntentHref);
      return false;
    }
  }

  if (intent_service.HasKey(keys::kIntentHref)) {
    if (!href.empty()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidIntentHrefOldAndNewKey, action_name,
          keys::kIntentPath, keys::kIntentHref);
       return false;
    }
    if (!intent_service.GetString(keys::kIntentHref, &href)) {
      *error = ASCIIToUTF16(errors::kInvalidIntentHref);
      return false;
    }
  }

  // For packaged/hosted apps, empty href implies the respective launch URLs.
  if (href.empty()) {
    if (extension->is_hosted_app()) {
      href = extension->launch_web_url();
    } else if (extension->is_legacy_packaged_app()) {
      href = extension->launch_local_path();
    }
  }

  // If there still is not an  href, the manifest is malformed, unless this is a
  // platform app in which case the href should not be present.
  if (href.empty() && !extension->is_platform_app()) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidIntentHrefEmpty, action_name);
    return false;
  } else if (!href.empty() && extension->is_platform_app()) {
    *error = ErrorUtils::FormatErrorMessageUTF16(
        errors::kInvalidIntentHrefInPlatformApp, action_name);
    return false;
  }

  GURL service_url(href);
  if (extension->is_hosted_app()) {
    // Hosted apps require an absolute URL for intents.
    if (!service_url.is_valid() ||
        !(extension->web_extent().MatchesURL(service_url))) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidIntentPageInHostedApp, action_name);
      return false;
    }
    service.service_url = service_url;
  } else if (extension->is_platform_app()) {
    service.service_url = extension->GetBackgroundURL();
  } else {
    // We do not allow absolute intent URLs in non-hosted apps.
    if (service_url.is_valid()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kCannotAccessPage, href);
      return false;
    }
    service.service_url = extension->GetResourceURL(href);
  }

  if (intent_service.HasKey(keys::kIntentTitle) &&
      !intent_service.GetString(keys::kIntentTitle, &service.title)) {
    *error = ASCIIToUTF16(errors::kInvalidIntentTitle);
    return false;
  }

  if (intent_service.HasKey(keys::kIntentDisposition)) {
    if (extension->is_platform_app()) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidIntentDispositionInPlatformApp, action_name);
      return false;
    }
    if (!intent_service.GetString(keys::kIntentDisposition, &value) ||
        (value != values::kIntentDispositionWindow &&
         value != values::kIntentDispositionInline)) {
      *error = ASCIIToUTF16(errors::kInvalidIntentDisposition);
      return false;
    }
    if (value == values::kIntentDispositionInline) {
      service.disposition =
          webkit_glue::WebIntentServiceData::DISPOSITION_INLINE;
    } else {
      service.disposition =
          webkit_glue::WebIntentServiceData::DISPOSITION_WINDOW;
    }
  }

  for (size_t i = 0; i < mime_types->GetSize(); ++i) {
    if (!mime_types->GetString(i, &service.type)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidIntentTypeElement, action_name,
          std::string(base::IntToString(i)));
      return false;
    }
    intents_services->push_back(service);
  }
  return true;
}

}  // namespace

WebIntentsInfo::WebIntentsInfo() {}
WebIntentsInfo::~WebIntentsInfo() {}

static base::LazyInstance<WebIntentServiceDataList> g_empty_intents_data =
    LAZY_INSTANCE_INITIALIZER;

// static
const WebIntentServiceDataList& WebIntentsInfo::GetIntentsServices(
    const Extension* extension) {
  WebIntentsInfo* info = static_cast<WebIntentsInfo*>(
      extension->GetManifestData(keys::kIntents));
  return info ? info->intents_services_ : g_empty_intents_data.Get();
}

WebIntentsHandler::WebIntentsHandler() {
}

WebIntentsHandler::~WebIntentsHandler() {
}

bool WebIntentsHandler::Parse(const base::Value* value,
                              Extension* extension,
                              string16* error) {
  scoped_ptr<WebIntentsInfo> info(new WebIntentsInfo);
  const DictionaryValue* all_services = NULL;
  if (!value->GetAsDictionary(&all_services)) {
    *error = ASCIIToUTF16(errors::kInvalidIntents);
    return false;
  }

  for (DictionaryValue::key_iterator iter(all_services->begin_keys());
       iter != all_services->end_keys(); ++iter) {
    // Any entry in the intents dictionary can either have a list of
    // dictionaries, or just a single dictionary attached to that. Try
    // lists first, fall back to single dictionary.
    const ListValue* service_list = NULL;
    const DictionaryValue* one_service = NULL;
    if (all_services->GetListWithoutPathExpansion(*iter, &service_list)) {
      for (size_t i = 0; i < service_list->GetSize(); ++i) {
        if (!service_list->GetDictionary(i, &one_service)) {
            *error = ASCIIToUTF16(errors::kInvalidIntent);
            return false;
        }
        if (!LoadWebIntentsAction(*iter, *one_service, extension,
                                  error, &info->intents_services_))
          return false;
      }
    } else {
      if (!all_services->GetDictionaryWithoutPathExpansion(*iter,
                                                           &one_service)) {
        *error = ASCIIToUTF16(errors::kInvalidIntent);
        return false;
      }
      if (!LoadWebIntentsAction(*iter, *one_service, extension,
                                error, &info->intents_services_))
        return false;
    }
  }
  extension->SetManifestData(keys::kIntents, info.release());
  return true;
}

}  // namespace extensions
